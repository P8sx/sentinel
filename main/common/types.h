#ifndef TYPES_H
#define TYPES_H

#include <stdio.h>
#include "stdatomic.h"
#include "stdbool.h"

#define WING_CMD(cmd_action, cmd_id) \
    (wing_command_t) { .action = cmd_action, .id = cmd_id }
#define STATE_STRING(num) ((const char *[]){"OPENED", "OPENING", "CLOSED", "CLOSING", "STOPPED_OPENING", "STOPPED_CLOSING", "UNKNOWN"}[(num % 8)])
#define STATE_STRING_NORMALIZED(num) ((const char *[]){"Opened", "Opening", "Closed", "Closing", "Stopped", "Stopped", "Unknown"}[(num % 8)])
#define INPUT_ACTION_TO_GATE_COMMAND(inputAction) ((wing_command_t){.id = (wing_id_t)(inputAction & 0x0F), .action = (wing_action_t)(inputAction & 0xF0)})

typedef enum wing_id_t
{
    RIGHT_WING      = 0x01,
    LEFT_WING       = 0x02,
    BOTH_WING       = 0x03,
} wing_id_t;

typedef enum wing_action_t
{
    OPEN            = 0x10,
    CLOSE           = 0x20,
    STOP            = 0x30,
    NEXT_STATE      = 0x40,
    HW_STOP         = 0x50,
} wing_action_t;

typedef enum wing_state_t
{
    OPENED,
    OPENING,
    CLOSED,
    CLOSING,
    STOPPED_OPENING,
    STOPPED_CLOSING,
    UNKNOWN,
} wing_state_t;

typedef struct wing_info_t
{
    wing_id_t id;
    wing_state_t state;
} wing_info_t;

typedef struct wing_command_t
{
    wing_id_t id;
    wing_action_t action;
} wing_command_t;

typedef enum input_action_t
{
    RIGHT_WING_OPEN         = RIGHT_WING    | OPEN,
    LEFT_WING_OPEN          = LEFT_WING     | OPEN,
    BOTH_WING_OPEN          = BOTH_WING     | OPEN,
    RIGHT_WING_CLOSE        = RIGHT_WING    | CLOSE,
    LEFT_WING_CLOSE         = LEFT_WING     | CLOSE,
    BOTH_WING_CLOSE         = BOTH_WING     | CLOSE,
    RIGHT_WING_STOP         = RIGHT_WING    | STOP,
    LEFT_WING_STOP          = LEFT_WING     | STOP,
    BOTH_WING_STOP          = BOTH_WING     | STOP,
    RIGHT_WING_NEXT_STATE   = RIGHT_WING    | NEXT_STATE,
    LEFT_WING_NEXT_STATE    = LEFT_WING     | NEXT_STATE,
    BOTH_WING_NEXT_STATE    = BOTH_WING     | NEXT_STATE,
    UNKNOWN_ACTION          = 0xFF,
} input_action_t;

typedef enum output_action_t
{
    RIGHT_WING_MOVING_BLINK,
    LEFT_WING_MOVING_BLINK,
    BOTH_WING_MOVING_BLINK,
    RIGHT_WING_MOVING_ON,
    LEFT_WING_MOVING_ON,
    BOTH_WING_MOVING_ON,
} output_action_t;

typedef struct device_config_t
{
    char wifi_ssid[32];
    char wifi_password[64];

    char mqtt_uri[64];
    char mqtt_password[64];
    char mqtt_username[64];

    char device_name[64];

    uint32_t mqtt_port;

    bool right_wing_dir;
    bool left_wing_dir;

    uint16_t right_wing_ocp_threshold;
    uint16_t left_wing_ocp_threshold;
    uint16_t right_wing_ocp_count;
    uint16_t left_wing_ocp_count;

    uint8_t input_actions[4];
    uint8_t output_actions[2];

    char sw_version[32];
    char hw_version[32];
} device_config_t;

typedef struct rf_remote_config_t
{
    uint64_t id;
    uint8_t action;
} rf_remote_config_t;

#endif