/*
 * GROUND MODULE - Two-way Transparent Serial Bridge (ESP-NOW)
 * Hardware : ESP32-S3 (Native USB CDC)
 * Fungsi   : Mission Planner (USB) <-> ESP-NOW <-> Air Module
 *
 * Pengaturan Arduino IDE (Tools):
 *   - Board              : ESP32S3 Dev Module
 *   - USB CDC On Boot    : Enabled
 *   - USB Mode           : Hardware CDC and JTAG / USB-OTG (TinyUSB) sesuai port yang dipakai
 *   - ESP32 Core         : v2.x.x
 *
 * Catatan: Tidak ada Serial.print() teks apapun. Hanya data biner murni.
 */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// MAC Air Module (target pengiriman)
const uint8_t AIR_MAC[6] = { 0x7C,0xE8,0xB1,0xB1,0xE7,0xB8}; // mac addres esp32 dev modul 

#define ESPNOW_CHANNEL   1
#define SEND_BUF_SIZE    240    // batas payload per paket
#define SEND_TIMEOUT_MS  5      // kirim paket jika idle > 5 ms
#define USB_BAUD         57600
#define USB_BUF_SIZE     4096

// ---------- Buffer kirim (USB -> ESP-NOW) ----------
static uint8_t  txBuf[SEND_BUF_SIZE];
static size_t   txLen = 0;
static uint32_t lastByteTime = 0;

// ---------- Ring buffer terima (ESP-NOW -> USB) ----------
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
  if (esp_now_send(AIR_MAC, txBuf, txLen) == ESP_OK) {
    txLen = 0;
  }
  // Jika gagal, buffer dipertahankan dan dicoba lagi pada iterasi loop berikutnya
}

void setup() {
  Serial.setRxBufferSize(USB_BUF_SIZE);
  Serial.setTxBufferSize(USB_BUF_SIZE);
  Serial.begin(USB_BAUD);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    // Tidak boleh print apapun ke Serial. Restart agar mencoba ulang.
    ESP.restart();
  }

  esp_now_register_recv_cb(OnDataRecv);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, AIR_MAC, 6);
  peerInfo.channel = ESPNOW_CHANNEL;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
}

void loop() {
  // 1) USB (Mission Planner) -> buffer -> ESP-NOW
  while (Serial.available() && txLen < SEND_BUF_SIZE) {
    int b = Serial.read();
    if (b < 0) break;
    txBuf[txLen++] = (uint8_t)b;
    lastByteTime = millis();
  }

  if (txLen >= SEND_BUF_SIZE) {
    flushToEspNow();
  } else if (txLen > 0 && (millis() - lastByteTime) > SEND_TIMEOUT_MS) {
    flushToEspNow();
  }

  // 2) ESP-NOW -> ring buffer -> USB (Mission Planner)
  while (ringTail != ringHead) {
    int space = Serial.availableForWrite();
    if (space <= 0) break;               // jangan blocking
    while (space-- > 0 && ringTail != ringHead) {
      Serial.write(ring[ringTail]);
      ringTail = (ringTail + 1) & (RING_SIZE - 1);
    }
  }
}
