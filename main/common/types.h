#ifndef TYPES_H
#define TYPES_H

#include <stdio.h>
#include "stdatomic.h"
#include "stdbool.h"

static const char *STATES_STRING[] = {"OPENED","OPENING","CLOSED","CLOSING","STOPPED_OPENING","STOPPED_CLOSING", "UNKNOWN"};

typedef enum motor_id_t {
    M1 = 1,
    M2 = 2,
} motor_id_t;

typedef enum gate_action_t{
    OPEN,
    CLOSE,
    STOP,
    NEXT_STATE,
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
    M1_OPEN,
    M2_OPEN,
    M_OPEN,
    M1_CLOSE,
    M2_CLOSE,
    M_CLOSE,
    M1_STOP,
    M2_STOP,
    M_STOP,
    M1_NEXT_STATE,
    M2_NEXT_STATE,
    M_NEXT_STATE,
} input_action_t;


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

    uint8_t input_actions[8];
    uint8_t output_actions[2];

} device_config_t;


typedef struct rf_remote_config_t{
    uint64_t id;
    uint8_t action; 
} rf_remote_config_t;

#endif