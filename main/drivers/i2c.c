#include "i2c.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "io/io.h"
#include <u8g2.h>
#include "esp_app_desc.h"
#include "wireless/wifi.h"
static bool i2c_oled_init = false;
static SemaphoreHandle_t i2c_int_semaphore;
static u8g2_t u8g2;

static const unsigned char image_arrows_180_36x36_bits[] U8X8_PROGMEM = {0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x80, 0x02, 0x00, 0x00, 0x00, 0x40, 0x02, 0x00, 0x00, 0x00, 0x20, 0x02, 0x00, 0x00, 0x00, 0x10, 0x1e, 0x00, 0x00, 0x00, 0x08, 0x60, 0x00, 0x00, 0x00, 0x04, 0x80, 0x01, 0x00, 0x00, 0x08, 0x00, 0x06, 0x00, 0x00, 0x10, 0x1e, 0x08, 0x00, 0x00, 0x20, 0x62, 0x10, 0x00, 0x00, 0x40, 0x82, 0x21, 0x00, 0xc0, 0x80, 0x02, 0x22, 0x00, 0x20, 0x01, 0x03, 0x44, 0x00, 0x20, 0x01, 0x00, 0x48, 0x00, 0x90, 0x00, 0x00, 0x48, 0x00, 0x90, 0x00, 0x00, 0x88, 0x00, 0x90, 0x00, 0x00, 0x90, 0x00, 0x90, 0x00, 0x00, 0x90, 0x00, 0x90, 0x00, 0x00, 0x90, 0x00, 0x90, 0x00, 0x00, 0x90, 0x00, 0x10, 0x01, 0x00, 0x90, 0x00, 0x20, 0x01, 0x00, 0x90, 0x00, 0x20, 0x01, 0x00, 0x48, 0x00, 0x20, 0x02, 0x0c, 0x48, 0x00, 0x40, 0x04, 0x14, 0x30, 0x00, 0x40, 0x18, 0x24, 0x00, 0x00, 0x80, 0x60, 0x44, 0x00, 0x00, 0x00, 0x81, 0x87, 0x00, 0x00, 0x00, 0x06, 0x00, 0x01, 0x00, 0x00, 0x18, 0x00, 0x02, 0x00, 0x00, 0x60, 0x00, 0x01, 0x00, 0x00, 0x80, 0x87, 0x00, 0x00, 0x00, 0x00, 0x44, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00};
static const unsigned char image_arrows_36x36_bits[] U8X8_PROGMEM = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x70, 0xc0, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x01, 0x00, 0x00, 0x02, 0x3f, 0x01, 0x00, 0x00, 0xe1, 0xc0, 0x00, 0x00, 0x80, 0x10, 0x00, 0x00, 0x00, 0x80, 0x08, 0x00, 0x20, 0x00, 0x40, 0x04, 0x00, 0x50, 0x00, 0x40, 0x04, 0x00, 0x88, 0x00, 0x20, 0x02, 0x00, 0x04, 0x01, 0x20, 0x02, 0x00, 0x02, 0x02, 0x10, 0x01, 0x00, 0x01, 0x04, 0x10, 0x01, 0x80, 0x00, 0x08, 0x10, 0x01, 0x80, 0x8f, 0x0f, 0x1f, 0x1f, 0x00, 0x88, 0x00, 0x01, 0x10, 0x00, 0x88, 0x00, 0x02, 0x08, 0x00, 0x88, 0x00, 0x04, 0x04, 0x00, 0x44, 0x00, 0x08, 0x02, 0x00, 0x44, 0x00, 0x10, 0x01, 0x00, 0x22, 0x00, 0xa0, 0x00, 0x00, 0x22, 0x00, 0x40, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x80, 0x10, 0x00, 0x00, 0x30, 0x70, 0x08, 0x00, 0x00, 0xc8, 0x0f, 0x04, 0x00, 0x00, 0x08, 0x00, 0x03, 0x00, 0x00, 0x30, 0xe0, 0x00, 0x00, 0x00, 0xc0, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const unsigned char image_wifi_12x12_bits[] U8X8_PROGMEM = {0xfc, 0x03, 0xfe, 0x07, 0x03, 0x0c, 0xf9, 0x09, 0xfc, 0x03, 0x06, 0x06, 0xf2, 0x04, 0xf8, 0x01, 0x08, 0x01, 0x60, 0x00, 0x60, 0x00, 0x00, 0x00};
static const unsigned char image_no_wifi_12x12_bits[] U8X8_PROGMEM = {0x00, 0x00, 0x06, 0x06, 0x0e, 0x07, 0x9c, 0x03, 0xf8, 0x01, 0xf0, 0x00, 0xf0, 0x00, 0xf8, 0x01, 0x9c, 0x03, 0x0e, 0x07, 0x06, 0x06, 0x00, 0x00};

static const unsigned char image_wing_close_left_21x21_bits[] U8X8_PROGMEM = {0x00, 0x00, 0x1c, 0x00, 0x70, 0x1c, 0xc0, 0x71, 0x1c, 0xc0, 0x71, 0x1c, 0xc0, 0x71, 0x1c, 0xc7, 0x71, 0x1c, 0xc7, 0x71, 0x1c, 0xc7, 0x71, 0x1c, 0xff, 0xff, 0x1f, 0xff, 0xff, 0x1f, 0xc7, 0x71, 0x1c, 0xc7, 0x71, 0x1c, 0xc7, 0x71, 0x1c, 0xc7, 0x71, 0x1c, 0xc7, 0x71, 0x1c, 0xc7, 0x71, 0x1c, 0xff, 0xff, 0x1f, 0xff, 0xff, 0x1f, 0xc7, 0x71, 0x1c, 0xc7, 0x71, 0x1c, 0xc7, 0x71, 0x1c};
static const unsigned char image_wing_open_left_9x19_bits[] U8X8_PROGMEM = {0xc0, 0x01, 0xc0, 0x01, 0xc0, 0x01, 0xc0, 0x01, 0xc7, 0x01, 0xc7, 0x01, 0xc7, 0x01, 0xff, 0x01, 0xff, 0x01, 0xc7, 0x01, 0xc7, 0x01, 0xc7, 0x01, 0xc7, 0x01, 0xc7, 0x01, 0xc7, 0x01, 0xff, 0x01, 0xff, 0x01, 0xc7, 0x01, 0xc7, 0x01};

static const unsigned char image_wing_close_right_21x21_bits[] U8X8_PROGMEM = {0x07, 0x00, 0x00, 0xc7, 0x01, 0x00, 0xc7, 0x71, 0x00, 0xc7, 0x71, 0x00, 0xc7, 0x71, 0x00, 0xc7, 0x71, 0x1c, 0xc7, 0x71, 0x1c, 0xc7, 0x71, 0x1c, 0xff, 0xff, 0x1f, 0xff, 0xff, 0x1f, 0xc7, 0x71, 0x1c, 0xc7, 0x71, 0x1c, 0xc7, 0x71, 0x1c, 0xc7, 0x71, 0x1c, 0xc7, 0x71, 0x1c, 0xc7, 0x71, 0x1c, 0xff, 0xff, 0x1f, 0xff, 0xff, 0x1f, 0xc7, 0x71, 0x1c, 0xc7, 0x71, 0x1c, 0xc7, 0x71, 0x1c};
static const unsigned char image_wing_open_right_9x19_bits[] U8X8_PROGMEM = {0x07, 0x00, 0x07, 0x00, 0x07, 0x00, 0x07, 0x00, 0xc7, 0x01, 0xc7, 0x01, 0xc7, 0x01, 0xff, 0x01, 0xff, 0x01, 0xc7, 0x01, 0xc7, 0x01, 0xc7, 0x01, 0xc7, 0x01, 0xc7, 0x01, 0xc7, 0x01, 0xff, 0x01, 0xff, 0x01, 0xc7, 0x01, 0xc7, 0x01};
static const unsigned char image_wing_unknonw_8_4x16_bits[] U8X8_PROGMEM = {0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x06, 0x06, 0x00, 0x06, 0x0f, 0x0f, 0x06};

static const unsigned char image_return_arrow_8x16_bits[] U8X8_PROGMEM = {0x18, 0xc0, 0x1c, 0xc0, 0x1e, 0xc0, 0xff, 0xff, 0xff, 0xff, 0x1e, 0x00, 0x1c, 0x00, 0x18, 0x00};

bool i2c_oled_initialized()
{
	return i2c_oled_init;
}
static uint8_t u8g2_hal_i2c_byte(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
	static i2c_cmd_handle_t int_handle_i2c = NULL;
	xSemaphoreTake(i2c_int_semaphore, pdMS_TO_TICKS(10));
	switch (msg)
	{
	case U8X8_MSG_BYTE_SEND:
		uint8_t *data_ptr = (uint8_t *)arg_ptr;
		while (arg_int-- > 0)
			ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_write_byte(int_handle_i2c, *data_ptr++, 0x1));
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
	xSemaphoreGive(i2c_int_semaphore);
	return 0;
}

static uint8_t u8g2_hal_gpio_and_delay(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
	switch (msg)
	{
	case U8X8_MSG_DELAY_MILLI:
		vTaskDelay(pdMS_TO_TICKS(arg_int));
		break;
	}
	return 0;
}

bool i2c_device_check(uint8_t i2c_master_num, uint8_t i2c_addr)
{
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, i2c_addr | I2C_MASTER_WRITE, true);
	i2c_master_stop(cmd);

	esp_err_t ret = i2c_master_cmd_begin(i2c_master_num, cmd, pdMS_TO_TICKS(1000));
	i2c_cmd_link_delete(cmd);
	return (ESP_OK == ret) ? true : false;
}

void init_i2c()
{
	i2c_config_t conf = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = I2C_INT_SDA_PIN,
		.scl_io_num = I2C_INT_SCL_PIN,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master.clk_speed = I2C_INT_FREQ,
	};
	ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &conf));
	ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0));
}

void init_i2c_oled()
{
	i2c_int_semaphore = xSemaphoreCreateBinary();
	if (!i2c_device_check(I2C_NUM_0, I2C_INT_OLED_ADDR))
	{
		ESP_LOGE(I2C_LOG_TAG, "I2C oled not found, initialization aborted");
		return;
	}
	else
	{
		ESP_LOGI(I2C_LOG_TAG, "I2C oled found");
	}

	u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R2, u8g2_hal_i2c_byte, u8g2_hal_gpio_and_delay);

	u8x8_SetI2CAddress(&u8g2.u8x8, I2C_INT_OLED_ADDR);
	u8g2_InitDisplay(&u8g2);
	u8g2_SetPowerSave(&u8g2, false);
	i2c_oled_init = true;
}

void i2c_oled_power_save(bool enable)
{
	if (!i2c_oled_init)
		return;
	static bool status = false;
	if (status != enable)
	{
		u8g2_SetPowerSave(&u8g2, enable);
		status = enable;
	}
}

void i2c_oled_home_screen(uint8_t screen_saver_time, float soc_temp, bool wifi_status, wing_state_t right_wing_state, wing_state_t left_wing_state, bool animation_toggle)
{
	if (!i2c_oled_init)
		return;
	u8g2_ClearBuffer(&u8g2);
	u8g2_SetBitmapMode(&u8g2, 1);
	u8g2_SetFontMode(&u8g2, 1);
	u8g2_SetFont(&u8g2, u8g2_font_5x8_tr);

	char screen_saver_text[4];
	snprintf(screen_saver_text, sizeof(screen_saver_text), "%i", (int)screen_saver_time);
	u8g2_DrawStr(&u8g2, 51, 9, screen_saver_text);

	char progress_text[11];
	snprintf(progress_text, sizeof(progress_text), "SOC:%.1fC", soc_temp);
	u8g2_DrawStr(&u8g2, 77, 9, progress_text);

	u8g2_DrawXBMP(&u8g2, 2, 1, 12, 12, wifi_status ? image_wifi_12x12_bits : image_no_wifi_12x12_bits);
	u8g2_DrawLine(&u8g2, 0, 13, 127, 13);

	uint8_t right_wing_str_len = u8g2_GetStrWidth(&u8g2, STATE_TO_STRING_NORMALIZED(right_wing_state));
	uint8_t left_wing_str_len = u8g2_GetStrWidth(&u8g2, STATE_TO_STRING_NORMALIZED(left_wing_state));

	u8g2_DrawStr(&u8g2, 32 - (right_wing_str_len / 2), 27, STATE_TO_STRING_NORMALIZED(right_wing_state));
	u8g2_DrawStr(&u8g2, 96 - (left_wing_str_len / 2), 27, STATE_TO_STRING_NORMALIZED(left_wing_state));

	switch (right_wing_state)
	{
	case OPENING:
	case CLOSING:
		u8g2_DrawXBMP(&u8g2, 32 - 16, 28, 36, 36, animation_toggle ? image_arrows_36x36_bits : image_arrows_180_36x36_bits);
		break;
	case OPENED:
		u8g2_DrawXBMP(&u8g2, 32 - 5, 33, 9, 19, image_wing_open_left_9x19_bits);
		break;
	case CLOSED:
		u8g2_DrawXBMP(&u8g2, 32 - 10, 33, 21, 21, image_wing_close_left_21x21_bits);
		break;
	default:
		u8g2_DrawXBMP(&u8g2, 32 - 2, 33, 4, 16, image_wing_unknonw_8_4x16_bits);
		break;
	}
	switch (left_wing_state)
	{
	case OPENING:
	case CLOSING:
		u8g2_DrawXBMP(&u8g2, 96 - 16, 28, 36, 36, animation_toggle ? image_arrows_36x36_bits : image_arrows_180_36x36_bits);
		break;
	case OPENED:
		u8g2_DrawXBMP(&u8g2, 96 - 5, 33, 9, 19, image_wing_open_right_9x19_bits);
		break;
	case CLOSED:
		u8g2_DrawXBMP(&u8g2, 96 - 10, 33, 21, 21, image_wing_close_right_21x21_bits);
		break;
	default:
		u8g2_DrawXBMP(&u8g2, 96 - 2, 33, 4, 16, image_wing_unknonw_8_4x16_bits);
		break;
	}
	u8g2_SendBuffer(&u8g2);
}

void i2c_oled_welcome_screen()
{
	if (!i2c_oled_init)
		return;
	const esp_app_desc_t *desc = esp_app_get_description();
	u8g2_ClearBuffer(&u8g2);
	u8g2_SetFont(&u8g2, u8g2_font_profont22_tr);
	u8g2_DrawStr(&u8g2, 19, 19, "Sentinel");
	u8g2_DrawLine(&u8g2, 10, 23, 117, 23);

	u8g2_SetFont(&u8g2, u8g2_font_haxrcorp4089_tr);
	u8g2_DrawStr(&u8g2, 10, 34, "SW Ver:");
	u8g2_DrawStr(&u8g2, 118 - u8g2_GetStrWidth(&u8g2, desc->version), 35, desc->version);
	u8g2_DrawStr(&u8g2, 118 - u8g2_GetStrWidth(&u8g2, desc->date), 44, desc->date);
	u8g2_SendBuffer(&u8g2);
}

void i2c_oled_ota_update(uint8_t progress)
{
	if (!i2c_oled_init)
		return;
	u8g2_ClearBuffer(&u8g2);
	u8g2_SetBitmapMode(&u8g2, 1);
	u8g2_SetFontMode(&u8g2, 1);
	u8g2_DrawFrame(&u8g2, 8, 19, 112, 20);
	u8g2_DrawBox(&u8g2, 10, 21, progress + 8, 16);
	u8g2_SetFont(&u8g2, u8g2_font_helvB08_tr);
	u8g2_DrawStr(&u8g2, 35, 14, "OTA Update");
	u8g2_DrawStr(&u8g2, 25, 51, "Progress:");
	char progress_text[5];
	snprintf(progress_text, sizeof(progress_text), "%d%%", progress);
	u8g2_DrawStr(&u8g2, 76, 51, progress_text);
	u8g2_SendBuffer(&u8g2);
}

void i2c_oled_ota_start_check()
{
	if (!i2c_oled_init)
		return;
	u8g2_ClearBuffer(&u8g2);
	u8g2_SetBitmapMode(&u8g2, 1);
	u8g2_SetFontMode(&u8g2, 1);
	u8g2_SetFont(&u8g2, u8g2_font_helvB08_tr);
	u8g2_DrawStr(&u8g2, 35, 14, "OTA Update");
	u8g2_DrawStr(&u8g2, 12, 30, "Checking for updates");
	u8g2_DrawXBMP(&u8g2, 47, 30, 36, 36, image_arrows_36x36_bits);
	u8g2_SendBuffer(&u8g2);
}
void i2c_oled_ota_update_available()
{
	if (!i2c_oled_init)
		return;
	u8g2_ClearBuffer(&u8g2);
	u8g2_SetBitmapMode(&u8g2, 1);
	u8g2_SetFontMode(&u8g2, 1);
	u8g2_SetFont(&u8g2, u8g2_font_helvB08_tr);
	u8g2_DrawStr(&u8g2, 35, 14, "OTA Update");
	u8g2_DrawStr(&u8g2, 30, 30, "Update found!");
	u8g2_SendBuffer(&u8g2);
}
void i2c_oled_ota_start_update()
{
	if (!i2c_oled_init)
		return;
	u8g2_ClearBuffer(&u8g2);
	u8g2_SetBitmapMode(&u8g2, 1);
	u8g2_SetFontMode(&u8g2, 1);
	u8g2_SetFont(&u8g2, u8g2_font_helvB08_tr);
	u8g2_DrawStr(&u8g2, 35, 14, "OTA Update");
	u8g2_DrawStr(&u8g2, 30, 30, "Starting update");
	u8g2_SendBuffer(&u8g2);
}
void i2c_oled_ota_finish_update()
{
	if (!i2c_oled_init)
		return;
	u8g2_ClearBuffer(&u8g2);
	u8g2_SetBitmapMode(&u8g2, 1);
	u8g2_SetFontMode(&u8g2, 1);
	u8g2_SetFont(&u8g2, u8g2_font_helvB08_tr);
	u8g2_DrawStr(&u8g2, 35, 14, "OTA Update");
	u8g2_DrawStr(&u8g2, 28, 30, "Update finished");
	u8g2_SendBuffer(&u8g2);
}
void i2c_oled_ota_update_failed()
{
	if (!i2c_oled_init)
		return;
	u8g2_ClearBuffer(&u8g2);
	u8g2_SetBitmapMode(&u8g2, 1);
	u8g2_SetFontMode(&u8g2, 1);
	u8g2_SetFont(&u8g2, u8g2_font_helvB08_tr);
	u8g2_DrawStr(&u8g2, 35, 14, "OTA Update");
	u8g2_DrawStr(&u8g2, 30, 30, "Update failed");
	u8g2_SendBuffer(&u8g2);
}
void i2c_oled_menaaau(uint8_t pos, const char *o1, const char *o2, const char *o3)
{
	if (!i2c_oled_init)
		return;
	u8g2_ClearBuffer(&u8g2);
	u8g2_SetBitmapMode(&u8g2, 1);
	u8g2_SetFontMode(&u8g2, 1);
	u8g2_SetFont(&u8g2, u8g2_font_helvB08_tr);
	if (pos <= 1)
	{
		u8g2_DrawStr(&u8g2, 36, 16, "Return");
		u8g2_DrawXBMP(&u8g2, 16, 8, 16, 8, image_return_arrow_8x16_bits);
		u8g2_DrawStr(&u8g2, 16, 36, o1);
		u8g2_DrawStr(&u8g2, 16, 56, o2);
		u8g2_DrawFrame(&u8g2, 4, 4 + pos * 20, 120, 16);
	}
	else
	{
		u8g2_DrawFrame(&u8g2, 4, (4 + pos * 20) - 20, 120, 16);
		u8g2_DrawStr(&u8g2, 16, 16, o1);
		u8g2_DrawStr(&u8g2, 16, 36, o2);
		u8g2_DrawStr(&u8g2, 16, 56, o3);
	}
	u8g2_SendBuffer(&u8g2);
}

void i2c_oled_menu(const char *menu_title, int pos, int arg_count, ...)
{
	if (!i2c_oled_init)
		return;

	pos = pos % (arg_count + 1);
	int menu_pos = pos % (arg_count + 1);

	va_list arg_list;
	va_start(arg_list, arg_count);

	u8g2_ClearBuffer(&u8g2);
	u8g2_SetBitmapMode(&u8g2, 1);
	u8g2_SetFontMode(&u8g2, 1);
	u8g2_SetFont(&u8g2, u8g2_font_5x8_tr);
	u8g2_DrawStr(&u8g2, 8, 8, menu_title);
	u8g2_DrawLine(&u8g2, 0, 10, 127, 10);
	u8g2_SetFont(&u8g2, u8g2_font_helvB08_tr);

	if (pos == arg_count && pos > 2)
	{
		while ((menu_pos - 1) > 2)
		{
			(void)va_arg(arg_list, char *);
			menu_pos--;
		}
		menu_pos = 3;
	}
	else
	{
		while (menu_pos > 2)
		{
			(void)va_arg(arg_list, char *);
			menu_pos--;
		}
	}

	if (menu_pos == 0)
	{
		u8g2_DrawStr(&u8g2, 29, 24, "Return");
		u8g2_DrawXBMP(&u8g2, 8, 16, 16, 8, image_return_arrow_8x16_bits);
		u8g2_DrawFrame(&u8g2, 2, 12, 124, 16);
		u8g2_DrawStr(&u8g2, 8, 40, va_arg(arg_list, char *));
		u8g2_DrawStr(&u8g2, 8, 56, va_arg(arg_list, char *));
	}
	else if (menu_pos == 1)
	{
		u8g2_DrawStr(&u8g2, 29, 24, "Return");
		u8g2_DrawXBMP(&u8g2, 8, 16, 16, 8, image_return_arrow_8x16_bits);
		u8g2_DrawStr(&u8g2, 8, 40, va_arg(arg_list, char *));
		u8g2_DrawFrame(&u8g2, 2, 12 + 1 * 16, 124, 16);
		u8g2_DrawStr(&u8g2, 8, 56, va_arg(arg_list, char *));
	}
	else if (menu_pos == 2 && arg_count == 2)
	{
		u8g2_DrawStr(&u8g2, 29, 24, "Return");
		u8g2_DrawXBMP(&u8g2, 8, 16, 16, 8, image_return_arrow_8x16_bits);
		u8g2_DrawStr(&u8g2, 8, 40, va_arg(arg_list, char *));
		u8g2_DrawStr(&u8g2, 8, 56, va_arg(arg_list, char *));
		u8g2_DrawFrame(&u8g2, 2, 12 + 2 * 16, 124, 16);
	}
	else if (menu_pos == 2)
	{
		u8g2_DrawStr(&u8g2, 8, 24, va_arg(arg_list, char *));
		u8g2_DrawStr(&u8g2, 8, 40, va_arg(arg_list, char *));
		u8g2_DrawFrame(&u8g2, 2, 12 + 1 * 16, 124, 16);
		u8g2_DrawStr(&u8g2, 8, 56, va_arg(arg_list, char *));
	}
	else if (menu_pos == 3)
	{
		u8g2_DrawStr(&u8g2, 8, 24, va_arg(arg_list, char *));
		u8g2_DrawStr(&u8g2, 8, 40, va_arg(arg_list, char *));
		u8g2_DrawStr(&u8g2, 8, 56, va_arg(arg_list, char *));
		u8g2_DrawFrame(&u8g2, 2, 12 + 2 * 16, 124, 16);
	}
	u8g2_SendBuffer(&u8g2);
	va_end(arg_list);
}


void i2c_oled_generic_info_screen(int arg_count, ...){
	va_list arg_list;
	va_start(arg_list, arg_count);

	u8g2_ClearBuffer(&u8g2);
	u8g2_SetBitmapMode(&u8g2, 1);
	u8g2_SetFontMode(&u8g2, 1);
	u8g2_SetFont(&u8g2, u8g2_font_5x8_tr);
	u8g2_DrawStr(&u8g2, 8, 8, va_arg(arg_list, char *));
	u8g2_DrawLine(&u8g2, 0, 10, 127, 10);
	u8g2_SetFont(&u8g2, u8g2_font_helvB08_tr);
	for(int i = 1; i < arg_count && i < 4; i++){
		char *str = va_arg(arg_list, char *);
		u8g2_DrawStr(&u8g2, 64 - (u8g2_GetStrWidth(&u8g2, str)/2), 24 + 16 * (i-1), str);
	}
	u8g2_SendBuffer(&u8g2);

	va_end(arg_list);

}
void i2c_oled_menu_status()
{
	if (!i2c_oled_init)
		return;
	u8g2_ClearBuffer(&u8g2);
	u8g2_SetBitmapMode(&u8g2, 1);
	u8g2_SetFontMode(&u8g2, 1);
	u8g2_SetFont(&u8g2, u8g2_font_5x8_tr);
	u8g2_DrawStr(&u8g2, 8, 8, "Nothing here :D");
	u8g2_SendBuffer(&u8g2);
}