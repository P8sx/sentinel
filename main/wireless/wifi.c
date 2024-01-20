#include "wifi.h"
#include "config.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "common/config.h"
#include "io/io.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "drivers/i2c.h"
#include "esp_console.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "esp_vfs_cdcacm.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>
#include <fcntl.h>
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"

#include "esp_timer.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "io/gate.h"
#include "esp_ota_ops.h"
#include <esp_ghota.h>
#include "mqtt_client.h"


static ghota_client_handle_t *ghota_client = NULL;
static esp_mqtt_client_handle_t mqtt_client = NULL;
static atomic_uint_fast8_t ghota_update_progress = 0;
static int tcp_socket = 0;
static bool wifi_status = false;


uint8_t ghota_get_update_progress(){
    uint8_t progress = atomic_load(&ghota_update_progress);
    return progress;
}

void ghota_start_check(){
    ghota_start_update_task(ghota_client);
}

static void wifi_ghota_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    ghota_client_handle_t *client = (ghota_client_handle_t *)arg;
    ESP_LOGI(GHOTA_LOG_TAG, "Got Update Callback: %s", ghota_get_event_str(event_id));
    if (event_id == GHOTA_EVENT_START_STORAGE_UPDATE) {
        ESP_LOGI(GHOTA_LOG_TAG, "Starting storage update");
        /* if we are updating the SPIFF storage we should unmount it */
    } else if (event_id == GHOTA_EVENT_FINISH_STORAGE_UPDATE) {
        ESP_LOGI(GHOTA_LOG_TAG, "Ending storage update");
        /* after updating we can remount, but typically the device will reboot shortly after recieving this event. */
    } else if (event_id == GHOTA_EVENT_FIRMWARE_UPDATE_PROGRESS) {
        /* display some progress with the firmware update */
        ESP_LOGI(GHOTA_LOG_TAG, "Firmware Update Progress: %d%%", *((int*) event_data));
        atomic_store(&ghota_update_progress , *((int*) event_data));
    }
    (void)client;
}

void ghota_config_init(){
    // wifi_ap_record_t ap_info;
    // if(ESP_OK != esp_wifi_sta_get_ap_info(&ap_info)){
    //     ESP_LOGE(GHOTA_LOG_TAG, "WiFi not connected, ota initialization failed");
    //     return;
    // }

    ghota_config_t ghconfig = {
        .filenamematch = "sentinel-esp32s3.bin",
        .storagenamematch = "storage-esp32.bin",
        .updateInterval = 0,
    };
    ghota_client = ghota_init(&ghconfig);
    if (NULL == ghota_client) {
        ESP_LOGE(GHOTA_LOG_TAG, "ghota_client_init failed");
    }
    ESP_ERROR_CHECK(esp_event_handler_instance_register(GHOTA_EVENTS, ESP_EVENT_ANY_ID, &wifi_ghota_event_handler, ghota_client, NULL));
    ESP_LOGI(GHOTA_LOG_TAG, "Init successful");
}



static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    static int retry_num = 0;
    if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED){
        wifi_status = true;
    }
    
    if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_status = false;
        if (retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            retry_num++;
            ESP_LOGI(WIFI_LOG_TAG, "retry to connect to the AP");
        }
        ESP_LOGI(WIFI_LOG_TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(WIFI_LOG_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        retry_num = 0;
    }
}

bool wifi_connected(){
    return wifi_status;
}

void wifi_config_init(){
    if(0 == strlen(device_config.wifi_ssid) || 0 == strlen(device_config.wifi_ssid)){
        ESP_LOGE(WIFI_LOG_TAG,"No SSID/Password provided WiFi init failed");
        return;
    }
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_create_default_wifi_sta();

    esp_netif_t *esp_netif = NULL;
    esp_netif = esp_netif_next(esp_netif);
    esp_netif_set_hostname(esp_netif, device_config.device_name);
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    memcpy(&(wifi_config.sta.ssid), &(device_config.wifi_ssid), sizeof(wifi_config.sta.ssid));
    memcpy(&(wifi_config.sta.password), &(device_config.wifi_password), sizeof(wifi_config.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(WIFI_LOG_TAG, "wifi_init_sta finished.");
}



static char mqtt_availability_topic[128];
static char mqtt_left_wing_state_topic[128];
static char mqtt_left_wing_cmd_topic[128];
static char mqtt_right_wing_state_topic[128];
static char mqtt_right_wing_cmd_topic[128];
static char mqtt_both_wing_state_topic[128];
static char mqtt_both_wing_cmd_topic[128];
static char mqtt_update_topic[128];


static void wifi_gate_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
     if(MOTOR_STATUS_CHANGED == event_id){
        gate_status_t gate_status = *(gate_status_t *)event_data;
        if(M1 == gate_status.id){
            switch (gate_status.state){
                case OPENED:
                    esp_mqtt_client_publish(mqtt_client, mqtt_right_wing_state_topic, "open", 0, 1, 1);
                break;
                case OPENING:
                    esp_mqtt_client_publish(mqtt_client, mqtt_right_wing_state_topic, "opening", 0, 1, 1);
                break;
                case CLOSED:
                    esp_mqtt_client_publish(mqtt_client, mqtt_right_wing_state_topic, "closed", 0, 1, 1);
                break;
                case CLOSING:
                    esp_mqtt_client_publish(mqtt_client, mqtt_right_wing_state_topic, "closing", 0, 1, 1);
                break;
                case STOPPED_CLOSING:
                case STOPPED_OPENING:
                    esp_mqtt_client_publish(mqtt_client, mqtt_right_wing_state_topic, "stopped", 0, 1, 1);             
                break;
                default:
                break;
            }
        } else if(M2 == gate_status.id){
            switch (gate_status.state){
                case OPENED:
                    esp_mqtt_client_publish(mqtt_client, mqtt_left_wing_state_topic, "open", 0, 1, 1);
                break;
                case OPENING:
                    esp_mqtt_client_publish(mqtt_client, mqtt_left_wing_state_topic, "opening", 0, 1, 1);
                break;
                case CLOSED:
                    esp_mqtt_client_publish(mqtt_client, mqtt_left_wing_state_topic, "closed", 0, 1, 1);
                break;
                case CLOSING:
                    esp_mqtt_client_publish(mqtt_client, mqtt_left_wing_state_topic, "closing", 0, 1, 1);
                break;
                case STOPPED_CLOSING:
                case STOPPED_OPENING:
                    esp_mqtt_client_publish(mqtt_client, mqtt_left_wing_state_topic, "stopped", 0, 1, 1);             
                break;
                default:
                break;
            }
        }
        gate_state_t m1m2_state = gate_get_state(M1M2);
        switch (m1m2_state){
            case OPENED:
                esp_mqtt_client_publish(mqtt_client, mqtt_both_wing_state_topic, "open", 0, 1, 1);
            break;
            case OPENING:
                esp_mqtt_client_publish(mqtt_client, mqtt_both_wing_state_topic, "opening", 0, 1, 1);
            break;
            case CLOSED:
                esp_mqtt_client_publish(mqtt_client, mqtt_both_wing_state_topic, "closed", 0, 1, 1);
            break;
            case CLOSING:
                esp_mqtt_client_publish(mqtt_client, mqtt_both_wing_state_topic, "closing", 0, 1, 1);
            break;
            case STOPPED_CLOSING:
            case STOPPED_OPENING:
                esp_mqtt_client_publish(mqtt_client, mqtt_both_wing_state_topic, "stopped", 0, 1, 1);             
            break;
            default:
            break;
        }
    }
}
void mqtt_cover_config(char* config_buffer, size_t size, const char* name, const char* device_name, const char* wing_name, const char* availability_topic, const char* state_topic, const char* command_topic) {
  snprintf(config_buffer,
           size,
           "{"
           "\"name\": \"%s\","
           "\"unique_id\": \"%s-%s\","
           "\"availability_topic\": \"%s\","
           "\"command_topic\": \"%s\","
           "\"state_topic\": \"%s\","
           "\"payload_available\": \"1\","
           "\"payload_not_available\": \"0\","
           "\"availability_mode\": \"latest\","
           "\"device\": {"
           "\"identifiers\": [\"%s\"],"
           "\"manufacturer\": \"P8sx\","
           "\"model\": \"Sentinel\","
           "\"name\": \"Sentinel\","
           "\"sw_version\": \"%s\","
           "\"hw_version\": \"%s\""
           "},"
           "\"device_class\": \"garage\""
           "}",
           name, device_name, wing_name, availability_topic, command_topic, state_topic, device_name, device_config.sw_version, device_config.hw_version);
}
void mqtt_button_config(char* config_buffer, size_t size, const char* name, const char* device_name, const char* unique_id, const char* availability_topic, const char* command_topic) {
  snprintf(config_buffer,
           size,
           "{"
           "\"name\": \"%s\","
           "\"unique_id\": \"%s-%s\","
           "\"availability_topic\": \"%s\","
           "\"availability_mode\": \"latest\","
           "\"command_topic\": \"%s\","
           "\"payload_available\": \"1\","
           "\"payload_not_available\": \"0\","
           "\"device\": {"
           "\"identifiers\": [\"%s\"],"
           "\"manufacturer\": \"P8sx\","
           "\"model\": \"Sentinel\","
           "\"name\": \"Sentinel\","
           "\"sw_version\": \"%s\","
           "\"hw_version\": \"%s\""
           "},"
           "\"device_class\": \"update\","
           "\"entity_category\": \"diagnostic\","
           "\"payload_press\": \"\""
           "}",
           name, device_name, unique_id, availability_topic, command_topic, device_name, device_config.sw_version, device_config.hw_version);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;

    static char config[512];
    static char config_topic[128];

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(MQTT_LOG_TAG, "MQTT_EVENT_CONNECTED");

        /* Publish cover configuration */
        snprintf(config_topic, sizeof(config_topic),"homeassistant/cover/%s/left-wing/config", device_config.device_name);
        mqtt_cover_config(config, 512, "Left wing", device_config.device_name, "left-wing", mqtt_availability_topic, mqtt_left_wing_state_topic, mqtt_left_wing_cmd_topic);
        esp_mqtt_client_publish(client, config_topic, config, 0, 1, 1);
        esp_mqtt_client_subscribe(client, mqtt_left_wing_cmd_topic, 1);

        snprintf(config_topic, sizeof(config_topic),"homeassistant/cover/%s/right-wing/config", device_config.device_name);
        mqtt_cover_config(config, 512, "Right wing", device_config.device_name, "right-wing", mqtt_availability_topic, mqtt_right_wing_state_topic, mqtt_right_wing_cmd_topic);
        esp_mqtt_client_publish(client, config_topic, config, 0, 1, 1);
        esp_mqtt_client_subscribe(client, mqtt_right_wing_cmd_topic, 1);

        snprintf(config_topic, sizeof(config_topic),"homeassistant/cover/%s/both-wing/config", device_config.device_name);
        mqtt_cover_config(config, 512, "Both wing", device_config.device_name, "both-wing", mqtt_availability_topic, mqtt_both_wing_state_topic, mqtt_both_wing_cmd_topic);
        esp_mqtt_client_publish(client, config_topic, config, 0, 1, 1);
        esp_mqtt_client_subscribe(client, mqtt_both_wing_cmd_topic, 1);

        snprintf(config_topic, sizeof(config_topic), "homeassistant/button/%s/update/config", device_config.device_name);
        mqtt_button_config(config, 512, "OTA Update", device_config.device_name, "update", mqtt_availability_topic, mqtt_update_topic);
        esp_mqtt_client_publish(client, config_topic, config, 0, 1, 1);
        esp_mqtt_client_subscribe(client, mqtt_update_topic, 1);

        /* Publish availability status */
        esp_mqtt_client_publish(client, mqtt_availability_topic, "1", 0, 1, 0);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(MQTT_LOG_TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(MQTT_LOG_TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(MQTT_LOG_TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(MQTT_LOG_TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(MQTT_LOG_TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(MQTT_LOG_TAG, "MQTT_EVENT_DATA");
        if (strncmp(event->topic, mqtt_right_wing_cmd_topic, event->topic_len) == 0) {
            xQueueSend(gate_action_queue, &GATE_CMD(
                (strncmp("OPEN", event->data, event->data_len) == 0) ? OPEN :
                (strncmp("CLOSE", event->data, event->data_len) == 0) ? CLOSE : STOP, M1), pdMS_TO_TICKS(500));
        } else if (strncmp(event->topic, mqtt_left_wing_cmd_topic, event->topic_len) == 0) {
            xQueueSend(gate_action_queue, &GATE_CMD(
                (strncmp("OPEN", event->data, event->data_len) == 0) ? OPEN :
                (strncmp("CLOSE", event->data, event->data_len) == 0) ? CLOSE : STOP, M2), pdMS_TO_TICKS(500));
        } else if (strncmp(event->topic, mqtt_both_wing_cmd_topic, event->topic_len) == 0) {
            xQueueSend(gate_action_queue, &GATE_CMD(
                (strncmp("OPEN", event->data, event->data_len) == 0) ? OPEN :
                (strncmp("CLOSE", event->data, event->data_len) == 0) ? CLOSE : STOP, M1M2), pdMS_TO_TICKS(500));
        }

        if(strncmp(event->topic, mqtt_update_topic, event->topic_len) == 0){
            ESP_LOGI(MQTT_LOG_TAG, "Initializing OTA");
            ghota_start_check();
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(MQTT_LOG_TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGI(MQTT_LOG_TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(MQTT_LOG_TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void mqtt_config_init(){
    if(0 == strlen(device_config.mqtt_uri)){
        ESP_LOGE(MQTT_LOG_TAG,"No MQTT URI provided");
        return;
    }
    
    snprintf(mqtt_availability_topic, sizeof(mqtt_availability_topic), "sentinel/%s/state/connected", device_config.device_name);
    snprintf(mqtt_left_wing_state_topic, sizeof(mqtt_left_wing_state_topic), "sentinel/%s/%s/state", device_config.device_name, "left-wing");
    snprintf(mqtt_left_wing_cmd_topic, sizeof(mqtt_left_wing_cmd_topic), "sentinel/%s/%s/cmd", device_config.device_name, "left-wing");
    snprintf(mqtt_right_wing_state_topic, sizeof(mqtt_right_wing_state_topic), "sentinel/%s/%s/state", device_config.device_name, "right-wing");
    snprintf(mqtt_right_wing_cmd_topic, sizeof(mqtt_right_wing_cmd_topic), "sentinel/%s/%s/cmd", device_config.device_name, "right-wing");
    snprintf(mqtt_both_wing_state_topic, sizeof(mqtt_both_wing_state_topic), "sentinel/%s/%s/state", device_config.device_name, "both-wing");
    snprintf(mqtt_both_wing_cmd_topic, sizeof(mqtt_both_wing_cmd_topic), "sentinel/%s/%s/cmd", device_config.device_name, "both-wing");
    snprintf(mqtt_update_topic, sizeof(mqtt_update_topic), "sentinel/%s/%s/cmd", device_config.device_name, "update");

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = device_config.mqtt_uri,
        .broker.address.port = device_config.mqtt_port,
        .credentials.client_id = device_config.device_name,
        .session.last_will.msg = "0",
        .session.last_will.qos = 1,
        .session.last_will.topic = mqtt_availability_topic,
        .session.last_will.retain = true,
        .session.keepalive = 10,
        .buffer.out_size = 1024,
        .buffer.size = 1024,
    };
    ESP_LOGI(MQTT_LOG_TAG, "%s",mqtt_cfg.broker.address.uri);

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_client));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(GATE_EVENTS, ESP_EVENT_ANY_ID, &wifi_gate_event_handler, NULL, NULL));
}




static void tcp_receive_handle(const int sock)
{
    int len;
    char rx_buffer[128];
    do {
        len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0) {
            ESP_LOGE(TCP_LOG_TAG, "Error occurred during receiving: errno %d", errno);
        } else if (len == 0) {
            ESP_LOGW(TCP_LOG_TAG, "Connection closed");
        } else {
            rx_buffer[len] = 0; // Null-terminate whatever is received and treat it like a string
            ESP_LOGI(TCP_LOG_TAG, "Received %d bytes: %s, strcmp: %i", len, rx_buffer, strcmp(rx_buffer, "uptime\n"));
            
            char tx_buffer[128] = {0};
            rx_buffer[strcspn(rx_buffer, "\n")] = 0;
            if(strcmp(rx_buffer, "uptime") == 0){
                int timer = esp_timer_get_time()/1000/1000;
                snprintf(tx_buffer, 128, "device uptime:%is\n", timer);
            }
            else if(strcmp(rx_buffer, "open m1") == 0){
                gate_command_t cmd = {.action = OPEN, .id = M1};
                xQueueSend(gate_action_queue, &cmd, portMAX_DELAY);
            }
            else if(strcmp(rx_buffer, "close m1") == 0){
                gate_command_t cmd = {.action = CLOSE, .id = M1};
                xQueueSend(gate_action_queue, &cmd, portMAX_DELAY);
            }
            else if(strcmp(rx_buffer, "stop m1") == 0){
                gate_command_t cmd = {.action = STOP, .id = M1};
                xQueueSend(gate_action_queue, &cmd, portMAX_DELAY);
            }
            else if(strcmp(rx_buffer, "next m1") == 0){
                gate_command_t cmd = {.action = NEXT_STATE, .id = M1};
                xQueueSend(gate_action_queue, &cmd, portMAX_DELAY);
            }
            else if(strcmp(rx_buffer, "open m2") == 0){
                gate_command_t cmd = {.action = OPEN, .id = M2};
                xQueueSend(gate_action_queue, &cmd, portMAX_DELAY);
            }
            else if(strcmp(rx_buffer, "close m2") == 0){
                gate_command_t cmd = {.action = CLOSE, .id = M2};
                xQueueSend(gate_action_queue, &cmd, portMAX_DELAY);
            }
            else if(strcmp(rx_buffer, "stop m2") == 0){
                gate_command_t cmd = {.action = STOP, .id = M2};
                xQueueSend(gate_action_queue, &cmd, portMAX_DELAY);
            }
            else if(strcmp(rx_buffer, "next m2") == 0){
                gate_command_t cmd = {.action = NEXT_STATE, .id = M2};
                xQueueSend(gate_action_queue, &cmd, portMAX_DELAY);
            }
            else if(strcmp(rx_buffer, "open") == 0){
                gate_command_t cmd = {.action = OPEN, .id = M1M2};
                xQueueSend(gate_action_queue, &cmd, portMAX_DELAY);
            }
            else if(strcmp(rx_buffer, "close") == 0){
                gate_command_t cmd = {.action = CLOSE, .id = M1M2};
                xQueueSend(gate_action_queue, &cmd, portMAX_DELAY);
            }
            else if(strcmp(rx_buffer, "stop") == 0){
                gate_command_t cmd = {.action = STOP, .id = M1M2};
                xQueueSend(gate_action_queue, &cmd, portMAX_DELAY);
            }
            else if(strcmp(rx_buffer, "next") == 0){
                gate_command_t cmd = {.action = NEXT_STATE, .id = M1M2};
                xQueueSend(gate_action_queue, &cmd, portMAX_DELAY);
            }
            else if(strcmp(rx_buffer, "status") == 0){
                gate_state_t m1 = gate_get_state(M1);
                gate_state_t m2 = gate_get_state(M2);

                snprintf(tx_buffer, 128, "M1:%s PCNT:%i ANALOG %i, OPCNT:%i CPCNT:%i, M2:%s PCNT:%i ANALOG %i, OPCNT:%i CPCNT:%i\n",STATE_STRING(m1), (int)io_motor_get_pcnt(M1), (int)io_motor_get_current(M1), (int)gate_get_open_pcnt(M1), (int)gate_get_close_pcnt(M1), STATE_STRING(m2), (int)io_motor_get_pcnt(M2), (int)io_motor_get_current(M2), (int)gate_get_open_pcnt(M2), (int)gate_get_close_pcnt(M2));
            }
            else if(strcmp(rx_buffer, "partition") == 0){
                const esp_partition_t *test = esp_ota_get_boot_partition();
                snprintf(tx_buffer, 128, "partition: %s, address: %i\n",test->label, (uint)test->address);
            }
            else if(strcmp(rx_buffer, "ota") == 0){
                ESP_ERROR_CHECK(ghota_start_update_task(ghota_client));
            }
            else if(strcmp(rx_buffer, "version") == 0){
                semver_t *current_version = NULL;
                current_version = ghota_get_current_version(ghota_client);
                snprintf(tx_buffer, 128, "Current version %i.%i.%i\n",current_version->major, current_version->minor, current_version->patch);
            }
            int tx_len = 0;
            tx_len = strlen(tx_buffer);
            if(tx_len > 0 && tcp_socket > 0){
                int to_write = tx_len;
                while (to_write > 0) {
                    int written = send(sock, tx_buffer + (tx_len - to_write), to_write, 0);
                    if (written < 0) {
                        ESP_LOGE(TCP_LOG_TAG, "Error occurred during sending: errno %d", errno);
                        // Failed to retransmit, giving up
                        return;
                    }
                    to_write -= written;
                }
            }

            // send() can return less bytes than supplied length.
            // Walk-around for robust implementation.
            
        }
    } while (len > 0);

}

void tcp_server_task(void *pvParameters)
{
    char addr_str[128];
    int addr_family = AF_INET;
    int ip_protocol = 0;
    int keepAlive = 1;
    int keepIdle = TCP_KEEPALIVE_IDLE;
    int keepInterval = TCP_KEEPALIVE_INTERVAL;
    int keepCount = TCP_KEEPALIVE_COUNT;
    struct sockaddr_storage dest_addr;

    if (addr_family == AF_INET) {
        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
        dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(TCP_PORT);
        ip_protocol = IPPROTO_IP;
    }

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) {
        ESP_LOGE(TCP_LOG_TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    // Note that by default IPV6 binds to both protocols, it is must be disabled
    // if both protocols used at the same time (used in CI)
    setsockopt(listen_sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));

    ESP_LOGI(TCP_LOG_TAG, "Socket created");

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TCP_LOG_TAG, "Socket unable to bind: errno %d", errno);
        ESP_LOGE(TCP_LOG_TAG, "IPPROTO: %d", addr_family);
        goto CLEAN_UP;
    }
    ESP_LOGI(TCP_LOG_TAG, "Socket bound, port %d", TCP_PORT);

    err = listen(listen_sock, 1);
    if (err != 0) {
        ESP_LOGE(TCP_LOG_TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }

    while (1) {

        ESP_LOGI(TCP_LOG_TAG, "Socket listening");

        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t addr_len = sizeof(source_addr);
        tcp_socket = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (tcp_socket < 0) {
            ESP_LOGE(TCP_LOG_TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        // Set tcp keepalive option
        setsockopt(tcp_socket, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
        setsockopt(tcp_socket, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
        setsockopt(tcp_socket, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
        setsockopt(tcp_socket, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));
        // Convert ip address to string
        if (source_addr.ss_family == PF_INET) {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        }
        ESP_LOGI(TCP_LOG_TAG, "Socket accepted ip address: %s", addr_str);

        tcp_receive_handle(tcp_socket);

        shutdown(tcp_socket, 0);
        ESP_LOGI(TCP_LOG_TAG, "Exited handle");
        close(tcp_socket);
        tcp_socket = -1;
    }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}

void tcp_server_task_transmit(void *pvParameters){
    while (true)
    {
        if (tcp_socket > 0) {
            int len = 64;
            char rx_buffer[64];

            int to_write = len;
            while (to_write > 0) {
                int written = send(tcp_socket, rx_buffer + (len - to_write), to_write, 0);
                if (written < 0) {
                    ESP_LOGE(TCP_LOG_TAG, "Error occurred during sending: errno %d", errno);
                }
                to_write -= written;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
}