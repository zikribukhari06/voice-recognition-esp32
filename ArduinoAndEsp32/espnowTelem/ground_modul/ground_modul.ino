#include <esp_now.h>
#include <WiFi.h>

uint8_t airMAC[] = {0x6C, 0xC8, 0x40, 0x33, 0xC9, 0xA0};

uint8_t sendBuffer[250];
int bufLen = 0;
unsigned long lastReadTime = 0; // Tambahan timer untuk nge-pack data

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  Serial.write(incomingData, len);
}

void setup() {
  // PERBESAR buffer USB CDC biar nggak mampet saat MP nge-spam data awal
  Serial.setRxBufferSize(4096);
  Serial.setTxBufferSize(4096);
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) return;

  esp_now_register_recv_cb(OnDataRecv);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, airMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
}

void loop() {
  while (Serial.available() > 0) {
    sendBuffer[bufLen] = Serial.read();
    bufLen++;
    lastReadTime = millis(); // Catat waktu terakhir nerima byte data

    // Kalau buffer udah menyentuh batas aman ESP-NOW (240 byte), langsung tembak
    if (bufLen >= 240) {
      esp_now_send(airMAC, sendBuffer, bufLen);
      bufLen = 0;
    }
  }

  // JANGAN ECER DATA: 
  // Tunggu sampai 5ms sejak byte terakhir diterima, baru tembak sisanya.
  // Ini mencegah chip Wi-Fi overload karena di-spam instruksi send.
  if (bufLen > 0 && (millis() - lastReadTime > 5)) {
    esp_now_send(airMAC, sendBuffer, bufLen);
    bufLen = 0;
  }
  
  delay(1);
}