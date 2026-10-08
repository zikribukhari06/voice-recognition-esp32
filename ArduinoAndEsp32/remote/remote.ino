#define USE_NIMBLE
#include <BleGamepad.h>
#include <IBusBM.h>

// Bikin object IBus dan BLE Gamepad
IBusBM IBus;
// Format: Nama Device, Nama Pembuat, Kapasitas Baterai (100%)
BleGamepad bleGamepad("Flysky Simulator", "DIY", 100); 

void setup() {
  Serial.begin(115200);
  
  // 1. Inisialisasi Bluetooth duluan sampai selesai
  bleGamepad.begin(); 
  Serial.println("Bluetooth ready...");

  // 2. Baru setting pin Serial2 ESP32-S3 dan aktifkan IBus
  IBus.begin(Serial2, IBUSBM_NOTIMER, 18, 17);
  
  Serial.println("Setup selesai! Menunggu koneksi...");
}
void loop() {
  IBus.loop(); // Wajib dipanggil terus buat update data receiver

  // Cek kalau laptop udah connect ke ESP32 via Bluetooth
  if (bleGamepad.isConnected()) {
    Serial.println(IBus.readChannel(0));
    
    // BACA STICK (Channel 1-4 / Index 0-3)
    // Sinyal mentah IBus = 1000 s/d 2000
    // Library Gamepad butuh = -32767 s/d 32767
    // Mapping disesuaikan dengan standar Mode 2 (AETR / Roll, Pitch, Throttle, Yaw)
    
    int roll     = map(IBus.readChannel(0), 1000, 2000, -32767, 32767);
    int pitch    = map(IBus.readChannel(1), 1000, 2000, 32767, -32767); // Invert axis
    int throttle = map(IBus.readChannel(2), 1000, 2000, -32767, 32767);
    int yaw      = map(IBus.readChannel(3), 1000, 2000, -32767, 32767);

    // Set pergerakan analog stick kiri dan kanan
    bleGamepad.setLeftThumb(yaw, throttle); 
    bleGamepad.setRightThumb(roll, pitch);  

    // BACA SWITCH / AUX (Channel 5-6 / Index 4-5)
    // Nilai > 1500 artinya switch posisinya lagi di bawah/aktif
    
    // Switch 1 (CH 5) -> Ditekan sebagai Button 1
    if (IBus.readChannel(4) > 1500) {
      bleGamepad.press(BUTTON_1);
    } else {
      bleGamepad.release(BUTTON_1);
    }

    // Switch 2 (CH 6) -> Ditekan sebagai Button 2
    if (IBus.readChannel(5) > 1500) {
      bleGamepad.press(BUTTON_2);
    } else {
      bleGamepad.release(BUTTON_2);
    }

    // Delay kecil biar ngirim datanya stabil dan nggak bikin lag
   
  }
   delay(10);
}