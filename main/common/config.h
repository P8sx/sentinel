#include "hal/gpio_types.h"

#define HW_VERSION 1.0
#define SW_VERSION 0.1

/* Analog PINS */
#define M1_SENSE_PIN            GPIO_NUM_1
#define M2_SENSE_PIN            GPIO_NUM_2

/* RMT Pins*/
#define RF_RECEIVER_PIN         GPIO_NUM_42

/* PCNT Pins*/
#define M1_PULSE_PIN            GPIO_NUM_41
#define M2_PULSE_PIN            GPIO_NUM_40

/* PWM Pins */
#define M1_PWM_PIN              GPIO_NUM_39
#define M2_PWM_PIN              GPIO_NUM_38

/* Digital output */
#define M1_RLY_A_PIN            GPIO_NUM_45
#define M1_RLY_B_PIN            GPIO_NUM_37
#define M2_RLY_A_PIN            GPIO_NUM_36
#define M2_RLY_B_PIN            GPIO_NUM_35

#define BUZZER_PIN              GPIO_NUM_48

#define OUT1_PIN                GPIO_NUM_12
#define OUT2_PIN                GPIO_NUM_46

/* Digital input */
#define BTN1_PIN                GPIO_NUM_0
#define BTN2_PIN                GPIO_NUM_14
#define BTN3_PIN                GPIO_NUM_13

#define INPUT1_PIN              GPIO_NUM_3
#define INPUT2_PIN              GPIO_NUM_9
#define INPUT3_PIN              GPIO_NUM_10
#define INPUT4_PIN              GPIO_NUM_11

#define INPUT5_PIN              GPIO_NUM_8
#define INPUT6_PIN              GPIO_NUM_18
#define INPUT7_PIN              GPIO_NUM_17
#define INPUT8_PIN              GPIO_NUM_16


/* Drivers */
#define RS485_TX_PIN            GPIO_NUM_4
#define RS485_EN_PIN            GPIO_NUM_5
#define RS485_RX_PIN            GPIO_NUM_6

#define I2C_SDA_EXT_PIN         GPIO_NUM_7
#define I2C_SCL_EXT_PIN         GPIO_NUM_15

#define I2C_SDA_INT_PIN         GPIO_NUM_21
#define I2C_SCL_INT_PIN         GPIO_NUM_47
