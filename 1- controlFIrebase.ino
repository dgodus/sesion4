#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include "ExampleFunctions.h"
#include <OneWire.h>
#include <DallasTemperature.h>

// ---------- Credenciales ----------
#define WIFI_SSID     "nombre wifi"
#define WIFI_PASSWORD "contraseña"

#define Web_API_KEY  "AIzaSGh2s"
#define DATABASE_URL "https://led-control-a0895-default-rtdb.firebaseio.com"
#define USER_EMAIL   "go.com"
#define USER_PASS    "12rrrr456"

// ---------- Umbrales (ajusta aqui) ----------
const float T_FRIO  = 10.0;   // debajo de esto: zona FRIO
const float T_CALOR = 20.0;   // encima de esto: zona CALOR
const float HIST    = 0.5;    // histeresis en grados

// ---------- Pines ----------
#define ONE_WIRE_BUS 32
const int LED_FRIO   = 12;
const int LED_NORMAL = 13;
const int LED_CALOR  = 14;

// ---------- Sensor ----------
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// ---------- Firebase ----------
void processData(AsyncResult &aResult);
UserAuth user_auth(Web_API_KEY, USER_EMAIL, USER_PASS);
SSL_CLIENT ssl_client;
FirebaseApp app;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);
RealtimeDatabase Database;

// ---------- Estado ----------
float tempActual = NAN;
int zonaActual = -1;              // 0 frio, 1 normal, 2 calor
const char *nombreZona[] = {"FRIO", "NORMAL", "CALOR"};

unsigned long tPeticion = 0;
bool esperandoLectura = false;
const unsigned long conversionMs = 800;

unsigned long ultimaLectura = 0;
const unsigned long intervaloLectura = 2000;

unsigned long ultimoEnvio = 0;
const unsigned long intervaloEnvio = 10000;

// ---------- Logica de zonas con histeresis ----------
int calcularZona(float t, int actual) {
  if (actual == 0) {                          // venia de FRIO
    if (t > T_CALOR + HIST) return 2;
    if (t > T_FRIO + HIST)  return 1;
    return 0;
  }
  if (actual == 2) {                          // venia de CALOR
    if (t < T_FRIO - HIST)  return 0;
    if (t < T_CALOR - HIST) return 1;
    return 2;
  }
  if (actual == 1) {                          // venia de NORMAL
    if (t < T_FRIO - HIST)  return 0;
    if (t > T_CALOR + HIST) return 2;
    return 1;
  }
  if (t < T_FRIO)  return 0;                  // primera vez
  if (t > T_CALOR) return 2;
  return 1;
}

void aplicarZona(int z) {
  digitalWrite(LED_FRIO,   z == 0 ? HIGH : LOW);
  digitalWrite(LED_NORMAL, z == 1 ? HIGH : LOW);
  digitalWrite(LED_CALOR,  z == 2 ? HIGH : LOW);
}

// ---------- WiFi ----------
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando WiFi ..");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
    Serial.print('.');
    delay(500);
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) Serial.println(WiFi.localIP());
  else Serial.println("Sin WiFi, reintentare en el loop");
}

// ---------- Envio a Firebase ----------
void enviarDatos() {
  if (!app.ready()) return;

  Database.set<number_t>(aClient, "/board1/sensor/temperatura",
                         number_t(tempActual, 2), processData, "setTemp");

  Database.set<String>(aClient, "/board1/sensor/estado",
                       String(nombreZona[zonaActual]), processData, "setEstado");

  Database.set<int>(aClient, "/board1/outputs/digital/12",
                    zonaActual == 0 ? 1 : 0, processData, "set12");
  Database.set<int>(aClient, "/board1/outputs/digital/13",
                    zonaActual == 1 ? 1 : 0, processData, "set13");
  Database.set<int>(aClient, "/board1/outputs/digital/14",
                    zonaActual == 2 ? 1 : 0, processData, "set14");

  Serial.printf(">> Enviado a Firebase: %.2f C, zona %s\n",
                tempActual, nombreZona[zonaActual]);
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_FRIO, OUTPUT);
  pinMode(LED_NORMAL, OUTPUT);
  pinMode(LED_CALOR, OUTPUT);
  aplicarZona(-1);

  sensors.begin();
  sensors.setResolution(12);
  sensors.setWaitForConversion(false);   // no bloquea la placa
  Serial.printf("Sensores detectados: %d\n", sensors.getDeviceCount());

  initWiFi();

  ssl_client.setInsecure();
  ssl_client.setConnectionTimeout(1000);
  ssl_client.setHandshakeTimeout(5);

  initializeApp(aClient, app, getAuth(user_auth), processData, "authTask");
  app.getApp<RealtimeDatabase>(Database);
  Database.url(DATABASE_URL);
}

void loop() {
  app.loop();

  unsigned long ahora = millis();

  // 1. Pedir conversion al sensor
  if (!esperandoLectura && ahora - ultimaLectura >= intervaloLectura) {
    sensors.requestTemperatures();
    tPeticion = ahora;
    esperandoLectura = true;
  }

  // 2. Recoger el resultado cuando ya paso el tiempo de conversion
  if (esperandoLectura && ahora - tPeticion >= conversionMs) {
    esperandoLectura = false;
    ultimaLectura = ahora;

    float t = sensors.getTempCByIndex(0);
    if (t == DEVICE_DISCONNECTED_C || t == -127.0 || t == 85.0) {
      Serial.printf("Lectura invalida: %.2f\n", t);
    } else {
      tempActual = t;
      int nuevaZona = calcularZona(t, zonaActual);

      if (nuevaZona != zonaActual) {
        zonaActual = nuevaZona;
        aplicarZona(zonaActual);
        Serial.printf("Cambio de zona -> %s\n", nombreZona[zonaActual]);
        enviarDatos();               // envia de inmediato al cambiar
        ultimoEnvio = ahora;
      } else {
        Serial.printf("Temp: %.2f C (%s)\n", t, nombreZona[zonaActual]);
      }
    }
  }

  // 3. Envio periodico
  if (ahora - ultimoEnvio >= intervaloEnvio) {
    ultimoEnvio = ahora;
    if (!isnan(tempActual) && zonaActual >= 0) enviarDatos();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi caido, reconectando...");
      WiFi.reconnect();
    }
  }
}

void processData(AsyncResult &aResult) {
  if (!aResult.isResult()) return;

  if (aResult.isError())
    Firebase.printf("Error [%s]: %s (code %d)\n", aResult.uid().c_str(),
                    aResult.error().message().c_str(), aResult.error().code());

  if (aResult.isEvent())
    Firebase.printf("Event [%s]: %s\n", aResult.uid().c_str(),
                    aResult.eventLog().message().c_str());
}