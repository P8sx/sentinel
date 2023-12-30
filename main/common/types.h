#ifndef TYPES_H
#define TYPES_H

#include <stdio.h>
#include "stdatomic.h"

typedef enum gate_states_t {
    OPENED,
    OPENING,
    CLOSED,
    CLOSING,
    STOPPED_OPENING,
    STOPPED_CLOSING,
    UNKNOWN,
} gate_states_t;

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

typedef struct gate_command_t{
    motor_id_t id;
    gate_action_t action;
} gate_command_t;

typedef struct motor_t {
    atomic_uint_fast8_t state;
    const motor_id_t id; 
    atomic_int_fast16_t open_pcnt;
    atomic_int_fast16_t close_pcnt;
} motor_t;

#endif