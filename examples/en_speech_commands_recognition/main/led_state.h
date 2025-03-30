#ifndef LED_STATE_H
#define LED_STATE_H

#include "pca9555.h"

#define PCA8555_AUDIO_OUT_PIN PCA_PIN_P00
#define PCA8555_AUDIO_OUT_PORT pca_port_0
#define PCA8555_GREEN_OUT_PIN PCA_PIN_P06
#define PCA8555_GREEN_OUT_PORT PCA8555_AUDIO_OUT_PORT
#define PCA8555_BLUE_OUT_PIN PCA_PIN_P07
#define PCA8555_BLUE_OUT_PORT PCA8555_AUDIO_OUT_PORT

void state_led_ready(bool on);
    
void state_led_speech_recognition(bool on);
    
esp_err_t state_led_init();

#endif