#ifndef TYPES_H
#define TYPES_H

#include <stdio.h>
#include "stdatomic.h"
#include "stdbool.h"

#define WING_CMD(cmd_action, cmd_id)  (wing_command_t) { .action = cmd_action, .id = cmd_id }
#define INPUT_ACTION_TO_GATE_COMMAND(inputAction) ((wing_command_t){.id = (wing_id_t)(inputAction & 0x0F), .action = (wing_action_t)(inputAction & 0xF0)})

#define STATE_TO_STRING(num) ((const char *[]){"OPENED", "OPENING", "CLOSED", "CLOSING", "STOPPED_OPENING", "STOPPED_CLOSING", "UNKNOWN"}[(num % 8)])
#define STATE_TO_STRING_NORMALIZED(num) ((const char *[]){"Opened", "Opening", "Closed", "Closing", "Stopped", "Stopped", "Unknown"}[(num % 8)])

#define INPUT_ACTION_TO_STRING(action) \
    (action == RIGHT_WING_OPEN ? "RW Open" : \
    action == LEFT_WING_OPEN ? "LW Open" : \
    action == BOTH_WING_OPEN ? "BW Open" : \
    action == RIGHT_WING_CLOSE ? "RW Close" : \
    action == LEFT_WING_CLOSE ? "LW Close" : \
    action == BOTH_WING_CLOSE ? "BW Close" : \
    action == RIGHT_WING_STOP ? "RW Stop" : \
    action == LEFT_WING_STOP ? "LW Stop" : \
    action == BOTH_WING_STOP ? "BW Stop" : \
    action == RIGHT_WING_NEXT_STATE ? "RW Next State" : \
    action == LEFT_WING_NEXT_STATE ? "LW Next State" : \
    action == BOTH_WING_NEXT_STATE ? "BW Next State" : \
    "No Action")
#define STRING_TO_INPUT_ACTION(str) \
    (std::strcmp(str, "RW Open") == 0 ? RIGHT_WING_OPEN : \
    std::strcmp(str, "LW Open") == 0 ? LEFT_WING_OPEN : \
    std::strcmp(str, "BW Open") == 0 ? BOTH_WING_OPEN : \
    std::strcmp(str, "RW Close") == 0 ? RIGHT_WING_CLOSE : \
    std::strcmp(str, "LW Close") == 0 ? LEFT_WING_CLOSE : \
    std::strcmp(str, "BW Close") == 0 ? BOTH_WING_CLOSE : \
    std::strcmp(str, "RW Stop") == 0 ? RIGHT_WING_STOP : \
    std::strcmp(str, "LW Stop") == 0 ? LEFT_WING_STOP : \
    std::strcmp(str, "BW Stop") == 0 ? BOTH_WING_STOP : \
    std::strcmp(str, "RW Next State") == 0 ? RIGHT_WING_NEXT_STATE : \
    std::strcmp(str, "LW Next State") == 0 ? LEFT_WING_NEXT_STATE : \
    std::strcmp(str, "BW Next State") == 0 ? BOTH_WING_NEXT_STATE : \
    INPUT_UNKNOWN_ACTION)

#define NEXT_INPUT_ACTION(action) \
    ((action == RIGHT_WING_OPEN) ? LEFT_WING_OPEN : \
    (action == LEFT_WING_OPEN) ? BOTH_WING_OPEN : \
    (action == BOTH_WING_OPEN) ? RIGHT_WING_CLOSE : \
    (action == RIGHT_WING_CLOSE) ? LEFT_WING_CLOSE : \
    (action == LEFT_WING_CLOSE) ? BOTH_WING_CLOSE : \
    (action == BOTH_WING_CLOSE) ? RIGHT_WING_STOP : \
    (action == RIGHT_WING_STOP) ? LEFT_WING_STOP : \
    (action == LEFT_WING_STOP) ? BOTH_WING_STOP : \
    (action == BOTH_WING_STOP) ? RIGHT_WING_NEXT_STATE : \
    (action == RIGHT_WING_NEXT_STATE) ? LEFT_WING_NEXT_STATE : \
    (action == LEFT_WING_NEXT_STATE) ? BOTH_WING_NEXT_STATE : \
    (action == BOTH_WING_NEXT_STATE) ? INPUT_UNKNOWN_ACTION : \
    RIGHT_WING_OPEN)


#define OUTPUT_ACTION_TO_STRING(action) \
    (action == RIGHT_WING_BLINK ? "RW Blink" : \
    action == LEFT_WING_BLINK ? "LW Blink" : \
    action == ANY_WING_BLINK ? "Any Blink" : \
    action == RIGHT_WING_ON ? "RW On" : \
    action == LEFT_WING_ON ? "LW On" : \
    action == ANY_WING_ON ? "Any On" : \
    "No Action")

#define STRING_TO_OUTPUT_ACTION(str) \
    (std::strcmp(str, "RW Blink") == 0 ? RIGHT_WING_BLINK_WHEN_MOVING : \
    std::strcmp(str, "LW Blink") == 0 ? LEFT_WING_BLINK : \
    std::strcmp(str, "Any Blink") == 0 ? ANY_WING_BLINK : \
    std::strcmp(str, "RW On") == 0 ? RIGHT_WING_ON : \
    std::strcmp(str, "LW On") == 0 ? LEFT_WING_ON : \
    std::strcmp(str, "Any On") == 0 ? ANY_WING_ON : \
    OUTPUT_UNKNOWN_ACTION)

#define NEXT_OUTPUT_ACTION(action) \
    ((action == RIGHT_WING_BLINK) ? LEFT_WING_BLINK : \
    (action == LEFT_WING_BLINK) ? ANY_WING_BLINK : \
    (action == ANY_WING_BLINK) ? RIGHT_WING_ON : \
    (action == RIGHT_WING_ON) ? LEFT_WING_ON : \
    (action == LEFT_WING_ON) ? ANY_WING_ON : \
    (action == ANY_WING_ON) ? OUTPUT_UNKNOWN_ACTION : \
    RIGHT_WING_BLINK)


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
    INPUT_UNKNOWN_ACTION    = 0xFF,
} input_action_t;



typedef enum output_action_t
{
    RIGHT_WING_BLINK,
    LEFT_WING_BLINK,
    ANY_WING_BLINK,
    RIGHT_WING_ON,
    LEFT_WING_ON,
    ANY_WING_ON,
    OUTPUT_UNKNOWN_ACTION          = 0xFF,
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

    uint16_t hw_options;

    uint8_t modbus_slave_id;
    uint8_t modbus_parity;
    uint32_t modbus_baudrate;

    bool wing_delay;
    bool wing_delay_dir;
    uint32_t wing_delay_time;
    
} device_config_t;

typedef struct rf_remote_config_t
{
    uint64_t id;
    uint8_t action;
} rf_remote_config_t;

#endif