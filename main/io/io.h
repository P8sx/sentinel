#ifndef IO_H
#define IO_H

#include "esp_err.h"
#include "common/types.h"
#include "esp_event.h"

ESP_EVENT_DECLARE_BASE(IO_EVENTS);

typedef enum io_event_e
{
    IO_INPUT_TRIGGERED_EVENT = 0x01,
    IO_RF_EVENT = 0x10,
} io_event_e;

void io_init();

void io_handler_init();
void io_handler_input_handling_task(void *pvParameters);

/* IO Controll */
void io_wing_dir(wing_id_t id, uint8_t clockwise);
void io_wing_stop(wing_id_t id);
void io_wing_pwm(wing_id_t id, uint32_t freq);
void io_wing_fade(wing_id_t id, uint32_t target, uint32_t time);

int32_t io_wing_get_pcnt(wing_id_t id);
uint16_t io_wing_get_current(wing_id_t id); // in mA

float io_get_soc_temp();
void io_buzzer(uint8_t counts, uint16_t on_period, uint16_t off_period);

#endif