#ifndef CONFIG_H
#define CONFIG_H

#include "hal/gpio_types.h"
#include "common/types.h"

#define HW_VERSION 1.0
#define SW_VERSION 0.1

extern device_config_t device_config;


/* Analog PINS */
#define M1_SENSE_CHANNEL        ADC_CHANNEL_1              // GPIO2
#define M2_SENSE_CHANNEL        ADC_CHANNEL_0              // GPIO1
#define ADC_ATTEN               ADC_ATTEN_DB_2_5
#define SHUNT_VALUE             10                          // in milliohms
#define OP_AMP_GAIN             20

/* RF Pins*/
#define RF_RECEIVER_PIN         GPIO_NUM_42

/* PCNT Pins*/
#define M1_PULSE_PIN            GPIO_NUM_41
#define M2_PULSE_PIN            GPIO_NUM_40

/* PWM Pins */
#define M1_PWM_PIN              GPIO_NUM_38
#define M2_PWM_PIN              GPIO_NUM_39

#define PWM_RESOLUTION          LEDC_TIMER_10_BIT
#define PWM_LIMIT               (1 << PWM_RESOLUTION) - 1

/* Digital output */
#define M1_RLY_A_PIN            GPIO_NUM_45
#define M1_RLY_B_PIN            GPIO_NUM_37
#define M2_RLY_A_PIN            GPIO_NUM_35
#define M2_RLY_B_PIN            GPIO_NUM_36

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

#define I2C_EXT_SDA_PIN         GPIO_NUM_7
#define I2C_EXT_SCL_PIN         GPIO_NUM_15

#define I2C_INT_FREQ            100000
#define I2C_INT_TIMEOUT_MS      1000
#define I2C_INT_SDA_PIN         GPIO_NUM_21
#define I2C_INT_SCL_PIN         GPIO_NUM_47
#define I2C_INT_OLED_ADDR       0x78

/* TCP socket config */
#define TCP_PORT                        3331
#define TCP_KEEPALIVE_IDLE              5
#define TCP_KEEPALIVE_INTERVAL          5
#define TCP_KEEPALIVE_COUNT             3

/* WIFI */
#define WIFI_MAXIMUM_RETRY              128
// #define WIFI_SSID                       "hehe"
// #define WIFI_PASSWORD                   "not hehe"


/* LOG */
#define WIFI_LOG_TAG            "WIFI"
#define I2C_LOG_TAG             "I2C"
#define TCP_LOG_TAG             "TCP"
#define MOTOR_CONTROL_LOG_TAG   "MT-CTRL"
#define MOTOR_LOG_TAG           "MT"

#endif