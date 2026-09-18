#include <WiFi.h>
#include <esp_wifi.h>
#include <WebServer.h>
#include <esp_now.h>

uint8_t BROADCAST[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
typedef struct { uint8_t estado; } Orden;
Orden o;

WebServer server(80);

void mandar(uint8_t v) {
  o.estado = v;
  esp_err_t r = esp_now_send(BROADCAST, (uint8_t*)&o, sizeof(o));

  uint8_t ch; wifi_second_chan_t s;
  esp_wifi_get_channel(&ch, &s);
  Serial.printf("Envio=%d  resultado=%d  canal=%d\n", v, r, ch);
}

void pagina() {
  String h = "<!DOCTYPE html><html><head><meta charset='utf-8'>";
  h += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  h += "<style>body{font-family:sans-serif;text-align:center;padding:40px;background:#0E2F52;color:#fff}";
  h += "h1{font-size:28px}a{display:block;margin:20px auto;padding:24px;width:220px;";
  h += "border-radius:12px;text-decoration:none;color:#fff;font-size:22px;font-weight:bold}";
  h += ".on{background:#2E7D5B}.off{background:#B23A2E}</style></head><body>";
  h += "<h1>Control de la clase</h1>";
  h += "<a class='on' href='/on'>ENCENDER</a>";
  h += "<a class='off' href='/off'>APAGAR</a>";
  h += "</body></html>";
  server.send(200, "text/html", h);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  WiFi.mode(WIFI_AP);
  WiFi.softAP("CLASE_IOT", "12345678", 1);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  Serial.print("Entra a http://");
  Serial.println(WiFi.softAPIP());

  if (esp_now_init() != ESP_OK) { Serial.println("ERROR ESP-NOW"); return; }

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BROADCAST, 6);
  peer.channel = 1;
  peer.ifidx   = WIFI_IF_AP;     // importante: el AP es la interfaz activa
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) Serial.println("ERROR al agregar peer");

  server.on("/", pagina);
  server.on("/on",  [](){ mandar(1); pagina(); });
  server.on("/off", [](){ mandar(0); pagina(); });
  server.begin();

  uint8_t ch; wifi_second_chan_t s;
  esp_wifi_get_channel(&ch, &s);
  Serial.printf("EMISOR listo en canal %d\n", ch);
}

void loop() {
  server.handleClient();
}