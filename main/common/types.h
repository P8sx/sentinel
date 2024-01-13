#ifndef TYPES_H
#define TYPES_H

#include <stdio.h>
#include "stdatomic.h"
#include "stdbool.h"

#define GATE_CMD(cmd_action, cmd_id) (gate_command_t){ .action = cmd_action, .id = cmd_id }
#define STATE_STRING(num)  ((const char *[]){"OPENED","OPENING","CLOSED","CLOSING","STOPPED_OPENING","STOPPED_CLOSING", "UNKNOWN"}[(num%8)])
#define STATE_STRING_NORMALIZED(num)  ((const char *[]){"Opened","Opening","Closed","Closing","Stopped","Stopped", "Unknown"}[(num%8)])
#define INPUT_ACTION_TO_GATE_COMMAND(inputAction)  ((gate_command_t) { .id = (motor_id_t)(inputAction & 0x0F), .action = (gate_action_t)(inputAction & 0xF0)})


typedef enum motor_id_t {
    M1      = 0x01,
    M2      = 0x02,
    M1M2    = 0x03,
} motor_id_t;

typedef enum gate_action_t {
    OPEN        = 0x10,
    CLOSE       = 0x20,
    STOP        = 0x30,
    NEXT_STATE  = 0x40,
    HW_STOP     = 0x50,
} gate_action_t;

typedef enum gate_state_t {
    OPENED,
    OPENING,
    CLOSED,
    CLOSING,
    STOPPED_OPENING,
    STOPPED_CLOSING,
    UNKNOWN,
} gate_state_t;


typedef struct gate_status_t{
    motor_id_t id;
    gate_state_t state;
} gate_status_t;

typedef struct gate_command_t{
    motor_id_t id;
    gate_action_t action;
} gate_command_t;



typedef enum input_action_t {
    M1_OPEN             = M1 | OPEN,
    M2_OPEN             = M2 | OPEN,
    M1M2_OPEN           = M1M2 | OPEN,
    M1_CLOSE            = M1 | CLOSE,
    M2_CLOSE            = M2 | CLOSE,
    M1M2_CLOSE          = M1M2 | CLOSE,
    M1_STOP             = M1 | STOP,
    M2_STOP             = M2 | STOP,
    M1M2_STOP           = M1M2 | STOP,
    M1_NEXT_STATE       = M1 | NEXT_STATE,
    M2_NEXT_STATE       = M2 | NEXT_STATE,
    M1M2_NEXT_STATE     = M1M2 | NEXT_STATE,
    UNKNOWN_ACTION      = 0xFF,
} input_action_t;

typedef enum output_action_t{
    M1_MOVING_BLINK,
    M2_MOVING_BLINK,
    M1M2_MOVING_BLINK,
    M1_MOVING_ON,
    M2_MOVING_ON,
    M1M2_MOVING_ON,
} output_action_t;

typedef struct device_config_t{
    char wifi_ssid[32];
    char wifi_password[64];
    char mqtt_server[64];
    char mqtt_password[64];
    char mqtt_username[64];

    bool m1_dir;
    bool m2_dir;

    uint16_t m1_ocp_treshold;
    uint16_t m2_ocp_treshold;
    uint16_t m1_ocp_count;
    uint16_t m2_ocp_count;

    uint8_t input_actions[4];
    uint8_t output_actions[2];

} device_config_t;


typedef struct rf_remote_config_t{
    uint64_t id;
    uint8_t action; 
} rf_remote_config_t;

#endif