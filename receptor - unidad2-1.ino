#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>

#define LED 14
typedef struct { uint8_t estado; } Orden;
Orden o;

void alLlegar(const esp_now_recv_info_t *info, const uint8_t *datos, int len) {
  memcpy(&o, datos, sizeof(o));
  digitalWrite(LED, o.estado);
  Serial.printf("RECIBIDO: %d\n", o.estado);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) { Serial.println("ERROR ESP-NOW"); return; }
  esp_now_register_recv_cb(alLlegar);

  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());
}

void loop() {
  uint8_t ch; wifi_second_chan_t s;
  esp_wifi_get_channel(&ch, &s);
  Serial.printf("RECEPTOR escuchando en canal %d\n", ch);
  delay(3000);
}