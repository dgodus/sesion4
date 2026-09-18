#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>

#define NOMBRE "QUEA_JUAN"        // cada alumno cambia esto

typedef struct { char nombre[16]; char texto[64]; } Mensaje;
uint8_t BROADCAST[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

void alLlegar(const esp_now_recv_info_t *info, const uint8_t *datos, int len) {
  if (len != sizeof(Mensaje)) return;
  Mensaje m; memcpy(&m, datos, sizeof(m));
  m.nombre[15] = 0; m.texto[63] = 0;
  Serial.printf(">> %s: %s\n", m.nombre, m.texto);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) { Serial.println("ERROR ESP-NOW"); return; }
  esp_now_register_recv_cb(alLlegar);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BROADCAST, 6);
  peer.channel = 1;
  peer.ifidx   = WIFI_IF_STA;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  Serial.print("MAC: "); Serial.println(WiFi.macAddress());
  Serial.println("Escribe tu mensaje y presiona Enter.");
}

void loop() {
  if (!Serial.available()) return;

  String t = Serial.readStringUntil('\n');
  t.trim();
  if (t.length() == 0) return;

  Mensaje m;
  strncpy(m.nombre, NOMBRE, 15); m.nombre[15] = 0;
  strncpy(m.texto, t.c_str(), 63); m.texto[63] = 0;

  esp_err_t r = esp_now_send(BROADCAST, (uint8_t*)&m, sizeof(m));
  Serial.printf("Enviado (%d): %s\n", r, t.c_str());
}