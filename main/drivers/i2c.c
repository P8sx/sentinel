#include "i2c.h"
#include "common/config.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "io/io.h"
#include <u8g2.h>
#include "esp_app_desc.h"

static bool i2c_oled_initialized = false;
static u8g2_t u8g2;

bool i2c_oled_init_state(){
	return i2c_oled_initialized;
}
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
	i2c_oled_initialized = true;
}

void i2c_oled_power_save(bool enable){
	if(!i2c_oled_initialized) return;
	static bool status = false;
	if(status != enable){
		u8g2_SetPowerSave(&u8g2, enable);
		status = enable;
	}
}

void i2c_oled_welcome_screen(){
	if(!i2c_oled_initialized) return;
	const esp_app_desc_t *desc = esp_app_get_description();
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_profont22_tr);
    u8g2_DrawStr(&u8g2, 19, 19, "Sentinel");
	u8g2_DrawLine(&u8g2, 10,23,117,23);

    u8g2_SetFont(&u8g2, u8g2_font_haxrcorp4089_tr);
    u8g2_DrawStr(&u8g2, 10, 34, "SW Ver:");
    u8g2_DrawStr(&u8g2, 118-u8g2_GetStrWidth(&u8g2, desc->version), 35, desc->version);
    u8g2_DrawStr(&u8g2, 118-u8g2_GetStrWidth(&u8g2, desc->date), 44, desc->date);
	u8g2_SendBuffer(&u8g2);
}

void i2c_oled_ota_update(uint8_t progress){
	if(!i2c_oled_initialized) return;
	u8g2_ClearBuffer(&u8g2);
	u8g2_SetBitmapMode(&u8g2, 1);
	u8g2_SetFontMode(&u8g2, 1);
	u8g2_DrawFrame(&u8g2, 8, 19, 112, 20);
	u8g2_DrawBox(&u8g2, 10, 21, progress+8, 16);
	u8g2_SetFont(&u8g2, u8g2_font_helvB08_tr);
	u8g2_DrawStr(&u8g2, 35,14, "OTA Update");
	u8g2_DrawStr(&u8g2, 25,51, "Progress:");
	char progress_text[5];
    snprintf(progress_text, sizeof(progress_text), "%d%%", progress);
	u8g2_DrawStr(&u8g2, 76,51, progress_text);
	u8g2_SendBuffer(&u8g2);
}

void i2c_oled_ota_start_check(){
	if(!i2c_oled_initialized) return;
	static const unsigned char image_ArrowC_1_36x36_bits[] U8X8_PROGMEM = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x3f,0x00,0x00,0x00,0x70,0xc0,0x00,0x00,0x00,0x0c,0x00,0x01,0x00,0x00,0x02,0x3f,0x01,0x00,0x00,0xe1,0xc0,0x00,0x00,0x80,0x10,0x00,0x00,0x00,0x80,0x08,0x00,0x20,0x00,0x40,0x04,0x00,0x50,0x00,0x40,0x04,0x00,0x88,0x00,0x20,0x02,0x00,0x04,0x01,0x20,0x02,0x00,0x02,0x02,0x10,0x01,0x00,0x01,0x04,0x10,0x01,0x80,0x00,0x08,0x10,0x01,0x80,0x8f,0x0f,0x1f,0x1f,0x00,0x88,0x00,0x01,0x10,0x00,0x88,0x00,0x02,0x08,0x00,0x88,0x00,0x04,0x04,0x00,0x44,0x00,0x08,0x02,0x00,0x44,0x00,0x10,0x01,0x00,0x22,0x00,0xa0,0x00,0x00,0x22,0x00,0x40,0x00,0x00,0x11,0x00,0x00,0x00,0x80,0x10,0x00,0x00,0x30,0x70,0x08,0x00,0x00,0xc8,0x0f,0x04,0x00,0x00,0x08,0x00,0x03,0x00,0x00,0x30,0xe0,0x00,0x00,0x00,0xc0,0x1f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
	u8g2_ClearBuffer(&u8g2);
	u8g2_SetBitmapMode(&u8g2, 1);
	u8g2_SetFontMode(&u8g2, 1);
	u8g2_SetFont(&u8g2, u8g2_font_helvB08_tr);
	u8g2_DrawStr(&u8g2, 35,14, "OTA Update");
	u8g2_DrawStr(&u8g2, 12,30, "Checking for updates");
	u8g2_DrawXBMP(&u8g2 ,47, 30, 36, 36, image_ArrowC_1_36x36_bits);
	u8g2_SendBuffer(&u8g2);
}
void i2c_oled_ota_update_available(){
	if(!i2c_oled_initialized) return;
	u8g2_ClearBuffer(&u8g2);
	u8g2_SetBitmapMode(&u8g2, 1);
	u8g2_SetFontMode(&u8g2, 1);
	u8g2_SetFont(&u8g2, u8g2_font_helvB08_tr);
	u8g2_DrawStr(&u8g2, 35,14, "OTA Update");
	u8g2_DrawStr(&u8g2, 30,30, "Update found!");
	u8g2_SendBuffer(&u8g2);
}
void i2c_oled_ota_start_update(){
	if(!i2c_oled_initialized) return;
	u8g2_ClearBuffer(&u8g2);
	u8g2_SetBitmapMode(&u8g2, 1);
	u8g2_SetFontMode(&u8g2, 1);
	u8g2_SetFont(&u8g2, u8g2_font_helvB08_tr);
	u8g2_DrawStr(&u8g2, 35,14, "OTA Update");
	u8g2_DrawStr(&u8g2, 30,30, "Starting update");
	u8g2_SendBuffer(&u8g2);
}
void i2c_oled_ota_finish_update(){
	if(!i2c_oled_initialized) return;
	u8g2_ClearBuffer(&u8g2);
	u8g2_SetBitmapMode(&u8g2, 1);
	u8g2_SetFontMode(&u8g2, 1);
	u8g2_SetFont(&u8g2, u8g2_font_helvB08_tr);
	u8g2_DrawStr(&u8g2, 35,14, "OTA Update");
	u8g2_DrawStr(&u8g2, 28,30, "Update finished");
	u8g2_SendBuffer(&u8g2);
}
void i2c_oled_ota_update_failed(){
	if(!i2c_oled_initialized) return;
	u8g2_ClearBuffer(&u8g2);
	u8g2_SetBitmapMode(&u8g2, 1);
	u8g2_SetFontMode(&u8g2, 1);
	u8g2_SetFont(&u8g2, u8g2_font_helvB08_tr);
	u8g2_DrawStr(&u8g2, 35,14, "OTA Update");
	u8g2_DrawStr(&u8g2, 30,30, "Update failed");
	u8g2_SendBuffer(&u8g2);
}