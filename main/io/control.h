#ifndef CONTROL_H
#define CONTROL_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "driver/gpio.h"

extern QueueHandle_t input_queue; 

void control_input_handling_task(void *pvParameters);
void control_oled_handling_task(void *pvParameters);
void control_init();


#endif