#include <WiFi.h> // library Wifi.h

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  delay(1000);
  Serial.println();
  Serial.print("MAC Address ESP32 ini: ");
  Serial.println(WiFi.macAddress());
}

void loop() {
}