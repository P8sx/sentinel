#ifndef WIFI_H
#define WIFI_H

void ota_init();
void wifi_init();
void tcp_server_task(void *pvParameters);

#endif