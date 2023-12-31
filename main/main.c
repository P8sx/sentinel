#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "common/config.h"
#include "io/io.h"
#include "wireless/wifi.h"
#include "io/control.h"

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

TaskHandle_t tcp_server_task_handle = NULL;
TaskHandle_t motor_action_task_handle = NULL;
TaskHandle_t motor_m1_task_handle = NULL;
TaskHandle_t motor_m2_task_handle = NULL;

device_config_t device_config;
void device_config_init(){
    atomic_init(&(device_config.m1_dir),false);
    atomic_init(&(device_config.m2_dir),false);

    atomic_init(&(device_config.m1_ocp_treshold), 1100);
    atomic_init(&(device_config.m1_ocp_count), 150);

    atomic_init(&(device_config.m2_ocp_treshold), 1100);
    atomic_init(&(device_config.m2_ocp_count), 150);

}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(nvs_flash_init_partition("nvs_ext"));
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    device_config_init();

    io_init_outputs();
    io_init_inputs();
    io_init_pwm();
    io_init_pcnt();
    io_init_analog();
    
    control_motor_init();
    
    wifi_init();
    init_i2c();
	init_i2c_oled();

    /* All control critical tasks are handled by core 1 everything else like wifi/oled/user action by core 0 */
    xTaskCreatePinnedToCore(tcp_server_task, "tcp_server", 4096, NULL, 5, &tcp_server_task_handle, PRO_CPU_NUM);

    xTaskCreatePinnedToCore(motor_action_task, "motor_action_task", 4096, NULL, configMAX_PRIORITIES - 1, &motor_action_task_handle, APP_CPU_NUM);
    xTaskCreatePinnedToCore(motor_task, "motor_m1_task", 4096, (void *)M1, configMAX_PRIORITIES - 1, &motor_m1_task_handle, APP_CPU_NUM);
    xTaskCreatePinnedToCore(motor_task, "motor_m2_task", 4096, (void *)M2, configMAX_PRIORITIES - 1, &motor_m2_task_handle, APP_CPU_NUM);
    
    io_buzzer(1,50,100);
    ESP_LOGI("MAIN","INIT DONE");
    while(1){
        // uint64_t start = esp_timer_get_time();
        // int16_t adc = io_motor_get_current(M1);
        // uint64_t diff = esp_timer_get_time() - start;
        // ESP_LOGI("MAIN", "time:%lu, adc:%i",(long)diff, (int)adc);

        // ESP_LOGI("MAIN", "M1 PCNT:%i, M2 PCNT:%i\n", (int)io_get_pcnt_m1(),(int)io_get_pcnt_m2());
        // ESP_LOGI("MAIN", "M1 ANALOG:%i, M2 ANALOG:%i\n", (int)io_motor_get_current(M1),(int)io_motor_get_current(M2));
        // ESP_LOGI("MAIN", "M1:%s PCNT:%i ANALOG %i, M2:%s PCNT:%i ANALOG %i",STATES_STRING[control_get_motor_state(M1)], (int)io_get_pcnt_m1(), (int)io_get_analog_m1(), STATES_STRING[control_get_motor_state(M2)], (int)io_get_pcnt_m2(), (int)io_get_analog_m2());
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}