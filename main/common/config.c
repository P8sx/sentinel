#include "config.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "string.h"
#include "types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_app_desc.h"
/* Default configuration */

device_config_t device_config;
rf_remote_config_t remotes_config[STATIC_CFG_NUM_OF_REMOTES];

void load_config_value(nvs_handle_t nvs_handle, const char *key, void *value, size_t size)
{
    if (ESP_OK != nvs_get_blob(nvs_handle, key, value, &size))
    {
        // Wartość nie została znaleziona, więc ustawiamy domyślną wartość
        ESP_LOGI(CFG_LOG_TAG, "%s - not found, initializing value with default", key);
        ESP_ERROR_CHECK(nvs_set_blob(nvs_handle, key, value, size));
    }
}

void save_config_value(nvs_handle_t nvs_handle, const char *key, void *value, size_t size)
{
    nvs_set_blob(nvs_handle, key, value, size);
}
void save_config_value_without_handle(const char *key, void *value, size_t size)
{
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open_from_partition("nvs_ext", CFG_NAMESPACE, NVS_READWRITE, &nvs_handle));
    nvs_set_blob(nvs_handle, key, value, size);
    ESP_ERROR_CHECK(nvs_commit(nvs_handle));
    nvs_close(nvs_handle);
}
void config_load()
{
    nvs_handle_t nvs_handle;
    device_config_t initial_device_config = {
        .wifi_ssid = "",
        .wifi_password = "",
        .right_wing_dir = false,
        .left_wing_dir = false,
        .right_wing_ocp_threshold = 1100,
        .left_wing_ocp_threshold = 1200,
        .right_wing_ocp_count = 150,
        .left_wing_ocp_count = 150,
        .input_actions = {BOTH_WING_NEXT_STATE, RIGHT_WING_STOP, LEFT_WING_STOP, BOTH_WING_STOP},
        .output_actions = {ANY_WING_ON, ANY_WING_BLINK},
        .mqtt_uri = "",
        .mqtt_username = "",
        .mqtt_password = "",
        .mqtt_port = 1883,
        .hw_options = 0,
    };
    rf_remote_config_t initial_rf_config[STATIC_CFG_NUM_OF_REMOTES] = {0};

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(initial_device_config.device_name, sizeof(initial_device_config.device_name), "sentinel-%02x%02x%02x", mac[3], mac[4], mac[5]);
    const esp_app_desc_t *desc = esp_app_get_description();
    snprintf(initial_device_config.sw_version, sizeof(initial_device_config.sw_version), "%s", desc->version);
    snprintf(initial_device_config.hw_version, sizeof(initial_device_config.hw_version), "%s", HW_VERSION);

    memcpy(&device_config, &initial_device_config, sizeof(device_config_t));
    ESP_ERROR_CHECK(nvs_open_from_partition("nvs_ext", CFG_NAMESPACE, NVS_READWRITE, &nvs_handle));
    /* Read WIFI credentials */
    load_config_value(nvs_handle, CFG_WIFI_SSID, &initial_device_config.wifi_ssid, sizeof(initial_device_config.wifi_ssid));

    /* Read WIFI password */
    load_config_value(nvs_handle, CFG_WIFI_PASSWORD, &initial_device_config.wifi_password, sizeof(initial_device_config.wifi_password));

    /* Read wing configuration */
    load_config_value(nvs_handle, CFG_RIGHT_WING_DIR, &initial_device_config.right_wing_dir, sizeof(initial_device_config.right_wing_dir));
    load_config_value(nvs_handle, CFG_RIGHT_WING_OCP_COUNT, &initial_device_config.right_wing_ocp_count, sizeof(initial_device_config.right_wing_ocp_count));
    load_config_value(nvs_handle, CFG_RIGHT_WING_OCP_THRESHOLD, &initial_device_config.right_wing_ocp_threshold, sizeof(initial_device_config.right_wing_ocp_threshold));

    load_config_value(nvs_handle, CFG_LEFT_WING_DIR, &initial_device_config.left_wing_dir, sizeof(initial_device_config.left_wing_dir));
    load_config_value(nvs_handle, CFG_LEFT_WING_OCP_COUNT, &initial_device_config.left_wing_ocp_count, sizeof(initial_device_config.left_wing_ocp_count));
    load_config_value(nvs_handle, CFG_LEFT_WING_OCP_THRESHOLD, &initial_device_config.left_wing_ocp_threshold, sizeof(initial_device_config.left_wing_ocp_threshold));

    /* Read input/output configuration */
    load_config_value(nvs_handle, CFG_INPUT_ACTIONS, &initial_device_config.input_actions, sizeof(initial_device_config.input_actions));
    load_config_value(nvs_handle, CFG_OUTPUT_ACTIONS, &initial_device_config.output_actions, sizeof(initial_device_config.output_actions));

    /* Read MQTT credentials */
    load_config_value(nvs_handle, CFG_MQTT_URI, &initial_device_config.mqtt_uri, sizeof(initial_device_config.mqtt_uri));
    load_config_value(nvs_handle, CFG_MQTT_PASSWORD, &initial_device_config.mqtt_password, sizeof(initial_device_config.mqtt_password));
    load_config_value(nvs_handle, CFG_MQTT_USERNAME, &initial_device_config.mqtt_username, sizeof(initial_device_config.mqtt_username));
    load_config_value(nvs_handle, CFG_MQTT_PORT, &initial_device_config.mqtt_port, sizeof(initial_device_config.mqtt_port));

    /* HW options */
    load_config_value(nvs_handle, CFG_HW_OPTIONS, &initial_device_config.hw_options, sizeof(initial_device_config.hw_options));

    /* RF remotes */
    load_config_value(nvs_handle, CFG_RF_LIST, &initial_rf_config, sizeof(initial_rf_config));

    nvs_close(nvs_handle);
    memcpy(&device_config, &initial_device_config, sizeof(device_config));
    memcpy(&remotes_config, &initial_rf_config, sizeof(remotes_config));
}

void config_update_wing_settings(wing_id_t wing_id, bool dir, uint16_t ocp_threshold, uint16_t ocp_count)
{
    ESP_LOGI(CFG_LOG_TAG, "Config update start");
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open_from_partition("nvs_ext", CFG_NAMESPACE, NVS_READWRITE, &nvs_handle));

    save_config_value(nvs_handle, RIGHT_WING == wing_id ? CFG_RIGHT_WING_DIR : CFG_LEFT_WING_DIR, &dir, sizeof(dir));
    save_config_value(nvs_handle, RIGHT_WING == wing_id ? CFG_RIGHT_WING_OCP_THRESHOLD : CFG_LEFT_WING_OCP_THRESHOLD, &ocp_threshold, sizeof(ocp_threshold));
    save_config_value(nvs_handle, RIGHT_WING == wing_id ? CFG_RIGHT_WING_OCP_COUNT : CFG_LEFT_WING_OCP_COUNT, &ocp_count, sizeof(ocp_count));

    ESP_ERROR_CHECK(nvs_commit(nvs_handle));
    nvs_close(nvs_handle);

    ESP_LOGI(CFG_LOG_TAG, "Config updated");
}

void config_update_input_settings(input_action_t in1, input_action_t in2, input_action_t in3, input_action_t in4)
{
    ESP_LOGI(CFG_LOG_TAG, "INPUT - Config update start");
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open_from_partition("nvs_ext", CFG_NAMESPACE, NVS_READWRITE, &nvs_handle));

    device_config.input_actions[0] = in1;
    device_config.input_actions[1] = in2;
    device_config.input_actions[2] = in3;
    device_config.input_actions[3] = in4;
    save_config_value(nvs_handle, CFG_INPUT_ACTIONS, &device_config.input_actions, sizeof(device_config.input_actions));

    ESP_ERROR_CHECK(nvs_commit(nvs_handle));
    nvs_close(nvs_handle);

    ESP_LOGI(CFG_LOG_TAG, "INPUT - Config updated");
}

void config_update_output_settings(output_action_t out1, output_action_t out2)
{
    ESP_LOGI(CFG_LOG_TAG, "OUTPUT - Config update start");
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open_from_partition("nvs_ext", CFG_NAMESPACE, NVS_READWRITE, &nvs_handle));

    device_config.output_actions[0] = out1;
    device_config.output_actions[1] = out2;
    save_config_value(nvs_handle, CFG_OUTPUT_ACTIONS, &device_config.output_actions, sizeof(device_config.output_actions));

    ESP_ERROR_CHECK(nvs_commit(nvs_handle));
    nvs_close(nvs_handle);

    ESP_LOGI(CFG_LOG_TAG, "OUTPUT - Config updated");
}

bool config_add_remote(uint64_t rf_code, input_action_t action)
{
    bool status = false;
    for(size_t i = 0; i < STATIC_CFG_NUM_OF_REMOTES; i++){
        if(remotes_config[i].id == 0){
            remotes_config[i].id = rf_code;
            remotes_config[i].action = action;
            status = true;
            break;
        }
    }
    if(status){
        ESP_LOGI(CFG_LOG_TAG, "RF - Config update start");
        nvs_handle_t nvs_handle;
        ESP_ERROR_CHECK(nvs_open_from_partition("nvs_ext", CFG_NAMESPACE, NVS_READWRITE, &nvs_handle));

        save_config_value(nvs_handle, CFG_RF_LIST, &remotes_config, sizeof(remotes_config));

        ESP_ERROR_CHECK(nvs_commit(nvs_handle));
        nvs_close(nvs_handle);

        ESP_LOGI(CFG_LOG_TAG, "RF - Config updated");
    }
    return status;
}
void config_remove_remote(uint64_t rf_code)
{
    for(size_t i = 0; i < STATIC_CFG_NUM_OF_REMOTES; i++){
        if(remotes_config[i].id == rf_code){
            remotes_config[i].id = 0;
            remotes_config[i].action = 0;

            ESP_LOGI(CFG_LOG_TAG, "RF - Config update start");
            nvs_handle_t nvs_handle;
            ESP_ERROR_CHECK(nvs_open_from_partition("nvs_ext", CFG_NAMESPACE, NVS_READWRITE, &nvs_handle));

            save_config_value(nvs_handle, CFG_RF_LIST, &remotes_config, sizeof(remotes_config));

            ESP_ERROR_CHECK(nvs_commit(nvs_handle));
            nvs_close(nvs_handle);

            ESP_LOGI(CFG_LOG_TAG, "RF - Config updated");
            
            break;
        }
    }
}

bool config_check_remote(uint64_t rf_code){
    for(size_t i = 0; i < STATIC_CFG_NUM_OF_REMOTES; i++){
        if(remotes_config[i].id == rf_code){
            return true;
        }
    }
    return false;
}
uint64_t config_get_next_remote(uint64_t rf_code){
    if(0 == rf_code){
        for(size_t i = 0; i < STATIC_CFG_NUM_OF_REMOTES; i++){
            if(0 != remotes_config[i].id){
                return remotes_config[i].id;
            }
        }
    }
    else{
        size_t pos = 0;
        for(size_t i = 0; i < STATIC_CFG_NUM_OF_REMOTES; i++){
            if(rf_code == remotes_config[i].id){
                pos = i+1;
            }
        }
        for(size_t i = pos; i < STATIC_CFG_NUM_OF_REMOTES; i++){
            if(0 != remotes_config[i].id){
                return remotes_config[i].id;
            }
        }
    }
    return 0;
}