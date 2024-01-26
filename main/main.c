#include "common/config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "io/gate.h"
#include "io/io.h"
#include "io/ui_handler.h"
#include "wireless/wifi.h"
#include <stdio.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "drivers/i2c.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>

TaskHandle_t tcp_server_task_handle = NULL;
TaskHandle_t wing_action_task_handle = NULL;
TaskHandle_t right_wing_task_handle = NULL;
TaskHandle_t left_wing_task_handle = NULL;

TaskHandle_t ui_handler_button_task_handle = NULL;
TaskHandle_t ui_handler_oled_display_task_handle = NULL;

void app_main(void)
{
    esp_log_level_set("event", ESP_LOG_NONE);
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(nvs_flash_init_partition("nvs_ext"));
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    config_load();

    io_init();
    io_handler_init();

    init_i2c();

    wing_module_init();
    xTaskCreatePinnedToCore(wing_action_task, "wing_action_task", 4096, NULL, configMAX_PRIORITIES - 1, &wing_action_task_handle, APP_CPU_NUM);
    xTaskCreatePinnedToCore(wing_task, "right_wing_task", 4096, (void *)RIGHT_WING, configMAX_PRIORITIES - 1, &right_wing_task_handle, APP_CPU_NUM);
    xTaskCreatePinnedToCore(wing_task, "left_wing_task", 4096, (void *)LEFT_WING, configMAX_PRIORITIES - 1, &left_wing_task_handle, APP_CPU_NUM);

    wifi_config_init();
    xTaskCreatePinnedToCore(tcp_server_task, "tcp_server", 4096, NULL, 5, &tcp_server_task_handle, PRO_CPU_NUM);

    init_i2c_oled();
    if (i2c_oled_initialized())
    {
        ui_handler_init();
        xTaskCreatePinnedToCore(ui_button_handling_task, "button_task", 4096, NULL, configMAX_PRIORITIES - 6, &ui_handler_button_task_handle, APP_CPU_NUM);
        xTaskCreatePinnedToCore(ui_oled_display_task, "display_task", 16384, NULL, configMAX_PRIORITIES - 6, &ui_handler_oled_display_task_handle, APP_CPU_NUM);
    }

    ghota_config_init();
    mqtt_config_init();

    io_buzzer(1, 50, 100);
    ESP_LOGI("MAIN", "INIT DONE");
    while (1)
    {
        ESP_LOGI("MAIN", "%i,%i", uxTaskGetStackHighWaterMark(ui_handler_button_task_handle), uxTaskGetStackHighWaterMark(ui_handler_oled_display_task_handle));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}