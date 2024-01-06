#ifndef GATE_H
#define GATE_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "common/types.h"
#include <stdio.h>
#include "esp_event.h"


ESP_EVENT_DECLARE_BASE(GATE_EVENTS);


extern QueueHandle_t gate_action_queue;

typedef enum motor_event_e
{
    MOTOR_STATUS_CHANGED = 0,
    MOTOR_ERROR_OCCURED = 1,
} motor_event_e;

typedef struct gate_t {
    const motor_id_t id; 
    atomic_uint_fast8_t state;
    atomic_int_fast16_t open_pcnt;
    atomic_int_fast16_t close_pcnt;
    atomic_bool open_pcnt_cal;
    atomic_bool close_pcnt_cal;
} gate_t;

gate_status_t gate_get_motor_state(motor_id_t id);
int16_t gate_get_motor_open_pcnt(motor_id_t id);
int16_t gate_get_motor_close_pcnt(motor_id_t id);

void gate_motor_init();
void gate_action_task(void *pvParameters);
void gate_task(void *pvParameters);

#endif