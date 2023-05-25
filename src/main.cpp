#include <Arduino.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>

esp_now_peer_info_t broadcastPeerInfo;
unsigned long lastHertbeatReceived{0};
unsigned long lastHertbeatSent{0};
bool peerAlive{false};
esp_now_peer_info_t peerInfo;
uint8_t peerMac[6];

void broadcast();
void onDataReceived(const uint8_t *macAddr, const uint8_t *data, int len);
void onDataSent(const uint8_t *macAddr, esp_now_send_status_t status);

void setup() {
  Serial.begin(115200);

  Serial.println("INFO: inicialização do MDI");
  Serial.println("INFO: mac address " + WiFi.macAddress());

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ERRO: erro ao inicializar ESP-NOW");
    Serial.println("INFO: reinicializando");
    ESP.restart();
  }

  esp_now_register_recv_cb(onDataReceived);
  esp_now_register_send_cb(onDataSent);
}

void loop() {
  unsigned long now = millis();

  if (!peerAlive) {
    broadcast();
  }
  else if (now - lastHertbeatReceived > 5e3) {
    Serial.println("INFO: conexão com o CTI perdida");
    peerAlive = false;
  }

  if (peerAlive && now - lastHertbeatSent > 2e3) {
    lastHertbeatSent = now;
    String data = "heartbeat";
    Serial.println("INFO: enviando heartbeat");
    esp_now_send((uint8_t*)peerMac, (const uint8_t*)data.c_str(), data.length());
  }
}

void broadcast() {
  for (int i = 1; i < 14; i++) {
    WiFi.printDiag(Serial);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(i, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);
    WiFi.printDiag(Serial);

    String data = "registro";
    uint8_t macAddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    memcpy(broadcastPeerInfo.peer_addr, macAddr, 6);
    broadcastPeerInfo.channel = i;
    broadcastPeerInfo.encrypt = false;
    broadcastPeerInfo.ifidx = WIFI_IF_STA;

    if (!esp_now_is_peer_exist(macAddr)) {
      esp_now_add_peer(&broadcastPeerInfo);
    }
    else {
      esp_now_del_peer(macAddr);
      esp_now_add_peer(&broadcastPeerInfo);
    }

    Serial.println("INFO: realizando broadcast no canal " + String(i));
    esp_err_t result = esp_now_send(macAddr, (const uint8_t*)data.c_str(), data.length());

    delay(2000);

    if (peerAlive) {
      break;
    }
  }
}

void onDataReceived(const uint8_t *macAddr, const uint8_t *data, int len) {
  char macStr[18];

  Serial.print("INFO: pacote recebido de: ");
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
  Serial.println(macStr);

  char buffer[ESP_NOW_MAX_DATA_LEN + 1];
  int msgLen = min(ESP_NOW_MAX_DATA_LEN, len);

  strncpy(buffer, (const char*)data, msgLen);
  buffer[msgLen] = 0;
  String mdiMac = WiFi.macAddress();
  mdiMac.toLowerCase();

  if (strcmp(buffer, mdiMac.c_str()) == 0) {
    Serial.println("INFO: recebida confirmação de pareamento");
    lastHertbeatReceived = millis();
    memcpy(peerInfo.peer_addr, macAddr, 6);
    memcpy(peerMac, macAddr, 6);
    peerInfo.channel = WiFi.channel();
    peerInfo.ifidx = WIFI_IF_STA;
    if (esp_now_is_peer_exist(macAddr)) {
      esp_now_del_peer(macAddr);
    }
    if (esp_now_add_peer(&peerInfo) == ESP_OK) {
      peerAlive = true;
      Serial.println("INFO: peer adicionado com sucesso");
    }
    else {
      Serial.println("ERRO: falha ao adicionar peer");
    }
  }
  else if(strcmp(buffer, "heartbeat") == 0) {
    lastHertbeatReceived = millis();
    Serial.println("INFO: heartbeat do CTI recebido com sucesso");
  }
}

void onDataSent(const uint8_t *macAddr, esp_now_send_status_t status) {
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "INFO: mensagem enviada com sucesso" : "ERRO: falha no envio da mensagem");
}