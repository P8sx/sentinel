#ifndef I2C_H
#define I2C_H
#include <stdbool.h>

void init_i2c();
void init_i2c_oled();
void i2c_oled_power_saver(bool enable);
void i2c_oled_init_screen();

#endif