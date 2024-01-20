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
    .output_actions = {M1M2_MOVING_ON, M1M2_MOVING_BLINK},
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
        .output_actions = {M1M2_MOVING_ON, M1M2_MOVING_BLINK},
    };
    ESP_ERROR_CHECK(nvs_open_from_partition("nvs_ext","cfg-nmspace", NVS_READWRITE, &nvs_handle));

    ESP_ERROR_CHECK(nvs_set_blob(nvs_handle, "wifi_ssid", &memory_device_config.wifi_ssid, sizeof(memory_device_config.wifi_ssid)));
    ESP_ERROR_CHECK(nvs_set_blob(nvs_handle, "wifi_password", &memory_device_config.wifi_password, sizeof(memory_device_config.wifi_password)));

    ESP_ERROR_CHECK(nvs_set_u8(nvs_handle, "m1_dir", memory_device_config.m1_dir));
    ESP_ERROR_CHECK(nvs_set_u8(nvs_handle, "m2_dir", memory_device_config.m2_dir));
    ESP_ERROR_CHECK(nvs_set_u16(nvs_handle, "m1_ocp_thr", memory_device_config.m1_ocp_threshold));
    ESP_ERROR_CHECK(nvs_set_u16(nvs_handle, "m2_ocp_thr", memory_device_config.m2_ocp_threshold));
    ESP_ERROR_CHECK(nvs_set_u16(nvs_handle, "m1_ocp_count", memory_device_config.m1_ocp_count));
    ESP_ERROR_CHECK(nvs_set_u16(nvs_handle, "m1_ocp_count", memory_device_config.m2_ocp_count));

    ESP_ERROR_CHECK(nvs_set_blob(nvs_handle, "input_actions", &memory_device_config.input_actions, sizeof(memory_device_config.input_actions)));
    ESP_ERROR_CHECK(nvs_set_blob(nvs_handle, "output_actions", &memory_device_config.output_actions, sizeof(memory_device_config.output_actions)));


    ESP_ERROR_CHECK(nvs_commit(nvs_handle));
    nvs_close(nvs_handle);
}

void migrate_config(){
    nvs_handle_t nvs_handle;
    device_config_t memory_device_config = {0};
    size_t size = sizeof(device_config_t);
    ESP_ERROR_CHECK(nvs_open_from_partition("nvs_ext","cfg-nmspace", NVS_READWRITE, &nvs_handle));

    if(nvs_get_blob(nvs_handle, "config", &memory_device_config, &size) != ESP_OK){
        nvs_close(nvs_handle);
        ESP_LOGI(CFG_LOG_TAG, "Migration already done");
        return;
    }

    ESP_ERROR_CHECK(nvs_set_blob(nvs_handle, "wifi_ssid", &memory_device_config.wifi_ssid, sizeof(memory_device_config.wifi_ssid)));
    ESP_ERROR_CHECK(nvs_set_blob(nvs_handle, "wifi_password", &memory_device_config.wifi_password, sizeof(memory_device_config.wifi_password)));

    ESP_ERROR_CHECK(nvs_set_u8(nvs_handle, "m1_dir", (uint8_t)memory_device_config.m1_dir));
    ESP_ERROR_CHECK(nvs_set_u16(nvs_handle, "m1_ocp_thr", memory_device_config.m1_ocp_threshold));
    ESP_ERROR_CHECK(nvs_set_u16(nvs_handle, "m1_ocp_count", memory_device_config.m1_ocp_count));

    ESP_ERROR_CHECK(nvs_set_u8(nvs_handle, "m2_dir", (uint8_t)memory_device_config.m2_dir));
    ESP_ERROR_CHECK(nvs_set_u16(nvs_handle, "m2_ocp_thr", memory_device_config.m2_ocp_threshold));
    ESP_ERROR_CHECK(nvs_set_u16(nvs_handle, "m2_ocp_count", memory_device_config.m2_ocp_count));

    ESP_ERROR_CHECK(nvs_set_blob(nvs_handle, "input_actions", &memory_device_config.input_actions, sizeof(memory_device_config.input_actions)));
    ESP_ERROR_CHECK(nvs_set_blob(nvs_handle, "output_actions", &memory_device_config.output_actions, sizeof(memory_device_config.output_actions)));

    ESP_ERROR_CHECK(nvs_erase_key(nvs_handle, "config"));
    ESP_ERROR_CHECK(nvs_commit(nvs_handle));
    nvs_close(nvs_handle);
}

void config_init(){
    nvs_handle_t nvs_handle;
    device_config_t memory_device_config;
    size_t size = 0;

    ESP_ERROR_CHECK(nvs_open_from_partition("nvs_ext","cfg-nmspace", NVS_READONLY, &nvs_handle));
    size = sizeof(memory_device_config.wifi_ssid);
    ESP_ERROR_CHECK(nvs_get_blob(nvs_handle, "wifi_ssid", &memory_device_config.wifi_ssid, &size));
    size = sizeof(memory_device_config.wifi_password);
    ESP_ERROR_CHECK(nvs_get_blob(nvs_handle, "wifi_password", &memory_device_config.wifi_password, &size));

    ESP_ERROR_CHECK(nvs_get_u8(nvs_handle, "m1_dir", (uint8_t *)&memory_device_config.m1_dir));
    ESP_ERROR_CHECK(nvs_get_u16(nvs_handle, "m1_ocp_count", &memory_device_config.m1_ocp_count));
    ESP_ERROR_CHECK(nvs_get_u16(nvs_handle, "m1_ocp_thr", &memory_device_config.m1_ocp_threshold));

    ESP_ERROR_CHECK(nvs_get_u8(nvs_handle, "m2_dir", (uint8_t *)&memory_device_config.m2_dir));
    ESP_ERROR_CHECK(nvs_get_u16(nvs_handle, "m2_ocp_thr", &memory_device_config.m2_ocp_threshold));
    ESP_ERROR_CHECK(nvs_get_u16(nvs_handle, "m2_ocp_count", &memory_device_config.m2_ocp_count));

    size = sizeof(memory_device_config.input_actions);
    ESP_ERROR_CHECK(nvs_get_blob(nvs_handle, "input_actions", &memory_device_config.input_actions, &size));
    size = sizeof(memory_device_config.output_actions);
    ESP_ERROR_CHECK(nvs_get_blob(nvs_handle, "output_actions", &memory_device_config.output_actions, &size));  


    nvs_close(nvs_handle);
    memcpy(&device_config, &memory_device_config, sizeof(device_config_t));
}

void config_update_motor_settings(motor_id_t motor_id, bool dir, uint16_t ocp_threshold, uint16_t ocp_count){
    ESP_LOGI(CFG_LOG_TAG, "Config update start");
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open_from_partition("nvs_ext","cfg-nmspace", NVS_READWRITE, &nvs_handle));
    if(M1 == motor_id){
        ESP_ERROR_CHECK(nvs_set_u8(nvs_handle, "m1_dir", (uint8_t)dir));
        ESP_ERROR_CHECK(nvs_set_u16(nvs_handle, "m1_ocp_thr", ocp_threshold));
        ESP_ERROR_CHECK(nvs_set_u16(nvs_handle, "m1_ocp_count", ocp_count));
    }
    else{
        ESP_ERROR_CHECK(nvs_set_u8(nvs_handle, "m2_dir", (uint8_t)dir));
        ESP_ERROR_CHECK(nvs_set_u16(nvs_handle, "m2_ocp_thr", ocp_threshold));
        ESP_ERROR_CHECK(nvs_set_u16(nvs_handle, "m2_ocp_count", ocp_count));
    }
    ESP_ERROR_CHECK(nvs_commit(nvs_handle));
    nvs_close(nvs_handle);

    ESP_LOGI(CFG_LOG_TAG, "Config updated");
}
