#include "i2c.h"
#include "common/config.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "io/io.h"
#include <u8g2.h>
#include "esp_app_desc.h"
#include <string.h>

static u8g2_t u8g2;

static uint8_t u8g2_hal_i2c_byte(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
	static i2c_cmd_handle_t int_handle_i2c = NULL;
	switch (msg) {
		case U8X8_MSG_BYTE_SEND: 
			uint8_t* data_ptr = (uint8_t*)arg_ptr;
			while (arg_int-- > 0) ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_write_byte(int_handle_i2c, *data_ptr++, 0x1));
			break;
		case U8X8_MSG_BYTE_START_TRANSFER: 
			uint8_t i2c_address = u8x8_GetI2CAddress(u8x8);
			int_handle_i2c = i2c_cmd_link_create();
			ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_start(int_handle_i2c));
			ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_write_byte(int_handle_i2c, i2c_address | I2C_MASTER_WRITE, 0x1));
			break;
		case U8X8_MSG_BYTE_END_TRANSFER: 
			ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_stop(int_handle_i2c));
			ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_cmd_begin(I2C_NUM_0, int_handle_i2c, pdMS_TO_TICKS(I2C_INT_TIMEOUT_MS)));
			i2c_cmd_link_delete(int_handle_i2c);
			break;
	}
	return 0;
}


static uint8_t u8g2_hal_gpio_and_delay(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
	switch (msg) {
		case U8X8_MSG_DELAY_MILLI:
			vTaskDelay(pdMS_TO_TICKS(arg_int));
			break;
	}
	return 0;
} 

bool i2c_device_check(uint8_t i2c_master_num, uint8_t i2c_addr) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, i2c_addr | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(i2c_master_num, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
	return (ESP_OK == ret) ? true : false;
}

void init_i2c(){
	i2c_config_t conf = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = I2C_INT_SDA_PIN,
		.scl_io_num = I2C_INT_SCL_PIN,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master.clk_speed = I2C_INT_FREQ,
	};
	ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &conf));
	ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, conf.mode, 0,  0, 0));
}

void init_i2c_oled()
{
	if(!i2c_device_check(I2C_NUM_0, I2C_INT_OLED_ADDR)){
		ESP_LOGE(I2C_LOG_TAG, "I2C oled not found, initialization aborted");
		return;
	}
	else{
		ESP_LOGI(I2C_LOG_TAG, "I2C oled found");
	}

	u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R2, u8g2_hal_i2c_byte, u8g2_hal_gpio_and_delay);

	u8x8_SetI2CAddress(&u8g2.u8x8, I2C_INT_OLED_ADDR);
	u8g2_InitDisplay(&u8g2);
	u8g2_SetPowerSave(&u8g2, false);

	i2c_oled_power_saver(false);
	i2c_oled_init_screen();
}

void i2c_oled_power_saver(bool enable){
	static bool status = false;
	if(status != enable){
		u8g2_SetPowerSave(&u8g2, enable);
		status = enable;
	}
}

void i2c_oled_init_screen(){

	const esp_app_desc_t *desc = esp_app_get_description();
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_t0_16b_tf);
    u8g2_DrawStr(&u8g2, 35, 18, "Sentinel");
	u8g2_DrawLine(&u8g2, 8,20,120,20);
    u8g2_DrawStr(&u8g2, 8, 36, strcat("SW ver: ",desc->version));

	u8g2_SendBuffer(&u8g2);
}