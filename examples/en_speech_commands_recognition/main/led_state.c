#include "led_state.h"

void state_led_ready(bool on) {
#if defined CONFIG_ESP32_S3_KORVO_2_V3_0_BOARD
  pca9555_set_value(PCA8555_AUDIO_OUT_PORT, 1 << PCA8555_GREEN_OUT_PIN,
                    on << PCA8555_GREEN_OUT_PIN);
#endif
}

void state_led_speech_recognition(bool on) {
#if defined CONFIG_ESP32_S3_KORVO_2_V3_0_BOARD
  pca9555_set_value(PCA8555_AUDIO_OUT_PORT, 1 << PCA8555_BLUE_OUT_PIN,
                    on << PCA8555_BLUE_OUT_PIN);
#endif
}

esp_err_t state_led_init() {
#if defined CONFIG_ESP32_S3_KORVO_2_V3_0_BOARD
  esp_err_t err = pca9555_init(I2C_NUM_0,
                               0xff & ((0 << PCA8555_AUDIO_OUT_PIN) |
                                       (0 << PCA8555_GREEN_OUT_PIN) |
                                       (0 << PCA8555_BLUE_OUT_PIN)),
                               0);
  if (err != ESP_OK) {
    printf("pca9555 error = %s\n", esp_err_to_name(err));
  } else {
    pca9555_set_value(PCA8555_AUDIO_OUT_PORT, (1 << PCA8555_AUDIO_OUT_PIN),
                      (1 << PCA8555_AUDIO_OUT_PIN));
    pca9555_set_value(PCA8555_GREEN_OUT_PORT, (1 << PCA8555_GREEN_OUT_PIN),
                      (0 << PCA8555_GREEN_OUT_PIN));
    pca9555_set_value(PCA8555_BLUE_OUT_PORT, (1 << PCA8555_BLUE_OUT_PIN),
                      (0 << PCA8555_BLUE_OUT_PIN));
  }
#endif
}