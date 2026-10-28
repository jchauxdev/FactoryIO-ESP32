#include <WiFi.h>
#include <ModbusIP_ESP8266.h>
#include "secrets.h" // Define WIFI_SSID y WIFI_PASSWORD (ver secrets.example.h)

// 1. Direcciones de Registros Modbus (Offset 0)
const int COIL_SENSOR_FIO  = 0; // Factory IO escribe aquí el estado del Sensor (Coil 0)
const int ISTS_MOTOR_FIO   = 0; // Factory IO lee de aquí para mover la Banda (Input 0)

// 2. Pines físicos del ESP32
const int PIN_LED_ESTADO   = 2; // LED integrado en la placa ESP32
const int PIN_BOTON_MARCHA = 4; // Pulsador entre GPIO 4 y GND

// Objeto Servidor Modbus TCP
ModbusIP mb;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED_ESTADO, OUTPUT);
  pinMode(PIN_BOTON_MARCHA, INPUT_PULLUP);

  // Conexión a la red Wi-Fi (credenciales leídas de secrets.h)
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n--- CONEXION WIFI EXITOSA ---");
  Serial.print("IP del ESP32 (Colocar en HOST de Factory IO): ");
  Serial.println(WiFi.localIP());

  // Inicializar ESP32 como Servidor (Slave) Modbus TCP en puerto 502
  mb.server();

  // Dar de alta registros digitales en memoria
  mb.addCoil(COIL_SENSOR_FIO, false); // Recibe sensor de Factory IO
  mb.addIsts(ISTS_MOTOR_FIO, false);  // Envía mando de actuador a Factory IO
}

void loop() {
  // Atender peticiones de Factory IO en cada iteración
  mb.task();

  // A) Leer estado del sensor virtual enviado por Factory IO (Coil 0)
  bool cajaDetectada = mb.Coil(COIL_SENSOR_FIO);

  // B) Leer pulsador físico conectado al GPIO 4 (lógica invertida por PULLUP)
  bool botonPresionado = (digitalRead(PIN_BOTON_MARCHA) == LOW);

  // Reflejar sensor virtual en el LED físico del ESP32
  digitalWrite(PIN_LED_ESTADO, cajaDetectada ? HIGH : LOW);

  // C) Lógica: Activar la banda si el sensor detecta caja O si se presiona el botón físico
  if (cajaDetectada || botonPresionado) {
    mb.Ists(ISTS_MOTOR_FIO, true);  // Enciende Output (Input 0) en Factory IO
  } else {
    mb.Ists(ISTS_MOTOR_FIO, false); // Apaga Output (Input 0) en Factory IO
  }

  delay(10); // Estabilidad del Watchdog y pila TCP/IP
}
