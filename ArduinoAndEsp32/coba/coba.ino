#include <driver/i2s.h>

#define I2S_WS 37
#define I2S_SD 35
#define I2S_SCK 36

#define I2S_PORT I2S_NUM_0

void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,                         // 16kHz (standar buat Voice Detection / TinyML)
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // Data 24-bit INMP441 dibungkus dalam slot 32-bit
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,  // Mode Mono (karena pin L/R dicolok ke GND)
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
}

void setup() {
  Serial.begin(115200);
  setupI2S();
  delay(500);
}

void loop() {
  int32_t raw_sample = 0;
  size_t bytes_read = 0;

  // Ambil 1 sampel audio dari buffer DMA
  esp_err_t result = i2s_read(I2S_PORT, &raw_sample, sizeof(raw_sample), &bytes_read, portMAX_DELAY);

  if (result == ESP_OK && bytes_read > 0) {
    // Shift bit 8 kali ke kanan karena data aslinya 24-bit di dalam wadah 32-bit
    int32_t sample = raw_sample >> 8;

    // Output angka ke Serial Plotter
    Serial.println(sample);
  }
}
