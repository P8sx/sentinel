#ifndef CONTROL_H
#define CONTROL_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "common/types.h"
#include <stdio.h>



extern QueueHandle_t motor_action_queue;

gate_states_t control_get_motor_state(motor_id_t id);
int16_t control_get_motor_open_pcnt(motor_id_t id);
int16_t control_get_motor_close_pcnt(motor_id_t id);

void control_motor_init();
void motor_action_task(void *pvParameters);
void motor_task(void *pvParameters);

#endif