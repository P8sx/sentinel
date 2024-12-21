#ifndef CONFIG_H
#define CONFIG_H

#include "hal/gpio_types.h"
#include "common/types.h"

void config_load();
void config_update_wing_settings(wing_id_t wing_id, bool dir, uint16_t ocp_threshold, uint16_t ocp_count);
void config_update_input_settings(input_action_t in1, input_action_t in2, input_action_t in3, input_action_t in4);
void config_update_output_settings(output_action_t out1, output_action_t out2);
void config_update_modbus_settings(uint8_t slave_id, uint8_t parity, uint32_t baudrate);
void config_update_delay_settings(bool wing_delay, bool wing_delay_dir, uint32_t wing_delay_time);
bool config_add_remote(uint64_t rf_code, input_action_t action);
void config_remove_remote(uint64_t rf_code);
bool config_check_remote(uint64_t rf_code);
uint64_t config_get_next_remote(uint64_t rf_code);

#define HW_VERSION                          "v1.0"

/* Analog PINS */
#define RIGHT_WING_SENSE_CHANNEL            ADC_CHANNEL_1           // GPIO2
#define LEFT_WING_SENSE_CHANNEL             ADC_CHANNEL_0           // GPIO1
#define ADC_ATTEN                           ADC_ATTEN_DB_2_5
#define SHUNT_VALUE                         10                      // in milliohms
#define OP_AMP_GAIN                         20                      // OP Amp gain for B1 is 20

/* RF Pins*/
#define RF_RECEIVER_PIN                     GPIO_NUM_42

/* PCNT Pins*/
#define RIGHT_WING_PULSE_PIN                GPIO_NUM_41
#define LEFT_WING_PULSE_PIN                 GPIO_NUM_40

/* PWM Pins */
#define RIGHT_WING_PWM_PIN                  GPIO_NUM_38
#define LEFT_WING_PWM_PIN                   GPIO_NUM_39

#define PWM_RESOLUTION                      LEDC_TIMER_10_BIT
#define PWM_LIMIT                           (1 << PWM_RESOLUTION) - 1

/* Digital output */
#define RIGHT_WING_RLY_A_PIN                GPIO_NUM_45
#define RIGHT_WING_RLY_B_PIN                GPIO_NUM_37
#define LEFT_WING_RLY_A_PIN                 GPIO_NUM_35
#define LEFT_WING_RLY_B_PIN                 GPIO_NUM_36

#define BUZZER_PIN                          GPIO_NUM_48

#define OUT1_PIN                            GPIO_NUM_12
#define OUT2_PIN                            GPIO_NUM_46

/* Digital input */
#define BTN1_PIN                            GPIO_NUM_0
#define BTN2_PIN                            GPIO_NUM_14
#define BTN3_PIN                            GPIO_NUM_13

#define INPUT1_PIN                          GPIO_NUM_11
#define INPUT2_PIN                          GPIO_NUM_10
#define INPUT3_PIN                          GPIO_NUM_9
#define INPUT4_PIN                          GPIO_NUM_3

#define ENDSTOP_RIGHT_WING_A_PIN            GPIO_NUM_8
#define ENDSTOP_RIGHT_WING_B_PIN            GPIO_NUM_17
#define ENDSTOP_LEFT_WING_A_PIN             GPIO_NUM_18
#define ENDSTOP_LEFT_WING_B_PIN             GPIO_NUM_16

/* Drivers */
#define RS485_TX_PIN                        GPIO_NUM_4
#define RS485_RTS_PIN                       GPIO_NUM_5
#define RS485_RX_PIN                        GPIO_NUM_6


#define I2C_EXT_SDA_PIN                     GPIO_NUM_7
#define I2C_EXT_SCL_PIN                     GPIO_NUM_15

#define I2C_INT_FREQ                        400000
#define I2C_INT_TIMEOUT_MS                  1000
#define I2C_INT_SDA_PIN                     GPIO_NUM_21
#define I2C_INT_SCL_PIN                     GPIO_NUM_47
#define I2C_INT_OLED_ADDR                   0x78

/* TCP socket config */
#define TCP_PORT                            3331
#define TCP_KEEPALIVE_IDLE                  5
#define TCP_KEEPALIVE_INTERVAL              5
#define TCP_KEEPALIVE_COUNT                 3

/* WIFI */
#define WIFI_MAXIMUM_RETRY                  INT32_MAX
// #define WIFI_SSID                       "hehe"
// #define WIFI_PASSWORD                   "not hehe"

/* LOG */
#define WIFI_LOG_TAG                        "WIFI"
#define I2C_LOG_TAG                         "I2C"
#define TCP_LOG_TAG                         "TCP"
#define GATE_CONTROL_LOG_TAG                "MT-CTRL"
#define GATE_LOG_TAG                        "MT"
#define GHOTA_LOG_TAG                       "GHOTA"
#define CONTROL_LOG_TAG                     "CONTROL"
#define UI_LOG_TAG                          "UI"
#define CFG_LOG_TAG                         "CFG"
#define MQTT_LOG_TAG                        "MQTT"
#define IO_LOG_TAG                          "IO"
#define RF_LOG_TAG                          "RF"
#define MODBUS_LOG_TAG                      "MODBUS"

/* Config keys */
#define CFG_NAMESPACE                       "cfg-nmspace"

#define CFG_WIFI_SSID                       "WIFI_SSID"
#define CFG_WIFI_PASSWORD                   "WIFI_PASSWORD"

#define CFG_RIGHT_WING_DIR                  "RW_DIR"
#define CFG_RIGHT_WING_OCP_COUNT            "RW_OCP_COUNT"
#define CFG_RIGHT_WING_OCP_THRESHOLD        "RW_OCP_THR"

#define CFG_LEFT_WING_DIR                   "LW_DIR"
#define CFG_LEFT_WING_OCP_THRESHOLD         "LW_OCP_THR"
#define CFG_LEFT_WING_OCP_COUNT             "LW_OCP_COUNT"

#define CFG_WING_DELAY                      "W_DEL"
#define CFG_WING_DELAY_DIR                  "W_DEL_DIR"
#define CFG_WING_DELAY_TIME                 "W_DEL_TIME"

#define CFG_INPUT_ACTIONS                   "INPUT_ACTIONS"
#define CFG_OUTPUT_ACTIONS                  "OUTPUT_ACTIONS"

#define CFG_MQTT_URI                        "MQTT_URI"
#define CFG_MQTT_PASSWORD                   "MQTT_PASSWORD"
#define CFG_MQTT_USERNAME                   "MQTT_USERNAME"
#define CFG_MQTT_PORT                       "MQTT_PORT"

#define CFG_HW_OPTIONS                      "HW_OPTIONS"
#define CFG_RF_LIST                         "RF_LIST"

#define CFG_MODBUS_BAUDRATE                 "MB_BAUD"
#define CFG_MODBUS_PARITY                   "MB_PARITY"
#define CFG_MODBUS_SLAVE_ID                 "MB_ID"

/* HW_OPTIONS */

#define STATIC_CFG_NUM_OF_REMOTES           16
#define HW_RF_ENABLED                       0x01


extern device_config_t device_config;
extern rf_remote_config_t remotes_config[STATIC_CFG_NUM_OF_REMOTES];



#endif
