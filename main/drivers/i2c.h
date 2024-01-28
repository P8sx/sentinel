#ifndef I2C_H
#define I2C_H
#include <stdbool.h>
#include <stdio.h>
#include "common/config.h"
#include "common/types.h"
#include <stdarg.h>

void init_i2c();
void init_i2c_oled();
bool i2c_oled_initialized();
void i2c_oled_power_save(bool enable);
void i2c_oled_welcome_screen();
void i2c_oled_home_screen(uint8_t screen_saver, float soc_temp, bool wifi_status, wing_state_t right_wing_state, wing_state_t left_wing_state, bool animation_toggle);

void i2c_oled_ota_update(uint8_t progress);
void i2c_oled_ota_start_check();
void i2c_oled_ota_update_available();
void i2c_oled_ota_start_update();
void i2c_oled_ota_finish_update();
void i2c_oled_ota_update_failed();
void i2c_oled_menu(const char *menu_title, int pos, int arg_count, ...);
void i2c_oled_menu_with_params(const char *menu_title, int current_pos, bool selected ,int arg_count, ...);
void i2c_oled_generic_info_screen(int arg_count, ...);


void i2c_oled_menu_status();

#endif