#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "common/config.h"
#include "io/io.h"
#include "io/gate.h"
#include "wireless/wifi.h"
#include "wireless/rf.h"
#include "io/ui_handler.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "drivers/i2c.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include <string.h>

TaskHandle_t tcp_server_task_handle = NULL;
TaskHandle_t motor_action_task_handle = NULL;
TaskHandle_t gate_m1_task_handle = NULL;
TaskHandle_t gate_m2_task_handle = NULL;

TaskHandle_t ui_handler_button_task_handle = NULL;
TaskHandle_t ui_handler_oled_display_task_handle = NULL;

void app_main(void)
{
    esp_log_level_set("event", ESP_LOG_NONE);
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(nvs_flash_init_partition("nvs_ext"));
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    migrate_config();
    config_init();

    io_init();
    io_handler_init();

    init_i2c();

    gate_module_init();
    xTaskCreatePinnedToCore(gate_action_task, "gate_action_task", 4096, NULL, configMAX_PRIORITIES - 1, &motor_action_task_handle, APP_CPU_NUM);
    xTaskCreatePinnedToCore(gate_task, "gate_m1_task", 4096, (void *)M1, configMAX_PRIORITIES - 1, &gate_m1_task_handle, APP_CPU_NUM);
    xTaskCreatePinnedToCore(gate_task, "gate_m2_task", 4096, (void *)M2, configMAX_PRIORITIES - 1, &gate_m2_task_handle, APP_CPU_NUM);

    wifi_config_init();
    xTaskCreatePinnedToCore(tcp_server_task, "tcp_server", 4096, NULL, 5, &tcp_server_task_handle, PRO_CPU_NUM);


	init_i2c_oled();
    if(i2c_oled_initialized()){
        ui_handler_init();
        xTaskCreatePinnedToCore(ui_button_handling_task, "button_task", 2048, NULL, configMAX_PRIORITIES - 6, &ui_handler_button_task_handle, APP_CPU_NUM);
        xTaskCreatePinnedToCore(ui_oled_display_task, "display_task", 8192, NULL, configMAX_PRIORITIES - 6, &ui_handler_oled_display_task_handle, APP_CPU_NUM);
    }

    rf433_init();
    ghota_config_init();
    mqtt_config_init();

    io_buzzer(1,50,100);
    ESP_LOGI("MAIN","INIT DONE");
    while(1){
        
        ESP_LOGI("MAIN","%i,%i", uxTaskGetStackHighWaterMark(ui_handler_button_task_handle), uxTaskGetStackHighWaterMark(ui_handler_oled_display_task_handle));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}