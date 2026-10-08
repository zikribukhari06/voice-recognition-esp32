#include "esp_camera.h"

// Pin mapping AI-Thinker ESP32-CAM
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Variabel global: harus di atas setup() dan loop()
int aecVal = 70;

bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size   = FRAMESIZE_QQVGA;
  config.fb_count     = 2;
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.grab_mode    = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Kamera gagal init, error 0x%x\n", err);
    return false;
  }

  sensor_t *s = esp_camera_sensor_get();
  s->set_exposure_ctrl(s, 0);   // auto exposure OFF
  s->set_aec_value(s, aecVal);  // exposure manual
  s->set_gain_ctrl(s, 0);       // auto gain OFF
  s->set_agc_gain(s, 5);        // gain rendah dulu
  s->set_whitebal(s, 1);        // AWB ON dulu, dikunci nanti
  s->set_awb_gain(s, 1);
  s->set_whitebal(s, 0);        // AWB dikunci
  s->set_awb_gain(s, 0);
  return true;
}

// ---------- RGB565 -> HSV ----------

static inline void rgb565ToHsv(uint16_t p, uint8_t &h, uint8_t &s, uint8_t &v) {
  uint8_t r5 = (p >> 11) & 0x1F;
  uint8_t g6 = (p >> 5)  & 0x3F;
  uint8_t b5 =  p        & 0x1F;

  int r = (r5 << 3) | (r5 >> 2);
  int g = (g6 << 2) | (g6 >> 4);
  int b = (b5 << 3) | (b5 >> 2);

  int mx = max(r, max(g, b));
  int mn = min(r, min(g, b));
  int d  = mx - mn;

  v = mx;
  if (mx == 0 || d == 0) {
    s = 0;
    h = 0;
    return;
  }
  s = (255 * d) / mx;

  int hh;
  if (mx == r)      hh = (30 * (g - b)) / d;
  else if (mx == g) hh = 60 + (30 * (b - r)) / d;
  else              hh = 120 + (30 * (r - g)) / d;
  if (hh < 0) hh += 180;
  h = hh;
}

static inline uint16_t getPixel(const uint8_t *buf, int idx) {
  return ((uint16_t)buf[2 * idx] << 8) | buf[2 * idx + 1];
}

void setup() {
  Serial.begin(115200);
  delay(500);
  if (!initCamera()) {
    while (true) delay(1000);
  }
  Serial.println("Kamera siap! Kirim '+' atau '-' untuk ubah exposure.");
}

void loop() {
  // Kontrol exposure lewat Serial: '+' / '-' (langkah 20)
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '+') aecVal += 20;
    if (c == '-') aecVal -= 20;
    aecVal = constrain(aecVal, 5, 1200);
    sensor_t *sn = esp_camera_sensor_get();
    sn->set_aec_value(sn, aecVal);
    Serial.printf(">>> aec = %d\n", aecVal);
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return;

  int x0 = fb->width / 2 - 10;
  int y0 = fb->height / 2 - 10;
  long sumR = 0, sumG = 0, sumB = 0, sumS = 0, sumV = 0;
  int hMin = 255, hMax = 0, n = 0;

  for (int y = y0; y < y0 + 20; y++) {
    for (int x = x0; x < x0 + 20; x++) {
      uint16_t p = getPixel(fb->buf, y * fb->width + x);
      uint8_t h, s, v;
      rgb565ToHsv(p, h, s, v);
      uint8_t r5 = (p >> 11) & 0x1F, g6 = (p >> 5) & 0x3F, b5 = p & 0x1F;
      sumR += r5; sumG += g6; sumB += b5;
      sumS += s;  sumV += v;
      if (h < hMin) hMin = h;
      if (h > hMax) hMax = h;
      n++;
    }
  }

  Serial.printf("aec=%d | RGB avg=%ld/%ld/%ld | H=%d..%d | S avg=%ld | V avg=%ld\n",
                aecVal, sumR / n, sumG / n, sumB / n,
                hMin, hMax, sumS / n, sumV / n);

  esp_camera_fb_return(fb);
  delay(300);
}