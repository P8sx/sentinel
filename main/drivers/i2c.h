#ifndef I2C_H
#define I2C_H
#include <stdbool.h>
#include <stdio.h>

void init_i2c();
void init_i2c_oled();
bool i2c_oled_initialized();
void i2c_oled_power_save(bool enable);
void i2c_oled_welcome_screen();

void i2c_oled_ota_update(uint8_t progress);
void i2c_oled_ota_start_check();
void i2c_oled_ota_update_available();
void i2c_oled_ota_start_update();
void i2c_oled_ota_finish_update();
void i2c_oled_ota_update_failed();
#endif