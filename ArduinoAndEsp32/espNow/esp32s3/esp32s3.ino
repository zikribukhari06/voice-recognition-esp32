#include <esp_now.h>
#include <WiFi.h>

// Ganti sama MAC Address ESP32 lawan/pasangan
uint8_t peerAddress[] = {0x6C,0xC8,0x40,0x33,0xC9,0xA0};

// Struktur data yang dikirim dan diterima
struct DataPacket {
  int id;
  int val;
};

DataPacket myData;        // Data yang bakal dikirim
DataPacket incomingData;  // Data yang diterima

esp_now_peer_info_t peerInfo;

// Callback saat terima data dari ESP32 lawan
void OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingDataPtr, int len) {
  memcpy(&incomingData, incomingDataPtr, sizeof(incomingData));
  Serial.print("Diterima dari ID ");
  Serial.print(incomingData.id);
  Serial.print(" | Nilai: ");
  Serial.println(incomingData.val);
}

// Callback saat status pengiriman selesai
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Kirim Sukses" : "Kirim Gagal");
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA); // Harus mode Station

  if (esp_now_init() != ESP_OK) {
    Serial.println("Gagal init ESP-NOW");
    return;
  }

  // Daftarkan fungsi callback kirim & terima
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  // Daftarkan peer (ESP32 pasangan)
  memcpy(peerInfo.peer_addr, peerAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Gagal add peer");
    return;
  }

  myData.id = 1; // Ubah jadi 2 saat di-upload ke ESP32 kedua
}

void loop() {
  myData.val = random(10, 99); // Simulasi data acak
  
  // Kirim data ke ESP32 pasangan
  esp_now_send(peerAddress, (uint8_t *) &myData, sizeof(myData));
  
  delay(2000); // Kirim tiap 2 detik
}