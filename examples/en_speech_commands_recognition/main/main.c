/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_board_init.h"
#include "speech_commands_action.h"
#include "model_path.h"
#include "esp_process_sdkconfig.h"

#include "pca9555.h"


int detect_flag = 0;
static esp_afe_sr_iface_t *afe_handle = NULL;
static volatile int task_flag = 0;
srmodel_list_t *models = NULL;
static int play_voice = -2;

#define PCA8555_AUDIO_OUT_PIN		PCA_PIN_P00
#define PCA8555_AUDIO_OUT_PORT		pca_port_0
#define PCA8555_GREEN_OUT_PIN		PCA_PIN_P06
#define PCA8555_GREEN_OUT_PORT		PCA8555_AUDIO_OUT_PORT
#define PCA8555_BLUE_OUT_PIN		PCA_PIN_P07
#define PCA8555_BLUE_OUT_PORT		PCA8555_AUDIO_OUT_PORT

void play_music(void *arg)
{
    while (task_flag)
    {
        switch (play_voice)
        {
        case -2:
            vTaskDelay(10);
            break;
        case -1:
            wake_up_action();
            play_voice = -2;
            break;
        case 1:            
		    pca9555_set_value(PCA8555_AUDIO_OUT_PORT, 1 << PCA8555_GREEN_OUT_PIN, 1 << PCA8555_GREEN_OUT_PIN);
            play_voice = -2;
            break;
        case 2:
		    pca9555_set_value(PCA8555_AUDIO_OUT_PORT, 1 << PCA8555_GREEN_OUT_PIN, 0 << PCA8555_GREEN_OUT_PIN);
            play_voice = -2;
            break;
        case 3:            
		    pca9555_set_value(PCA8555_AUDIO_OUT_PORT, 1 << PCA8555_BLUE_OUT_PIN, 1 << PCA8555_BLUE_OUT_PIN);
            play_voice = -2;
            break;
        case 4:
		    pca9555_set_value(PCA8555_AUDIO_OUT_PORT, 1 << PCA8555_BLUE_OUT_PIN, 0 << PCA8555_BLUE_OUT_PIN);
            play_voice = -2;
            break;
        default:
//            speech_commands_action(play_voice);
//            play_voice = -2;
            break;
        }
    }
    vTaskDelete(NULL);
}

void feed_Task(void *arg)
{
    esp_afe_sr_data_t *afe_data = arg;
    int audio_chunksize = afe_handle->get_feed_chunksize(afe_data);
    int nch = afe_handle->get_channel_num(afe_data);
    int feed_channel = esp_get_feed_channel();
    assert(nch <= feed_channel);
    int16_t *i2s_buff = malloc(audio_chunksize * sizeof(int16_t) * feed_channel);
    assert(i2s_buff);

    while (task_flag)
    {
        esp_get_feed_data(false, i2s_buff, audio_chunksize * sizeof(int16_t) * feed_channel);

        afe_handle->feed(afe_data, i2s_buff);
    }
    if (i2s_buff)
    {
        free(i2s_buff);
        i2s_buff = NULL;
    }
    vTaskDelete(NULL);
}

void led_Task(void *arg) {
	while (1) {
        if (play_voice == 1){
		    pca9555_set_value(PCA8555_AUDIO_OUT_PORT, 1 << PCA8555_GREEN_OUT_PIN, 1 << PCA8555_GREEN_OUT_PIN);
        }
        else if (play_voice == 2){
		    pca9555_set_value(PCA8555_AUDIO_OUT_PORT, 1 << PCA8555_GREEN_OUT_PIN, 0 << PCA8555_GREEN_OUT_PIN);
        }
/*
		pca9555_set_value(PCA8555_AUDIO_OUT_PORT,
				(1 << PCA8555_GREEN_OUT_PIN) | (1 << PCA8555_BLUE_OUT_PIN),
				((i & 1) << PCA8555_GREEN_OUT_PIN)
						| ((i & 1) << PCA8555_BLUE_OUT_PIN));
		i++;
*/
		pca9555_set_value(PCA8555_AUDIO_OUT_PORT, 0xff, 0xff);
		vTaskDelay(100/portTICK_PERIOD_MS);
		pca9555_set_value(PCA8555_AUDIO_OUT_PORT, 0xff, 0x0);
		vTaskDelay(100/portTICK_PERIOD_MS);
	}
}

void detect_Task(void *arg)
{
    esp_afe_sr_data_t *afe_data = arg;
    int afe_chunksize = afe_handle->get_fetch_chunksize(afe_data);
    char *mn_name = esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_ENGLISH);
    printf("multinet:%s\n", mn_name);
    esp_mn_iface_t *multinet = esp_mn_handle_from_name(mn_name);
    model_iface_data_t *model_data = multinet->create(mn_name, 6000);
    int mu_chunksize = multinet->get_samp_chunksize(model_data);
    esp_mn_commands_update_from_sdkconfig(multinet, model_data); // Add speech commands from sdkconfig
    assert(mu_chunksize == afe_chunksize);
    // print active speech commands
    multinet->print_active_speech_commands(model_data);

    printf("------------detect start------------\n");
    while (task_flag)
    {
        afe_fetch_result_t *res = afe_handle->fetch(afe_data);
        if (!res || res->ret_value == ESP_FAIL)
        {
            printf("fetch error!\n");
            break;
        }

        if (res->wakeup_state == WAKENET_DETECTED)
        {
            printf("WAKEWORD DETECTED\n");
            multinet->clean(model_data);
        }
        else if (res->wakeup_state == WAKENET_CHANNEL_VERIFIED)
        {
            play_voice = -1;
            detect_flag = 1;
            printf("AFE_FETCH_CHANNEL_VERIFIED, channel index: %d\n", res->trigger_channel_id);
            // afe_handle->disable_wakenet(afe_data);
            // afe_handle->disable_aec(afe_data);
        }

        if (detect_flag == 1)
        {
            esp_mn_state_t mn_state = multinet->detect(model_data, res->data);

            if (mn_state == ESP_MN_STATE_DETECTING)
            {
                continue;
            }

            if (mn_state == ESP_MN_STATE_DETECTED)
            {
                esp_mn_results_t *mn_result = multinet->get_results(model_data);
                for (int i = 0; i < mn_result->num; i++)
                {
                    printf("TOP %d, command_id: %d, phrase_id: %d, string: %s, prob: %f\n",
                           i + 1, mn_result->command_id[i], mn_result->phrase_id[i], mn_result->string, mn_result->prob[i]);
                    if (i == 0)
                    {
                        play_voice = mn_result->command_id[i];
                    }
                }
                printf("-----------listening-----------\n");
            }

            if (mn_state == ESP_MN_STATE_TIMEOUT)
            {
                esp_mn_results_t *mn_result = multinet->get_results(model_data);
                printf("timeout, string:%s\n", mn_result->string);
                afe_handle->enable_wakenet(afe_data);
                detect_flag = 0;
                printf("\n-----------awaits to be waken up-----------\n");
                continue;
            }
        }
    }
    if (model_data)
    {
        multinet->destroy(model_data);
        model_data = NULL;
    }
    printf("detect exit\n");
    vTaskDelete(NULL);
}

void app_main()
{
    models = esp_srmodel_init("model"); // partition label defined in partitions.csv
    ESP_ERROR_CHECK(esp_board_init(8000, 2, 16));
	esp_audio_set_play_vol(100);
    // ESP_ERROR_CHECK(esp_sdcard_init("/sdcard", 10));

#if defined  CONFIG_ESP32_S3_KORVO_2_V3_0_BOARD
	esp_err_t err = pca9555_init(I2C_NUM_0,
			0xff &
			(
				(0 << PCA8555_AUDIO_OUT_PIN) | 
				(0 << PCA8555_GREEN_OUT_PIN) | 
				(0 << PCA8555_BLUE_OUT_PIN)
			), 0);
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

#if CONFIG_IDF_TARGET_ESP32
    printf("This demo only support ESP32S3\n");
    return;
#else
    afe_handle = (esp_afe_sr_iface_t *)&ESP_AFE_SR_HANDLE;
#endif

    afe_config_t afe_config = AFE_CONFIG_DEFAULT();
    afe_config.wakenet_model_name = esp_srmodel_filter(models, ESP_WN_PREFIX, NULL);
    ;
#if CONFIG_ESP32_S3_EYE_BOARD || CONFIG_ESP32_P4_FUNCTION_EV_BOARD
    afe_config.pcm_config.total_ch_num = 2;
    afe_config.pcm_config.mic_num = 1;
    afe_config.pcm_config.ref_num = 1;
    afe_config.wakenet_mode = DET_MODE_90;
    afe_config.se_init = false;
#endif
    esp_afe_sr_data_t *afe_data = afe_handle->create_from_config(&afe_config);

    task_flag = 1;
    xTaskCreatePinnedToCore(&detect_Task, "detect", 8 * 1024, (void *)afe_data, 5, NULL, 1);
    xTaskCreatePinnedToCore(&feed_Task, "feed", 8 * 1024, (void *)afe_data, 5, NULL, 0);
#if defined CONFIG_ESP32_S3_KORVO_1_V4_0_BOARD
    xTaskCreatePinnedToCore(&led_Task, "led", 3 * 1024, NULL, 5, NULL, 0);
#endif
#if defined CONFIG_ESP32_S3_KORVO_1_V4_0_BOARD || CONFIG_ESP32_S3_KORVO_2_V3_0_BOARD || CONFIG_ESP32_KORVO_V1_1_BOARD || CONFIG_ESP32_S3_BOX_BOARD
    xTaskCreatePinnedToCore(&play_music, "play", 4 * 1024, NULL, 5, NULL, 1);
//	xTaskCreatePinnedToCore(&led_Task, "flash", 4 * 1024, NULL, 5, NULL, 0);
#endif

    // // You can call afe_handle->destroy to destroy AFE.
    // task_flag = 0;

    // printf("destroy\n");
    // afe_handle->destroy(afe_data);
    // afe_data = NULL;
    // printf("successful\n");
}
