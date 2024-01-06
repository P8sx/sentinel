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

static ghota_client_handle_t *ghota_client = NULL;
static int tcp_socket = 0;


static void gate_ghota_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
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
    } else if (event_id == GHOTA_EVENT_STORAGE_UPDATE_PROGRESS) {
        /* display some progress with the spiffs partition update */
        ESP_LOGI(GHOTA_LOG_TAG, "Storage Update Progress: %d%%", *((int*) event_data));
    }
    (void)client;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    static int retry_num = 0;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
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





void wifi_init(){
    if(0 == strlen(device_config.wifi_ssid) || 0 == strlen(device_config.wifi_ssid)){
        ESP_LOGE(WIFI_LOG_TAG,"No SSID/Password provided WiFi init failed");
        return;
    }
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_create_default_wifi_sta();

    esp_netif_t *esp_netif = NULL;
    esp_netif = esp_netif_next(esp_netif);
    esp_netif_set_hostname(esp_netif, "sentinel");
    
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
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(WIFI_LOG_TAG, "wifi_init_sta finished.");
}

void ota_init(){
    wifi_ap_record_t ap_info;
    if(ESP_OK != esp_wifi_sta_get_ap_info(&ap_info)){
        ESP_LOGE(GHOTA_LOG_TAG, "WiFi not connected, ota initialization failed");
        return;
    }

    ghota_config_t ghconfig = {
        .filenamematch = "sentinel-esp32s3.bin",
        .storagenamematch = "storage-esp32.bin",
        .updateInterval = 0,
    };
    ghota_client = ghota_init(&ghconfig);
    if (NULL == ghota_client) {
        ESP_LOGE(GHOTA_LOG_TAG, "ghota_client_init failed");
    }
    ESP_ERROR_CHECK(esp_event_handler_instance_register(GHOTA_EVENTS, ESP_EVENT_ANY_ID, &gate_ghota_event_handler, ghota_client, NULL));
    ESP_LOGI(GHOTA_LOG_TAG, "Init successful");
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
                xQueueSend(motor_action_queue, &cmd, portMAX_DELAY);
            }
            else if(strcmp(rx_buffer, "close m1") == 0){
                gate_command_t cmd = {.action = CLOSE, .id = M1};
                xQueueSend(motor_action_queue, &cmd, portMAX_DELAY);
            }
            else if(strcmp(rx_buffer, "stop m1") == 0){
                gate_command_t cmd = {.action = STOP, .id = M1};
                xQueueSend(motor_action_queue, &cmd, portMAX_DELAY);
            }
            else if(strcmp(rx_buffer, "next m1") == 0){
                gate_command_t cmd = {.action = NEXT_STATE, .id = M1};
                xQueueSend(motor_action_queue, &cmd, portMAX_DELAY);
            }
            else if(strcmp(rx_buffer, "open m2") == 0){
                gate_command_t cmd = {.action = OPEN, .id = M2};
                xQueueSend(motor_action_queue, &cmd, portMAX_DELAY);
            }
            else if(strcmp(rx_buffer, "close m2") == 0){
                gate_command_t cmd = {.action = CLOSE, .id = M2};
                xQueueSend(motor_action_queue, &cmd, portMAX_DELAY);
            }
            else if(strcmp(rx_buffer, "stop m2") == 0){
                gate_command_t cmd = {.action = STOP, .id = M2};
                xQueueSend(motor_action_queue, &cmd, portMAX_DELAY);
            }
            else if(strcmp(rx_buffer, "next m2") == 0){
                gate_command_t cmd = {.action = NEXT_STATE, .id = M2};
                xQueueSend(motor_action_queue, &cmd, portMAX_DELAY);
            }
            else if(strcmp(rx_buffer, "open") == 0){
                gate_command_t cmd = {.action = OPEN, .id = M1};
                xQueueSend(motor_action_queue, &cmd, portMAX_DELAY);
                cmd.id = M2;
                xQueueSend(motor_action_queue, &cmd, portMAX_DELAY);
            }
            else if(strcmp(rx_buffer, "close") == 0){
                gate_command_t cmd = {.action = CLOSE, .id = M1};
                xQueueSend(motor_action_queue, &cmd, portMAX_DELAY);
                cmd.id = M2;
                xQueueSend(motor_action_queue, &cmd, portMAX_DELAY);
            }
            else if(strcmp(rx_buffer, "stop") == 0){
                gate_command_t cmd = {.action = STOP, .id = M1};
                xQueueSend(motor_action_queue, &cmd, portMAX_DELAY);
                cmd.id = M2;
                xQueueSend(motor_action_queue, &cmd, portMAX_DELAY);
            }
            else if(strcmp(rx_buffer, "next") == 0){
                gate_command_t cmd = {.action = NEXT_STATE, .id = M1};
                xQueueSend(motor_action_queue, &cmd, portMAX_DELAY);
                cmd.id = M2;
                xQueueSend(motor_action_queue, &cmd, portMAX_DELAY);
            }
            else if(strcmp(rx_buffer, "status") == 0){
                gate_status_t m1 = gate_get_motor_state(M1);
                gate_status_t m2 = gate_get_motor_state(M2);

                snprintf(tx_buffer, 128, "M1:%s PCNT:%i ANALOG %i, OPCNT:%i CPCNT:%i, M2:%s PCNT:%i ANALOG %i, OPCNT:%i CPCNT:%i\n",STATES_STRING[m1.state], (int)io_motor_get_pcnt(M1), (int)io_motor_get_current(M1), (int)gate_get_motor_open_pcnt(M1), (int)gate_get_motor_close_pcnt(M1), STATES_STRING[m2.state], (int)io_motor_get_pcnt(M2), (int)io_motor_get_current(M2), (int)gate_get_motor_open_pcnt(M2), (int)gate_get_motor_close_pcnt(M2));
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