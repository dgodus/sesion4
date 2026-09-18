#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <WebServer.h>

#define MAX_MSG 30

typedef struct { char nombre[16]; char texto[64]; } Mensaje;

struct Item { String mac, nombre, texto; int rssi; unsigned long t; };
Item lista[MAX_MSG];
int  total = 0;

uint8_t BROADCAST[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
WebServer server(80);

String limpiar(String s) {
  s.replace("\\", " "); s.replace("\"", "'");
  s.replace("\n", " "); s.replace("\r", " ");
  return s;
}

void agregar(String mac, String nom, String txt, int rssi) {
  if (total >= MAX_MSG) {
    for (int i = 0; i < MAX_MSG - 1; i++) lista[i] = lista[i+1];
    total = MAX_MSG - 1;
  }
  lista[total] = { mac, limpiar(nom), limpiar(txt), rssi, millis() };
  total++;
}

void alLlegar(const esp_now_recv_info_t *info, const uint8_t *datos, int len) {
  if (len != sizeof(Mensaje)) return;
  Mensaje m; memcpy(&m, datos, sizeof(m));
  m.nombre[15] = 0; m.texto[63] = 0;

  char buf[8];
  sprintf(buf, "%02X%02X", info->src_addr[4], info->src_addr[5]);

  int rssi = info->rx_ctrl->rssi;
  agregar(String(buf), String(m.nombre), String(m.texto), rssi);
  Serial.printf("[%d dBm] %s (%s): %s\n", rssi, m.nombre, buf, m.texto);
}

void enviarProfe(String txt) {
  Mensaje m;
  strncpy(m.nombre, "PROFE", 15); m.nombre[15] = 0;
  strncpy(m.texto, txt.c_str(), 63); m.texto[63] = 0;
  esp_now_send(BROADCAST, (uint8_t*)&m, sizeof(m));
  agregar("--", "PROFE", txt, 0);
}

void pagina() {
  String h = F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
  "<meta name='viewport' content='width=device-width,initial-scale=1'>"
  "<title>Chat ESP-NOW</title><style>"
  "*{box-sizing:border-box}body{margin:0;font-family:system-ui,sans-serif;"
  "background:#0E2F52;color:#fff;height:100vh;display:flex;flex-direction:column}"
  "header{padding:14px 18px;background:#07203B;font-weight:700;font-size:18px}"
  "#c{flex:1;overflow-y:auto;padding:14px}"
  ".m{background:#16304d;border-radius:10px;padding:10px 12px;margin-bottom:8px}"
  ".m.p{background:#C8791A}"
  ".n{font-weight:700;font-size:14px}.t{font-size:15px;margin-top:3px}"
  ".r{float:right;font-family:monospace;font-size:12px;opacity:.8}"
  "footer{display:flex;gap:8px;padding:12px;background:#07203B}"
  "input{flex:1;padding:12px;border:0;border-radius:8px;font-size:16px}"
  "button{padding:12px 20px;border:0;border-radius:8px;background:#2E7D5B;"
  "color:#fff;font-weight:700;font-size:16px}"
  "</style></head><body>"
  "<header>Chat de la clase &middot; ESP-NOW</header>"
  "<div id='c'></div>"
  "<footer><input id='i' placeholder='Escribe...' maxlength='60'>"
  "<button onclick='env()'>Enviar</button></footer>"
  "<script>"
  "async function upd(){try{const r=await fetch('/msgs');const d=await r.json();"
  "let s='';for(const m of d){const p=m.n=='PROFE'?' p':'';"
  "s+=\"<div class='m\"+p+\"'><span class='r'>\"+(m.r?m.r+' dBm':'')+\"</span>\"+"
  "\"<div class='n'>\"+m.n+' ('+m.m+\")</div><div class='t'>\"+m.t+'</div></div>';}"
  "const c=document.getElementById('c');c.innerHTML=s;c.scrollTop=c.scrollHeight;"
  "}catch(e){}}"
  "async function env(){const i=document.getElementById('i');"
  "if(!i.value)return;await fetch('/say?t='+encodeURIComponent(i.value));"
  "i.value='';upd();}"
  "document.getElementById('i').addEventListener('keydown',e=>{if(e.key=='Enter')env()});"
  "setInterval(upd,1000);upd();"
  "</script></body></html>");
  server.send(200, "text/html", h);
}

void msgs() {
  String j = "[";
  for (int i = 0; i < total; i++) {
    if (i) j += ",";
    j += "{\"n\":\"" + lista[i].nombre + "\",\"m\":\"" + lista[i].mac +
         "\",\"t\":\"" + lista[i].texto + "\",\"r\":" + String(lista[i].rssi) + "}";
  }
  j += "]";
  server.send(200, "application/json", j);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  WiFi.mode(WIFI_AP);
  WiFi.softAP("CLASE_IOT", "12345678", 1);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) { Serial.println("ERROR ESP-NOW"); return; }
  esp_now_register_recv_cb(alLlegar);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BROADCAST, 6);
  peer.channel = 1;
  peer.ifidx   = WIFI_IF_AP;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  server.on("/", pagina);
  server.on("/msgs", msgs);
  server.on("/say", [](){
    enviarProfe(server.arg("t"));
    server.send(200, "text/plain", "ok");
  });
  server.begin();

  Serial.print("Abre http://");
  Serial.println(WiFi.softAPIP());
}

void loop() { server.handleClient(); }