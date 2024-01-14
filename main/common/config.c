#include "config.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "string.h"
#include "types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
/* Default configuration */


device_config_t device_config = {
    .m1_dir = false,
    .m2_dir = false,
    .m1_ocp_threshold = 1100,
    .m2_ocp_threshold = 1200,
    .m1_ocp_count = 150,
    .m2_ocp_count = 150,
    .input_actions = {M1M2_NEXT_STATE, UNKNOWN_ACTION, UNKNOWN_ACTION, UNKNOWN_ACTION},
};

void save_config(){
    nvs_handle_t nvs_handle;
    device_config_t memory_device_config = {
        .wifi_ssid = "test",
        .wifi_password = "test",
        .m1_dir = false,
        .m2_dir = false,
        .m1_ocp_threshold = 1100,
        .m2_ocp_threshold = 1200,
        .m1_ocp_count = 150,
        .m2_ocp_count = 150,
        .input_actions = {M1M2_NEXT_STATE, M1_STOP, M2_STOP, M1M2_STOP},
    };
    ESP_ERROR_CHECK(nvs_open_from_partition("nvs_ext","cfg-nmspace", NVS_READWRITE, &nvs_handle));
    ESP_ERROR_CHECK(nvs_set_blob(nvs_handle, "config", &memory_device_config, sizeof(memory_device_config)));
    ESP_ERROR_CHECK(nvs_commit(nvs_handle));
    nvs_close(nvs_handle);
}


void config_init(){
    nvs_handle_t nvs_handle;
    device_config_t memory_device_config;
    size_t size = sizeof(device_config_t);
    ESP_ERROR_CHECK(nvs_open_from_partition("nvs_ext","cfg-nmspace", NVS_READWRITE, &nvs_handle));
    ESP_ERROR_CHECK(nvs_get_blob(nvs_handle, "config", &memory_device_config, &size));
    nvs_close(nvs_handle);
    memcpy(&device_config, &memory_device_config, size);
}

void config_update_motor_settings(motor_id_t motor_id, bool dir, uint16_t ocp_threshold, uint16_t ocp_count){
    ESP_LOGI(CFG_LOG_TAG, "Config update start");
    nvs_handle_t nvs_handle;
    if(M1 == motor_id){
        device_config.m1_dir = dir;
        device_config.m1_ocp_threshold = ocp_threshold;
        device_config.m1_ocp_count = ocp_count;
    }
    else{
        device_config.m2_dir = dir;
        device_config.m2_ocp_threshold = ocp_threshold;
        device_config.m2_ocp_count = ocp_count;
    }
    ESP_ERROR_CHECK(nvs_open_from_partition("nvs_ext","cfg-nmspace", NVS_READWRITE, &nvs_handle));
    ESP_ERROR_CHECK(nvs_set_blob(nvs_handle, "config", &device_config, sizeof(device_config)));
    ESP_ERROR_CHECK(nvs_commit(nvs_handle));
    nvs_close(nvs_handle);
    ESP_LOGI(CFG_LOG_TAG, "Config updated");
}
