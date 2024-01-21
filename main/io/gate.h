#ifndef GATE_H
#define GATE_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "common/types.h"
#include <stdio.h>
#include "esp_event.h"

/* Gate status change events */
ESP_EVENT_DECLARE_BASE(GATE_EVENTS);

/* Queue for controling wing using wing_command_t */
extern QueueHandle_t wing_action_queue;

typedef enum wing_event_e
{
    WING_STATUS_CHANGED = 0,
    WING_ERROR_OCCURED = 1,
} wing_event_e;

typedef struct wing_t {
    const wing_id_t id; 
    atomic_uint_fast8_t state;
    atomic_int_fast16_t open_pcnt;
    atomic_int_fast16_t close_pcnt;
    atomic_bool open_pcnt_cal;
    atomic_bool close_pcnt_cal;
} wing_t;

wing_state_t wing_get_state(wing_id_t id);
int16_t wing_get_open_pcnt(wing_id_t id);
int16_t wing_get_close_pcnt(wing_id_t id);

void wing_module_init();
void wing_action_task(void *pvParameters);
void wing_task(void *pvParameters);

#endif