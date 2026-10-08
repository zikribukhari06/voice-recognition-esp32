#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define LED_PIN 38
#define SERVICE_UUID "f2d90c1e-596a-47ba-811a-7cb5f5d46617"
#define CHARACTERISTIC_UUID "dbb5030e-fb4b-4ef3-9ba8-377ddad5ed5c"

int hitunganRead = 0;

// Gabungkan fungsi onWrite dan onRead ke dalam SATU Class Callback
class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
    
    // Event terpicu saat HP MENGIRIM data (WRITE)
    void onWrite(BLECharacteristic *pCharacteristic) {
      String value = pCharacteristic->getValue();

      if (value.length() > 0) {
        Serial.print("Data diterima dari HP: ");
        Serial.println(value);

        if (value == "1") {
          analogWrite(LED_PIN, 255);
          Serial.println("-> LED Bawaan Menyala!");
        } else if (value == "0") {
          analogWrite(LED_PIN, 0);
          Serial.println("-> LED Bawaan Mati!");
        }
      }
    }

    // Event terpicu saat HP MEMINTA data (READ)
    void onRead(BLECharacteristic *pCharacteristic) {
      hitunganRead++;
      
      String dataKirim = "Data ke-" + String(hitunganRead);

      // Update isi characteristic sebelum dikirim ke HP
      pCharacteristic->setValue(dataKirim.c_str());

      Serial.print("Event onRead terpicu! Mengirim: ");
      Serial.println(dataKirim);
    }
};

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  Serial.println("set up BLE.....");

  BLEDevice::init("ESP_saya");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);

  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  // Pasang gabungan callback CUKUP SATU KALI
  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

  pCharacteristic->setValue("BLE berhasil di-Set up");
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("SET UP BLE sudah berhasil dilakukan");
}

void loop() {
  delay(1000);
}
