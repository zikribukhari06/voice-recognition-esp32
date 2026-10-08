   #include <esp_system.h>
   void setup() {
     Serial.begin(115200);
     Serial.printf("Reset reason: %d\n", (int)esp_reset_reason());
   }
   void loop() {}