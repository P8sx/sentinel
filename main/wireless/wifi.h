#ifndef WIFI_H
#define WIFI_H

#include <stdio.h>
#include <stdbool.h>

void ghota_config_init();
uint8_t ghota_get_update_progress();
void ghota_start_check();

void wifi_config_init();
bool wifi_connected();

void mqtt_config_init();



void tcp_server_task(void *pvParameters);
#endif