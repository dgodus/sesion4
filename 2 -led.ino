#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include "ExampleFunctions.h"
#include <ArduinoJson.h>

#define WIFI_SSID     "tu wifi"
#define WIFI_PASSWORD " contraseña"

#define Web_API_KEY  "AIz"
#define DATABASE_URL "https://led-c.com"
#define USER_EMAIL   "gcorreo"
#define USER_PASS    "12contraseña56"

void processData(AsyncResult &aResult);

UserAuth user_auth(Web_API_KEY, USER_EMAIL, USER_PASS);
SSL_CLIENT ssl_client, stream_ssl_client;

FirebaseApp app;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client), streamClient(stream_ssl_client);
RealtimeDatabase Database;

unsigned long lastCheck = 0;
const unsigned long checkInterval = 10000;

String listenerPath = "board1/outputs/digital";

// Solo estos pines se pueden controlar
const int allowedPins[] = {12, 13, 14};
const int numPins = sizeof(allowedPins) / sizeof(allowedPins[0]);

bool isAllowedPin(int pin) {
  for (int i = 0; i < numPins; i++)
    if (allowedPins[i] == pin) return true;
  return false;
}

void setPin(int pin, bool state) {
  if (!isAllowedPin(pin)) {
    Serial.printf("Pin no permitido, ignorado: %d\n", pin);
    return;
  }
  digitalWrite(pin, state ? HIGH : LOW);
  Serial.printf("GPIO %d -> %s\n", pin, state ? "ON" : "OFF");
}

// Acepta 0/1, true/false, "on"/"off"
bool parseState(String s) {
  s.trim();
  s.toLowerCase();
  s.replace("\"", "");
  if (s == "true" || s == "on") return true;
  if (s == "false" || s == "off") return false;
  return s.toInt() != 0;
}

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
  else Serial.println("Sin WiFi, seguire reintentando");
}

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < numPins; i++) {
    pinMode(allowedPins[i], OUTPUT);
    digitalWrite(allowedPins[i], LOW);
  }

  initWiFi();

  ssl_client.setInsecure();
  stream_ssl_client.setInsecure();
  ssl_client.setConnectionTimeout(1000);
  ssl_client.setHandshakeTimeout(5);
  stream_ssl_client.setConnectionTimeout(1000);
  stream_ssl_client.setHandshakeTimeout(5);

  initializeApp(aClient, app, getAuth(user_auth), processData, "authTask");
  app.getApp<RealtimeDatabase>(Database);
  Database.url(DATABASE_URL);

  streamClient.setSSEFilters("get,put,patch,keep-alive,cancel,auth_revoked");
  Database.get(streamClient, listenerPath, processData, true, "streamTask");
}

void loop() {
  app.loop();

  unsigned long now = millis();
  if (now - lastCheck >= checkInterval) {
    lastCheck = now;
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi caido, reconectando...");
      WiFi.reconnect();
    } else {
      Serial.printf("Uptime %lu ms, ready: %d\n", now, app.ready());
    }
  }
}

void processData(AsyncResult &aResult) {
  if (!aResult.isResult()) return;

  if (aResult.isEvent())
    Firebase.printf("Event: %s, msg: %s, code: %d\n", aResult.uid().c_str(),
                    aResult.eventLog().message().c_str(), aResult.eventLog().code());

  if (aResult.isError())
    Firebase.printf("Error: %s, msg: %s, code: %d\n", aResult.uid().c_str(),
                    aResult.error().message().c_str(), aResult.error().code());

  if (!aResult.available()) return;

  RealtimeDatabaseResult &RTDB = aResult.to<RealtimeDatabaseResult>();
  if (!RTDB.isStream()) return;

  String path = RTDB.dataPath();
  String data = RTDB.to<String>();
  Serial.println("----------------------------");
  Firebase.printf("event: %s | path: %s | type: %d | data: %s\n",
                  RTDB.event().c_str(), path.c_str(), RTDB.type(), data.c_str());

  // Tipo 6 = JSON (llega al iniciar el stream o si escribes todo el nodo)
  if (RTDB.type() == 6) {
    #if ARDUINOJSON_VERSION_MAJOR >= 7
      JsonDocument doc;
    #else
      DynamicJsonDocument doc(1024);
    #endif
    DeserializationError err = deserializeJson(doc, data);
    if (err) {
      Serial.printf("JSON error: %s\n", err.c_str());
      return;
    }
    for (JsonPair kv : doc.as<JsonObject>()) {
      int pin = atoi(kv.key().c_str());
      setPin(pin, kv.value().as<bool>());
    }
    return;
  }

  // Tipo 1 = entero (tu caso 0/1), tipo 4 = booleano
  if (RTDB.type() == 1 || RTDB.type() == 4) {
    // path viene como "/12" o "/12/algo", tomamos el primer segmento
    String seg = path;
    if (seg.startsWith("/")) seg = seg.substring(1);
    int slash = seg.indexOf('/');
    if (slash > 0) seg = seg.substring(0, slash);

    int pin = seg.toInt();
    if (seg.length() == 0 || pin == 0) {
      Serial.println("Path sin pin valido, ignorado");
      return;
    }
    setPin(pin, parseState(data));
  }
}