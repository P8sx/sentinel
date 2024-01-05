#ifndef IO_H
#define IO_H

#include "esp_err.h"
#include "common/types.h"



void io_init_outputs();
void io_init_inputs();
void io_init_analog();
void io_init_pwm();
void io_init_pcnt();

void io_motor_dir(motor_id_t id, uint8_t clockwise);
void io_motor_stop(motor_id_t id);
void io_motor_pwm(motor_id_t id, uint32_t freq);
void io_motor_fade(motor_id_t id, uint32_t target, uint32_t time);

int32_t io_motor_get_pcnt(motor_id_t id);
uint16_t io_motor_get_current(motor_id_t id);                           // in mA

void io_buzzer(uint8_t counts, uint16_t on_period, uint16_t off_period);

#endif