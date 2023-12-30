#include "i2c.h"
#include "common/config.h"

static u8g2_t u8g2;
static u8g2_esp32_hal_t u8g2_esp32_hal = U8G2_ESP32_HAL_DEFAULT;

void init_i2c_oled()
{
    u8g2_esp32_hal.bus.i2c.sda = I2C_SDA_INT_PIN;
    u8g2_esp32_hal.bus.i2c.scl = I2C_SCL_INT_PIN;
	u8g2_esp32_hal_init(u8g2_esp32_hal);

	u8g2_Setup_ssd1306_i2c_128x64_noname_f(
		&u8g2, 
		U8G2_R2,
		u8g2_esp32_i2c_byte_cb,
		u8g2_esp32_gpio_and_delay_cb);
	
	u8x8_SetI2CAddress(&u8g2.u8x8, 0x78);
	u8g2_InitDisplay(&u8g2);

	u8g2_SetPowerSave(&u8g2, 0);
	u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_ncenB14_tr);
    u8g2_DrawStr(&u8g2, 0, 15, "Hello");

    vTaskDelay(pdMS_TO_TICKS(1500));

    u8g2_ClearBuffer(&u8g2);
    // u8g2_SetFont(&u8g2, u8g2_font_ncenB14_tr);
    // u8g2_DrawStr(&u8g2, 0, 15, "aaaaa");
	u8g2_SendBuffer(&u8g2);
}