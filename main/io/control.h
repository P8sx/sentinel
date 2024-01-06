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


void control_input_task(void *pvParameters);
void control_init();

void IRAM_ATTR input_isr_handler(void* arg);
void IRAM_ATTR button_isr_handler(void* arg);
void IRAM_ATTR endstop_isr_handler(void* arg);

#endif