#include <WiFi.h>
#include <ModbusIP_ESP8266.h>
#include "secrets.h" // Define WIFI_SSID y WIFI_PASSWORD (ver secrets.example.h)

// 1. Direcciones Modbus (Offset 0)
const int COIL_SENSOR_DIGITAL = 0; // FIO escribe sensor digital aquí (Coil 0)
const int ISTS_ACTUADOR_DIG   = 0; // FIO lee actuador digital de aquí (Input 0)
const int HREG_SENSOR_ANALOG  = 0; // FIO escribe sensor analógico 0-10V aquí (Holding Reg 0)
const int IREG_POT_ESP32      = 0; // FIO lee potenciómetro del ESP32 aquí (Input Reg 0)

// 2. Pines de Hardware en el ESP32
const int PIN_LED_DIGITAL = 2;  // LED indicador digital
const int PIN_BOTON_FISICO = 4; // Pulsador físico a GND
const int PIN_POT_ADC     = 34; // Pin ADC1_CH6 (Seguro para usar con Wi-Fi activo)
const int PIN_LED_PWM     = 5;  // Salida PWM para reflejar variable analógica de Factory IO

ModbusIP mb;
unsigned long lastPrintTime = 0;

void setup() {
  Serial.begin(115200);

  // Configurar pines digitales y analógicos
  pinMode(PIN_LED_DIGITAL, OUTPUT);
  pinMode(PIN_BOTON_FISICO, INPUT_PULLUP);
  pinMode(PIN_LED_PWM, OUTPUT);
  analogReadResolution(12); // Resolución ADC de 12 bits (0 a 4095)

  // Conectar a Wi-Fi (credenciales leídas de secrets.h)
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n--- CONEXION WIFI ESTABLECIDA ---");
  Serial.print("IP del ESP32 (Pegar en HOST de Factory IO): ");
  Serial.println(WiFi.localIP());

  // Iniciar servidor Modbus TCP
  mb.server();

  // Inicializar los 4 tipos de registros en la tabla Modbus del ESP32
  // Factory IO escribe/lee todos sus puntos en una sola petición: hay que definirlos todos
  // (16 digitales y 8 registros, el Count por defecto del driver), o el ESP32 rechaza la escritura.
  mb.addCoil(COIL_SENSOR_DIGITAL, false, 16);
  mb.addIsts(ISTS_ACTUADOR_DIG, false, 16);
  mb.addHreg(HREG_SENSOR_ANALOG, 0, 8); // Recibirá 0 - 1000 desde Factory IO (Scale = 100)
  mb.addIreg(IREG_POT_ESP32, 0, 8);     // Enviará 0 - 1000 hacia Factory IO (Scale = 100)
}

void loop() {
  // 1. Mantener activo el stack Modbus TCP
  mb.task();

  // ==========================================================
  // SECCIÓN A: SEÑALES DIGITALES (ON / OFF)
  // ==========================================================
  bool sensorVirtual = mb.Coil(COIL_SENSOR_DIGITAL);
  bool botonFisico   = (digitalRead(PIN_BOTON_FISICO) == LOW);

  digitalWrite(PIN_LED_DIGITAL, sensorVirtual ? HIGH : LOW);
  mb.Ists(ISTS_ACTUADOR_DIG, botonFisico); // El actuador solo depende del pulsador (independiente del sensor)

  // ==========================================================
  // SECCIÓN B: POTENCIÓMETRO ESP32 -> FACTORY I/O (0.0V a 10.0V)
  // ==========================================================
  // Promedio simple de 4 muestras para filtrar ruido eléctrico del ADC
  long sumaAdc = 0;
  for (int i = 0; i < 4; i++) {
    sumaAdc += analogRead(PIN_POT_ADC);
  }
  int adcPromedio = sumaAdc / 4; // Rango 0 - 4095

  // Escalar de [0 - 4095] a [0 - 1000] (equivale a 0.00V - 10.00V con Scale=100)
  uint16_t valorParaFactoryIO = map(adcPromedio, 0, 4095, 0, 1000);
  mb.Ireg(IREG_POT_ESP32, valorParaFactoryIO);

  // ==========================================================
  // SECCIÓN C: FACTORY I/O (0.0V a 10.0V) -> SALIDA PWM EN ESP32
  // ==========================================================
  // Leer Holding Register 0 escrito por Factory IO (Rango esperado: 0 - 1000)
  uint16_t valorDesdeFactoryIO = mb.Hreg(HREG_SENSOR_ANALOG);
  uint16_t valorRestringido    = constrain(valorDesdeFactoryIO, 0, 1000);

  // Convertir rango [0 - 1000] a ciclo útil PWM de 8 bits [0 - 255]
  int pwmSalida = map(valorRestringido, 0, 1000, 0, 255);
  analogWrite(PIN_LED_PWM, pwmSalida);

  // ==========================================================
  // SECCIÓN D: MONITOREO POR PUERTO SERIE CADA 500 ms
  // ==========================================================
  if (millis() - lastPrintTime >= 500) {
    lastPrintTime = millis();
    Serial.printf("[ESP32 -> FIO] ADC: %4d | Reg Ireg[0]: %4d (%.2f V)  ||  [FIO -> ESP32] Reg Hreg[0]: %4d (%.2f V) | PWM: %3d\n",
                  adcPromedio,
                  valorParaFactoryIO, valorParaFactoryIO / 100.0,
                  valorRestringido, valorRestringido / 100.0,
                  pwmSalida);
  }

  delay(10);
}
