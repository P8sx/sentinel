#include "config.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "string.h"
#include "types.h"
/* Default configuration */
device_config_t device_config = {
    .m1_dir = false,
    .m2_dir = false,
    .m1_ocp_treshold = 1100,
    .m2_ocp_treshold = 1200,
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
        .m1_ocp_treshold = 1100,
        .m2_ocp_treshold = 1200,
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