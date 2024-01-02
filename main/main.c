#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "common/config.h"
#include "io/io.h"
#include "io/control.h"
#include "wireless/wifi.h"
#include "wireless/rf.h"

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
#include <esp_ghota.h>

TaskHandle_t tcp_server_task_handle = NULL;
TaskHandle_t motor_action_task_handle = NULL;
TaskHandle_t motor_m1_task_handle = NULL;
TaskHandle_t motor_m2_task_handle = NULL;

/* Default configuration */
device_config_t device_config = {
    .m1_dir = false,
    .m2_dir = false,
    .m1_ocp_treshold = 1100,
    .m2_ocp_treshold = 1200,
    .m1_ocp_count = 150,
    .m2_ocp_count = 150,
};


static void ghota_event_callback(void* handler_args, esp_event_base_t base, int32_t id, void* event_data) {
    ghota_client_handle_t *client = (ghota_client_handle_t *)handler_args;
    ESP_LOGI(GHOTA_LOG_TAG, "Got Update Callback: %s", ghota_get_event_str(id));
    if (id == GHOTA_EVENT_START_STORAGE_UPDATE) {
        ESP_LOGI(GHOTA_LOG_TAG, "Starting storage update");
        /* if we are updating the SPIFF storage we should unmount it */
    } else if (id == GHOTA_EVENT_FINISH_STORAGE_UPDATE) {
        ESP_LOGI(GHOTA_LOG_TAG, "Ending storage update");
        /* after updating we can remount, but typically the device will reboot shortly after recieving this event. */
    } else if (id == GHOTA_EVENT_FIRMWARE_UPDATE_PROGRESS) {
        /* display some progress with the firmware update */
        ESP_LOGI(GHOTA_LOG_TAG, "Firmware Update Progress: %d%%", *((int*) event_data));
    } else if (id == GHOTA_EVENT_STORAGE_UPDATE_PROGRESS) {
        /* display some progress with the spiffs partition update */
        ESP_LOGI(GHOTA_LOG_TAG, "Storage Update Progress: %d%%", *((int*) event_data));
    }
    (void)client;
    return;
}

#if defined(WIFI_SSID) && defined(WIFI_PASSWORD)
void save_config(){
    nvs_handle_t nvs_handle;
    device_config_t memory_device_config = {
        .wifi_ssid = WIFI_SSID,
        .wifi_password = WIFI_PASSWORD,
        .m1_dir = false,
        .m2_dir = false,
        .m1_ocp_treshold = 1100,
        .m2_ocp_treshold = 1200,
        .m1_ocp_count = 150,
        .m2_ocp_count = 150,
    };
    ESP_ERROR_CHECK(nvs_open_from_partition("nvs_ext","cfg-nmspace", NVS_READWRITE, &nvs_handle));
    ESP_ERROR_CHECK(nvs_set_blob(nvs_handle, "config", &memory_device_config, sizeof(memory_device_config)));
    ESP_ERROR_CHECK(nvs_commit(nvs_handle));
    nvs_close(nvs_handle);
}
#endif

void config_init(){
    nvs_handle_t nvs_handle;
    device_config_t memory_device_config;
    size_t size = sizeof(device_config_t);
    ESP_ERROR_CHECK(nvs_open_from_partition("nvs_ext","cfg-nmspace", NVS_READWRITE, &nvs_handle));
    ESP_ERROR_CHECK(nvs_get_blob(nvs_handle, "config", &memory_device_config, &size));
    nvs_close(nvs_handle);
    memcpy(&device_config, &memory_device_config, size);
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(nvs_flash_init_partition("nvs_ext"));
#if defined(WIFI_SSID) && defined(WIFI_PASSWORD)
    save_config();
#endif
    config_init();

    ESP_ERROR_CHECK(esp_event_loop_create_default());

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
    rf433_init();
    
    io_buzzer(1,50,100);


    ghota_config_t ghconfig = {
        .filenamematch = "sentinel-esp32s3.bin",
        .storagenamematch = "storage-esp32.bin",
        .updateInterval = 1,
    };
    ghota_client_handle_t *ghota_client = ghota_init(&ghconfig);
    if (ghota_client == NULL) {
        ESP_LOGE(GHOTA_LOG_TAG, "ghota_client_init failed");
        return;
    }
    esp_event_handler_register(GHOTA_EVENTS, ESP_EVENT_ANY_ID, &ghota_event_callback, ghota_client);
    ESP_ERROR_CHECK(ghota_start_update_timer(ghota_client));
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