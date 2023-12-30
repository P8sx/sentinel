#ifndef IO_H
#define IO_H

#include "esp_err.h"

void io_init_outputs();
void io_init_inputs();
void io_init_analog();
void io_init_pwm();
void io_init_pcnt();

void io_m1_pwm(uint32_t freq);
void io_m2_pwm(uint32_t freq);
void io_m1_fade(uint32_t target, uint32_t time);
void io_m2_fade(uint32_t target, uint32_t time);
void io_m1_dir(uint8_t clockwise);
void io_m2_dir(uint8_t clockwise);
void io_m1_stop();
void io_m2_stop();

void io_buzzer(uint8_t counts, uint16_t on_period, uint16_t off_period);

int32_t io_get_pcnt_m1();
int32_t io_get_pcnt_m2();
int32_t io_get_analog_m1();
int32_t io_get_analog_m2();
#endif