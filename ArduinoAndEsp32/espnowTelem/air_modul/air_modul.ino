#include <esp_now.h>
#include <WiFi.h>

uint8_t groundMAC[] = {0x24, 0x0A, 0xC4, 0x9A, 0x03, 0xA4};

uint8_t sendBuffer[250];
int bufLen = 0;
unsigned long lastReadTime = 0; // Timer untuk nge-pack data

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  Serial2.write(incomingData, len);
}

void setup() {
  // rxPixhawk ke TX2(17), txPixhawk ke RX2(16)
  Serial2.begin(115200, SERIAL_8N1, 16, 17);

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) return;

  esp_now_register_recv_cb(OnDataRecv);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, groundMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
}

void loop() {
  while (Serial2.available() > 0) {
    sendBuffer[bufLen] = Serial2.read();
    bufLen++;
    lastReadTime = millis(); 

    if (bufLen >= 240) {
      esp_now_send(groundMAC, sendBuffer, bufLen);
      bufLen = 0;
    }
  }

  // Tunggu 5ms baru kirim sisa buffer
  if (bufLen > 0 && (millis() - lastReadTime > 5)) {
    esp_now_send(groundMAC, sendBuffer, bufLen);
    bufLen = 0;
  }
  
  delay(1); 
}