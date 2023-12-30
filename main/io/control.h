#ifndef CONTROL_H
#define CONTROL_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdio.h>

typedef enum states_t {
    OPENED,
    OPENING,
    CLOSED,
    CLOSING,
    STOPPED_OPENING,
    STOPPED_CLOSING,
    UNKNOWN,
} states_t;

static const char *STATES_STRING[] = {"OPENED","OPENING","CLOSED","CLOSING","STOPPED_OPENING","STOPPED_CLOSING", "UNKNOWN"};

typedef enum motor_id_t {
    M1 = 1,
    M2 = 2,
} motor_id_t;

typedef enum motor_action_t{
    OPEN,
    CLOSE,
    STOP,
    NEXT_STATE,
} motor_action_t;

typedef struct motor_command_t{
    motor_id_t id;
    motor_action_t action;
} motor_command_t;

typedef struct motor_t {
    states_t state;
    const motor_id_t id; 
    int16_t open_pcnt;
    int16_t close_pcnt;
} motor_t;

extern QueueHandle_t motor_action_queue;

states_t control_get_motor_state(motor_id_t id);
void motor_action_task(void *pvParameters);
void motor_m1_task(void *pvParameters);
void motor_m2_task(void *pvParameters);

#endif