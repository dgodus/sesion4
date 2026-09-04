#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 32
#define LED_PIN      13

const unsigned long INTERVALO_LED    = 500;  // parpadeo del LED
const unsigned long INTERVALO_SENSOR = 2000;  // lectura del sensor

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

unsigned long tLed = 0;
unsigned long tSensor = 0;
bool estadoLed = false;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== DS18B20 + LED parpadeando ===");

  pinMode(LED_PIN, OUTPUT);

  sensors.begin();
  sensors.setResolution(12);
  Serial.printf("Sensores detectados: %d\n", sensors.getDeviceCount());
}

void loop() {
  unsigned long ahora = millis();

  // --- Tarea 1: parpadear el LED cada 1 segundo ---
  if (ahora - tLed >= INTERVALO_LED) {
    tLed = ahora;
    estadoLed = !estadoLed;
    digitalWrite(LED_PIN, estadoLed);
  }

  // --- Tarea 2: leer el sensor cada 2 segundos ---
  if (ahora - tSensor >= INTERVALO_SENSOR) {
    tSensor = ahora;

    sensors.requestTemperatures();
    float t = sensors.getTempCByIndex(0);

    if (t == DEVICE_DISCONNECTED_C || t == -127.0) {
      Serial.println("Error de lectura (-127).");
    } else if (t == 85.0) {
      Serial.println("Lectura 85.0, el sensor no alcanzo a convertir.");
    } else {
      Serial.printf("Temperatura: %.2f C\n", t);
    }
  }
}