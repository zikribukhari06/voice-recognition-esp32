#include <Arduino.h>
#include <Wire.h>

#define MPU6050_ADDR 0x68

#define PWR_MGMT_1 0x6B
#define ACCEL_XOUT_H 0x3B

struct RawData {
  int16_t x;
  int16_t y;
  int16_t z;
};

void writeRegister(uint8_t regAddress, uint8_t dataByte) {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(regAddress);
  Wire.write(dataByte);
  Wire.endTransmission();
}

RawData readAccelRaw() {
  RawData accel;

  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(ACCEL_XOUT_H);
  Wire.endTransmission(false);

  Wire.requestFrom((uint8_t)MPU6050_ADDR, (uint8_t)6);
  if (Wire.available() >= 6) {
      uint8_t x_h = Wire.read();
      uint8_t x_l = Wire.read();
      uint8_t y_h = Wire.read();
      uint8_t y_l = Wire.read();
      uint8_t z_h = Wire.read();
      uint8_t z_l = Wire.read();
  accel.x = (int16_t)((x_h << 8 ) | x_l);
  accel.y = (int16_t)((y_h << 8 ) | y_l);
  accel.z = (int16_t)((z_h << 8 ) | z_l);
  }
  return accel;
}
void setup() {
  Serial.begin(115200);
  
  // Inisialisasi I2C ESP32 (SDA = GPIO 21, SCL = GPIO 22)
  Wire.begin(21, 22);

  // Matikan mode sleep MPU6050 (Kirim 0x00 ke register 0x6B)
  writeRegister(PWR_MGMT_1, 0x00);
  delay(100);

  Serial.println("MPU6050 siap dibaca!");
}

void loop() {
  RawData dataMentah = readAccelRaw();

  Serial.print("Raw X: "); Serial.print(dataMentah.x);
  Serial.print(" | Y: ");  Serial.print(dataMentah.y);
  Serial.print(" | Z: ");  Serial.println(dataMentah.z);

  delay(500);
}