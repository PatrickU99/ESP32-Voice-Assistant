#include <driver/i2s.h>
#include "display_functionality.h"

// Microphone pins (INMP441)
#define I2S_MIC_SD  41
#define I2S_MIC_SCK 42
#define I2S_MIC_WS  40
#define RECORD_SECONDS 5

// Recording settings
int sample_rate = 16000;
int buffer_size = (sample_rate * RECORD_SECONDS * 2);

int16_t* audio_buffer = nullptr;

void buffer_allocation() {
  audio_buffer = (int16_t*)ps_malloc(buffer_size);
  if (!audio_buffer) {
    Serial.println("PSRAM allocation failed!");
    return;
  }
}

void stop_mic() {
  i2s_driver_uninstall(I2S_NUM_1);
}

void setup_mic() {
  i2s_config_t mic_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = sample_rate,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 512,
    .use_apll = false
  };

  i2s_pin_config_t mic_pins = {
    .bck_io_num = I2S_MIC_SCK,
    .ws_io_num = I2S_MIC_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_MIC_SD
  };

  i2s_driver_install(I2S_NUM_1, &mic_config, 0, NULL);
  i2s_set_pin(I2S_NUM_1, &mic_pins);
}

void record_audio() {
  // Reinstall mic driver before recording
  setup_mic();
  
  Serial.println("Recording... speak now!");
  set_text("Recording... speak now!");
  size_t bytesRead = 0;
  size_t totalRead = 0;
  //while the mic hasnt recorded the audio data threshold read more bytes of audio
  while (totalRead < buffer_size) {
    i2s_read(I2S_NUM_1, audio_buffer + (totalRead / 2),
             buffer_size - totalRead, &bytesRead, portMAX_DELAY);
    totalRead += bytesRead;
  }
  Serial.println("Recording done!");
  set_text("Recording done!");
  delay(1000);
  
  set_text("Transcribing.....");
  
  // Uninstall mic driver after recording so Audio library can use I2S again
  stop_mic();
}

