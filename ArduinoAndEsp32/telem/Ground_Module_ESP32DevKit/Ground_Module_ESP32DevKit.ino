/*
 * GROUND MODULE - Two-way Transparent Serial Bridge (ESP-NOW)
 * Hardware : ESP32 DevKit standar (USB lewat chip CP2102/CH340 -> UART0)
 * Fungsi   : Mission Planner (USB) <-> ESP-NOW <-> Air Module
 *
 * Pengaturan Arduino IDE (Tools):
 *   - Board        : ESP32 Dev Module
 *   - ESP32 Core   : v2.x.x
 *
 * PENTING: USB_BAUD harus SAMA dengan baudrate di Mission Planner.
 *
 * LED GPIO2 (bawaan board):
 *   - Berkedip singkat setiap ada paket ESP-NOW MASUK dari Air Module.
 *   - Tidak pernah menyala = tidak ada paket dari Air (cek MAC / Air Module).
 *
 * Catatan: Tidak ada Serial.print() teks apapun. Hanya data biner murni.
 */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// MAC Air Module (target pengiriman)
const uint8_t AIR_MAC[6] = {0x7C, 0xE8, 0xB1, 0xB1, 0xE7, 0xB8}; // mac addres for esp32 s3

#define ESPNOW_CHANNEL   1
#define SEND_BUF_SIZE    240     // batas payload per paket
#define SEND_TIMEOUT_MS  5       // kirim paket jika idle > 5 ms
#define USB_BAUD         115200  // samakan dengan Mission Planner
#define USB_RX_BUF_SIZE  4096

#define LED_PIN          2
#define LED_BLINK_MS     30

// ---------- Buffer kirim (Serial -> ESP-NOW) ----------
static uint8_t  txBuf[SEND_BUF_SIZE];
static size_t   txLen = 0;
static uint32_t lastByteTime = 0;

// ---------- Ring buffer terima (ESP-NOW -> Serial) ----------
#define RING_SIZE 8192           // harus pangkat 2
static uint8_t           ring[RING_SIZE];
static volatile uint32_t ringHead = 0;   // ditulis oleh callback
static volatile uint32_t ringTail = 0;   // dibaca oleh loop

// ---------- Indikator LED ----------
static volatile bool gotPacket = false;  // diset oleh callback
static uint32_t      ledOffAt  = 0;

// Callback terima ESP-NOW (format ESP32 Core v2.x.x)
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  gotPacket = true;
  for (int i = 0; i < len; i++) {
    uint32_t next = (ringHead + 1) & (RING_SIZE - 1);
    if (next == ringTail) break;         // ring penuh -> buang sisa
    ring[ringHead] = incomingData[i];
    ringHead = next;
  }
}

static void flushToEspNow() {
  if (txLen == 0) return;

  // Kirim paket. Berhasil atau gagal, buffer selalu dikosongkan
  // agar jalur tidak mampet.
  esp_now_send(AIR_MAC, txBuf, txLen);
  txLen = 0;
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Serial = UART0 (HardwareSerial) pada ESP32 DevKit.
  // setRxBufferSize() wajib dipanggil SEBELUM begin().
  Serial.setRxBufferSize(USB_RX_BUF_SIZE);
  Serial.begin(USB_BAUD);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
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
  // 0) Indikator LED (non-blocking)
  if (gotPacket) {
    gotPacket = false;
    digitalWrite(LED_PIN, HIGH);
    ledOffAt = millis() + LED_BLINK_MS;
  }
  if (ledOffAt != 0 && (int32_t)(millis() - ledOffAt) >= 0) {
    digitalWrite(LED_PIN, LOW);
    ledOffAt = 0;
  }

  // 1) Serial (Mission Planner) -> buffer -> ESP-NOW
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

  // 2) ESP-NOW -> ring buffer -> Serial (Mission Planner)
  while (ringTail != ringHead) {
    int space = Serial.availableForWrite();
    if (space <= 0) break;               // jangan blocking
    while (space-- > 0 && ringTail != ringHead) {
      Serial.write(ring[ringTail]);
      ringTail = (ringTail + 1) & (RING_SIZE - 1);
    }
  }
}