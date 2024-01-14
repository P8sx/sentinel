#ifndef WIFI_H
#define WIFI_H

#include <stdio.h>
#include <stdbool.h>

void ota_init();
void wifi_init();
bool wifi_connected();
void tcp_server_task(void *pvParameters);
uint8_t ghota_get_update_progress();
void ghota_start_check();
#endif