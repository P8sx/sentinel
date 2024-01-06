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

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "drivers/i2c.h"
#include "io/control.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include <string.h>


TaskHandle_t tcp_server_task_handle = NULL;
TaskHandle_t motor_action_task_handle = NULL;
TaskHandle_t gate_m1_task_handle = NULL;
TaskHandle_t gate_m2_task_handle = NULL;
TaskHandle_t control_input_task_handle = NULL;

void app_main(void)
{
    esp_log_level_set("event", ESP_LOG_NONE);
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(nvs_flash_init_partition("nvs_ext"));
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    config_init();

    io_init_outputs();
    io_init_inputs();
    io_init_pwm();
    io_init_pcnt();
    io_init_analog();
    
    gate_motor_init();
    control_init();

    wifi_init();

    init_i2c();
	init_i2c_oled();
    
    /* All control critical tasks are handled by core 1 everything else like wifi/oled/user action by core 0 */
    xTaskCreatePinnedToCore(tcp_server_task, "tcp_server", 4096, NULL, 5, &tcp_server_task_handle, PRO_CPU_NUM);
    
    xTaskCreatePinnedToCore(gate_action_task, "gate_action_task", 4096, NULL, configMAX_PRIORITIES - 1, &motor_action_task_handle, APP_CPU_NUM);
    xTaskCreatePinnedToCore(gate_task, "gate_m1_task", 4096, (void *)M1, configMAX_PRIORITIES - 1, &gate_m1_task_handle, APP_CPU_NUM);
    xTaskCreatePinnedToCore(gate_task, "gate_m2_task", 4096, (void *)M2, configMAX_PRIORITIES - 1, &gate_m2_task_handle, APP_CPU_NUM);

    xTaskCreatePinnedToCore(control_input_task, "control_input_task", 4096, NULL, 5, &control_input_task_handle, PRO_CPU_NUM);
    
    rf433_init();
    ota_init();


    io_buzzer(1,50,100);
    ESP_LOGI("MAIN","INIT DONE");
    while(1){
        // ESP_LOGI("MAIN","%i:%i:%i",gpio_get_level(BTN1_PIN),gpio_get_level(BTN2_PIN),gpio_get_level(BTN3_PIN));
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(BUZZER_PIN, 0);
    }
}