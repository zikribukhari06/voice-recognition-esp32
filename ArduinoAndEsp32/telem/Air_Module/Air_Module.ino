/*
 * AIR MODULE - Two-way Transparent Serial Bridge (ESP-NOW)
 * Hardware : ESP32 standar
 * Fungsi   : Pixhawk TELEM1 (Serial2) <-> ESP-NOW <-> Ground Module
 *
 * Wiring:
 *   ESP32 GPIO16 (RX2) <- Pixhawk TELEM1 TX
 *   ESP32 GPIO17 (TX2) -> Pixhawk TELEM1 RX
 *   GND ESP32          -- GND Pixhawk
 *
 * Catatan: Tidak ada Serial.print() teks apapun. Hanya data biner murni.
 *          ESP32 Core v2.x.x
 */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// MAC Ground Module (target pengiriman)
const uint8_t GROUND_MAC[6] = {0x6C,0xC8,0x40,0x33,0xC9,0xA0}; 

#define ESPNOW_CHANNEL   1
#define SEND_BUF_SIZE    240    // batas payload per paket
#define SEND_TIMEOUT_MS  5      // kirim paket jika idle > 5 ms
#define PIX_BAUD         115200
#define PIX_RX_PIN 44 // Sesuaikan dengan pin TX dari TELEM Pixhawk
#define PIX_TX_PIN 43
#define PIX_BUF_SIZE     4096

// ---------- Buffer kirim (Pixhawk -> ESP-NOW) ----------
static uint8_t  txBuf[SEND_BUF_SIZE];
static size_t   txLen = 0;
static uint32_t lastByteTime = 0;

// ---------- Ring buffer terima (ESP-NOW -> Pixhawk) ----------
#define RING_SIZE 8192          // harus pangkat 2
static uint8_t           ring[RING_SIZE];
static volatile uint32_t ringHead = 0;   // ditulis oleh callback
static volatile uint32_t ringTail = 0;   // dibaca oleh loop

// Callback terima ESP-NOW (format ESP32 Core v2.x.x)
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  for (int i = 0; i < len; i++) {
    uint32_t next = (ringHead + 1) & (RING_SIZE - 1);
    if (next == ringTail) break;         // ring penuh -> buang sisa
    ring[ringHead] = incomingData[i];
    ringHead = next;
  }
}

static void flushToEspNow() {
  if (txLen == 0) return;
  
  esp_now_send(GROUND_MAC, txBuf, txLen);
  txLen = 0; // Wajib paksa reset di sini!
}

void setup() {
  Serial2.setRxBufferSize(PIX_BUF_SIZE);
  Serial2.begin(PIX_BAUD, SERIAL_8N1, PIX_RX_PIN, PIX_TX_PIN);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    ESP.restart();
  }

  esp_now_register_recv_cb(OnDataRecv);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, GROUND_MAC, 6);
  peerInfo.channel = ESPNOW_CHANNEL;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
}

void loop() {
  // 1) Pixhawk -> buffer -> ESP-NOW
  while (Serial2.available() && txLen < SEND_BUF_SIZE) {
    int b = Serial2.read();
    if (b < 0) break;
    txBuf[txLen++] = (uint8_t)b;
    lastByteTime = millis();
  }

  if (txLen >= SEND_BUF_SIZE) {
    flushToEspNow();
  } else if (txLen > 0 && (millis() - lastByteTime) > SEND_TIMEOUT_MS) {
    flushToEspNow();
  }

  // 2) ESP-NOW -> ring buffer -> Pixhawk
  while (ringTail != ringHead) {
    int space = Serial2.availableForWrite();
    if (space <= 0) break;               // jangan blocking
    while (space-- > 0 && ringTail != ringHead) {
      Serial2.write(ring[ringTail]);
      ringTail = (ringTail + 1) & (RING_SIZE - 1);
    }
  }
}
