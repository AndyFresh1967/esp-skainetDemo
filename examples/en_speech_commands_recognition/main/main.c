/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>

#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_board_init.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_process_sdkconfig.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "led_state.h"
#include "model_path.h"
#include "speech_commands_action.h"

#include "led_strip.h"
#include "led_strip_interface.h"
#include <driver/rmt_types_legacy.h>

#include "nvs.h"
#include "nvs_flash.h"

int detect_flag = 0;
static esp_afe_sr_iface_t *afe_handle = NULL;
static volatile int task_flag = 0;
srmodel_list_t *models = NULL;
static int play_voice = -2;

#define RMT_TX_CHANNEL 1 // Канал RMT для передачи данных
#define LED_GPIO_PIN 3   // Пин для подключения WS2812
#define LED_COUNT 1      // Количество светодиодов в ленте
#define LED_STRIP_RMT_RES_HZ (10 * 1000 * 1000)

led_strip_t *strip;
led_strip_handle_t led_play_handle;

typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} led_strip_color_t;

#define LED_STRIP_COLOR(r, g, b) ((led_strip_color_t){r, g, b})

static void set_led_color(led_strip_color_t color) {
  for (int i = 0; i < LED_COUNT; i++) {
    ESP_ERROR_CHECK(
        led_strip_set_pixel(led_play_handle, i, color.g, color.r, color.b));
  }
  ESP_ERROR_CHECK(led_strip_refresh(led_play_handle));
}

void set_led_strip_color(void *arg) {
  static led_strip_color_t color = {0, 0, 0};
  while (task_flag) {
    switch (play_voice) {
    case -2:
      vTaskDelay(10);
      break;
    case -1:
      wake_up_action();
      play_voice = -2;
      break;
    case 1:
      // turn on green
      color.g = 255;
      play_voice = -2;
      break;
    case 2:
      // turn off green
      color.g = 0;
      play_voice = -2;
      break;
    case 3:
      // turn on blue
      color.b = 255;
      play_voice = -2;
      break;
    case 4:
      // turn off blue
      color.b = 0;
      play_voice = -2;
      break;
    case 5:
      // turn on red
      color.r = 255;
      play_voice = -2;
      break;
    case 6:
      // turn off red
      color.r = 0;
      play_voice = -2;
      break;
    case 7:
      // vkluchi krasni
      color.r = 0;
      play_voice = -2;
      break;
    default:
      break;
    }
    set_led_color(color);
  }
  vTaskDelete(NULL);
}

void feed_Task(void *arg) {
  esp_afe_sr_data_t *afe_data = arg;
  //Получает размер блока данных для передачи в AFE.
  int audio_chunksize = afe_handle->get_feed_chunksize(afe_data);
  //Получает количество каналов в AFE.
  int nch = afe_handle->get_channel_num(afe_data);
  //Получает количество каналов доступных для обработки.
  int feed_channel = esp_get_feed_channel();
  //Проверка согласованости
  assert(nch <= feed_channel);
  //Выделяет память для буфера аудио данных.
  int16_t *i2s_buff = malloc(audio_chunksize * sizeof(int16_t) * feed_channel);
  assert(i2s_buff);

  while (task_flag) {
    //Получает данные из потока данных.
    esp_get_feed_data(false, i2s_buff,
                      audio_chunksize * sizeof(int16_t) * feed_channel);
    //Передает данные в AFE.
    afe_handle->feed(afe_data, i2s_buff);
  }
  if (i2s_buff) {
    free(i2s_buff);
    i2s_buff = NULL;
  }
  vTaskDelete(NULL);
}

void detect_Task(void *arg) {
  //Данные от AFE
  esp_afe_sr_data_t *afe_data = arg;
  int afe_chunksize = afe_handle->get_fetch_chunksize(afe_data);
  //Получает имя модели MultiNet для английского языка с помощью esp_srmodel_filter.
  char *mn_name = esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_ENGLISH);
  printf("multinet:%s\n", mn_name);
  //Получает обработчик MultiNet для английского языка.
  esp_mn_iface_t *multinet = esp_mn_handle_from_name(mn_name);
  model_iface_data_t *model_data = multinet->create(mn_name, 6000);
  int mu_chunksize = multinet->get_samp_chunksize(model_data);
  esp_mn_commands_update_from_sdkconfig(
      multinet, model_data);
  //проверка согласованности MultiNet и AFE
  assert(mu_chunksize == afe_chunksize);
  // print active speech commands
  multinet->print_active_speech_commands(model_data);

  printf("------------detect start------------\n");
  while (task_flag) {
    //Извлекает данные из AFE.
    afe_fetch_result_t *res = afe_handle->fetch(afe_data);
    if (!res || res->ret_value == ESP_FAIL) {
      printf("fetch error!\n");
      break;
    }
    //Проверяет, было ли обнаружено слово-ключевое слово.
    if (res->wakeup_state == WAKENET_DETECTED) {
      state_led_speech_recognition(1);
      printf("WAKEWORD DETECTED\n");
      multinet->clean(model_data);
    } else if (res->wakeup_state == WAKENET_CHANNEL_VERIFIED) {
      play_voice = -1;
      detect_flag = 1;
      printf("AFE_FETCH_CHANNEL_VERIFIED, channel index: %d\n",
             res->trigger_channel_id);
      // afe_handle->disable_wakenet(afe_data);
      // afe_handle->disable_aec(afe_data);
    }

    if (detect_flag == 1) {
      //выполняем распознавание команды
      esp_mn_state_t mn_state = multinet->detect(model_data, res->data);

      if (mn_state == ESP_MN_STATE_DETECTING) {
        continue;
      }
      //команда распознана
      if (mn_state == ESP_MN_STATE_DETECTED) {
        //какая конкретная команда
        esp_mn_results_t *mn_result = multinet->get_results(model_data);
        for (int i = 0; i < mn_result->num; i++) {
          printf(
              "TOP %d, command_id: %d, phrase_id: %d, string: %s, prob: %f\n",
              i + 1, mn_result->command_id[i], mn_result->phrase_id[i],
              mn_result->string, mn_result->prob[i]);
          if (i == 0) {
            play_voice = mn_result->command_id[i];
          }
        }
        printf("-----------listening-----------\n");
      }

      if (mn_state == ESP_MN_STATE_TIMEOUT) {
        esp_mn_results_t *mn_result = multinet->get_results(model_data);
        printf("timeout, string:%s\n", mn_result->string);
        afe_handle->enable_wakenet(afe_data);
        detect_flag = 0;
        state_led_speech_recognition(0);
        printf("\n-----------awaits to be waken up-----------\n");
        continue;
      }
    }
  }
  if (model_data) {
    multinet->destroy(model_data);
    model_data = NULL;
  }
  printf("detect exit\n");
  vTaskDelete(NULL);
}

static led_strip_handle_t init_ws2812() {
  led_strip_config_t strip_config = {
      .strip_gpio_num = LED_GPIO_PIN,
      .max_leds = LED_COUNT,
      .led_pixel_format = LED_PIXEL_FORMAT_GRB,
      .led_model = LED_MODEL_WS2812,
      .flags.invert_out = false, // whether to invert the output signal
  };

  led_strip_rmt_config_t rmt_config = {
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
      .rmt_channel = 0,
#else
      .clk_src = RMT_CLK_SRC_DEFAULT, // different clock source can lead to
                                      // different power consumption
      .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
      .flags.with_dma =
          false, // DMA feature is available on ESP target like ESP32-S3
#endif
  };

  led_strip_handle_t led_strip;
  ESP_ERROR_CHECK(
      led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
  return led_strip;
}

void led_strip_control_task(void *pvParameters) {
  led_strip_set_pixel(led_play_handle, 0, 255, 255, 255);
  led_strip_refresh(led_play_handle);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  vTaskDelete(NULL);
}

/// @brief Проверка причины перезагрузки
void reboot_reason_check() {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  // Открытие NVS хранилища
  nvs_handle_t nvs_handle;
  ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &nvs_handle));

  // Чтение счетчика перезагрузок
  uint32_t reboot_count = 0;
  err = nvs_get_u32(nvs_handle, "reboot_count", &reboot_count);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    reboot_count = 0; // Если ключ не найден, начинаем с 0
  }

  // Проверка причины перезагрузки
  esp_reset_reason_t reset_reason = esp_reset_reason();
  if (reset_reason == ESP_RST_POWERON) {
    printf("Power-on reset detected. Resetting reboot counter.\n");
    reboot_count = 0;
    ESP_ERROR_CHECK(nvs_set_u32(nvs_handle, "reboot_count", reboot_count));
    ESP_ERROR_CHECK(nvs_commit(nvs_handle));
  }

  // Увеличение счетчика
  reboot_count++;
  printf("Reboot count: %d\n", reboot_count);

  // Сохранение обновленного значения в NVS
  ESP_ERROR_CHECK(nvs_set_u32(nvs_handle, "reboot_count", reboot_count));
  ESP_ERROR_CHECK(nvs_commit(nvs_handle));
  nvs_close(nvs_handle);

  // Проверка, достиг ли счетчик 3
  if (reboot_count >= 3) {
    printf("Reboot limit reached. Halting the chip.\n");

    esp_deep_sleep_start(); // Остановка чипа
  }
}

void app_main() {
  reboot_reason_check();
  //Инициализирует аппаратные компоненты, такие как аудиоподсистема и т.п.
  ESP_ERROR_CHECK(esp_board_init(8000, 2, 16));
  ESP_ERROR_CHECK(state_led_init());

  led_play_handle = init_ws2812();
  ESP_ERROR_CHECK(led_play_handle != NULL ? ESP_OK : ESP_FAIL);
  ESP_ERROR_CHECK(led_strip_clear(led_play_handle));
  state_led_ready(0);

  //Читаем модели (по сути веса) из флаш-памяти
  models =
      esp_srmodel_init("model"); // partition label defined in partitions.csv
  esp_audio_set_play_vol(100);

  afe_handle = (esp_afe_sr_iface_t *)&ESP_AFE_SR_HANDLE;

  afe_config_t afe_config = AFE_CONFIG_DEFAULT();
  //находим модель по префиксу ESP_WN_PREFIX, это модель для WakeNet
  afe_config.wakenet_model_name =
      esp_srmodel_filter(models, ESP_WN_PREFIX, NULL);
  //Создаёт конфигурацию по умолчанию для AFE.
  esp_afe_sr_data_t *afe_data = afe_handle->create_from_config(&afe_config);

  task_flag = 1;
  //Считывает аудиоданные с микрофона. Передаёт эти данные в Acoustic Front-End (AFE) для обработки.
  xTaskCreatePinnedToCore(&feed_Task, "feed", 8 * 1024, (void *)afe_data, 5,
                          NULL, 0);
  //Получает данные от AFE обнаружение ключевого слова и распонавание команд
  xTaskCreatePinnedToCore(&detect_Task, "detect", 8 * 1024, (void *)afe_data, 5,
                          NULL, 1);
  //Выполняет команды
  xTaskCreatePinnedToCore(&set_led_strip_color, "play", 4 * 1024, NULL, 5, NULL,
                          1);
  state_led_ready(1);
  //мигнуть светодиодами при запуске
  xTaskCreatePinnedToCore(&led_strip_control_task, "led_strip", 4 * 1024, NULL,
                          5, NULL, 1);
}
