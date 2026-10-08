/*
 * AIR MODULE - Two-way Transparent Serial Bridge (ESP-NOW)
 * Hardware : ESP32 standar (DevKit)
 * Fungsi   : Pixhawk TELEM2 (Serial2) <-> ESP-NOW <-> Ground Module
 *
 * Wiring TELEM2:
 *   ESP32 GPIO16 (RX2) <- Pixhawk TELEM2 TX (pin 2)
 *   ESP32 GPIO17 (TX2) -> Pixhawk TELEM2 RX (pin 3)
 *   GND ESP32          -- GND Pixhawk (pin 6)
 *   CTS/RTS dibiarkan kosong
 *
 * Parameter Pixhawk: SERIAL2_PROTOCOL=2, SERIAL2_BAUD=115, BRD_SER2_RTSCTS=0
 *
 * LED GPIO2 (bawaan board) = status pengiriman ke Ground:
 *   - Kedip PENDEK (30 ms)  : paket dari Pixhawk terkirim DAN diterima Ground (ACK OK)
 *   - Kedip PANJANG (400 ms): paket dari Pixhawk ada, tapi GAGAL sampai ke Ground
 *                             (cek GROUND_MAC / Ground belum menyala)
 *   - Mati terus            : tidak ada data dari Pixhawk (cek wiring / parameter)
 *
 * Catatan: Tidak ada Serial.print() teks apapun. Hanya data biner murni.
 *          ESP32 Core v2.x.x
 */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// =====================================================================
// !! WAJIB DIGANTI dengan MAC address board GROUND yang sekarang dipakai
// !! (ESP32 DevKit Ground). Nilai di bawah adalah MAC lama (ESP32-S3).
// =====================================================================
// const uint8_t GROUND_MAC[6] = {0x6C,0xC8,0x40,0x33,0xC9,0xA0}; // mac addres esp32 jg
const uint8_t GROUND_MAC[6] = {0x20,0x50,0x0D,0xD0,0x28,0x2C};  // mac adress esp32 ipt

#define ESPNOW_CHANNEL   1
#define SEND_BUF_SIZE    240     // batas payload per paket
#define SEND_TIMEOUT_MS  5       // kirim paket jika idle > 5 ms
#define PIX_BAUD         115200  // samakan dengan SERIAL2_BAUD=115
#define PIX_RX_PIN       16
#define PIX_TX_PIN       17
#define PIX_BUF_SIZE     4096

#define LED_PIN          2
#define LED_OK_MS        30
#define LED_FAIL_MS      400

// ---------- Buffer kirim (Pixhawk -> ESP-NOW) ----------
static uint8_t  txBuf[SEND_BUF_SIZE];
static size_t   txLen = 0;
static uint32_t lastByteTime = 0;

// ---------- Ring buffer terima (ESP-NOW -> Pixhawk) ----------
#define RING_SIZE 8192           // harus pangkat 2
static uint8_t           ring[RING_SIZE];
static volatile uint32_t ringHead = 0;   // ditulis oleh callback
static volatile uint32_t ringTail = 0;   // dibaca oleh loop

// ---------- Indikator LED ----------
static volatile int8_t sendResult = 0;   // 0 = belum ada, 1 = OK, -1 = gagal
static uint32_t        ledOffAt   = 0;

// Callback terima ESP-NOW (format ESP32 Core v2.x.x)
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  for (int i = 0; i < len; i++) {
    uint32_t next = (ringHead + 1) & (RING_SIZE - 1);
    if (next == ringTail) break;         // ring penuh -> buang sisa
    ring[ringHead] = incomingData[i];
    ringHead = next;
  }
}

// Callback status kirim ESP-NOW (format ESP32 Core v2.x.x)
void OnDataSent(const uint8_t *mac, esp_now_send_status_t status) {
  sendResult = (status == ESP_NOW_SEND_SUCCESS) ? 1 : -1;
}

static void flushToEspNow() {
  if (txLen == 0) return;

  // Kirim paket. Berhasil atau gagal, buffer selalu dikosongkan
  // agar jalur tidak mampet.
  esp_now_send(GROUND_MAC, txBuf, txLen);
  txLen = 0;
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial2.setRxBufferSize(PIX_BUF_SIZE);
  Serial2.begin(PIX_BAUD, SERIAL_8N1, PIX_RX_PIN, PIX_TX_PIN);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    ESP.restart();
  }

  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, GROUND_MAC, 6);
  peerInfo.channel = ESPNOW_CHANNEL;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
}

void loop() {
  // 0) Indikator LED (non-blocking)
  int8_t r = sendResult;
  if (r != 0) {
    sendResult = 0;
    digitalWrite(LED_PIN, HIGH);
    ledOffAt = millis() + ((r > 0) ? LED_OK_MS : LED_FAIL_MS);
  }
  if (ledOffAt != 0 && (int32_t)(millis() - ledOffAt) >= 0) {
    digitalWrite(LED_PIN, LOW);
    ledOffAt = 0;
  }

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
