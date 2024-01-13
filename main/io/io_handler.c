#include "io.h"
#include "esp_log.h"
#include "common/config.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "gate.h"
#include "drivers/i2c.h"
#include "io.h"
#include "esp_event.h"
#include "esp_err.h"

extern QueueHandle_t gate_action_queue;
void io_handler_gate_action_handle(void *args);
void io_handler_gate_action_unknown_handle(void *args);

struct input_lookup_table_t {
    enum input_action_t action;
    void (*func)(void *);
} input_lookup_table[] = {
    {M1_OPEN,          io_handler_gate_action_handle},
    {M2_OPEN,          io_handler_gate_action_handle},
    {M1M2_OPEN,        io_handler_gate_action_handle},
    {M1_CLOSE,         io_handler_gate_action_handle},
    {M2_CLOSE,         io_handler_gate_action_handle},
    {M1M2_CLOSE,       io_handler_gate_action_handle},
    {M1_STOP,          io_handler_gate_action_handle},
    {M2_STOP,          io_handler_gate_action_handle},
    {M1M2_STOP,        io_handler_gate_action_handle},
    {M1_NEXT_STATE,    io_handler_gate_action_handle},
    {M2_NEXT_STATE,    io_handler_gate_action_handle},
    {M1M2_NEXT_STATE,  io_handler_gate_action_handle},
    {UNKNOWN_ACTION,   io_handler_gate_action_unknown_handle}
    /* More input actions */
};

struct output_lookup_table_t {
    enum output_action_t action;
    void (*func)(void *);
} output_lookup_table[] = {
    {M1_MOVING_BLINK,           io_handler_gate_action_unknown_handle},
    {M2_MOVING_BLINK,           io_handler_gate_action_unknown_handle},
    {M1M2_MOVING_BLINK,         io_handler_gate_action_unknown_handle},
    {M1_MOVING_ON,              io_handler_gate_action_unknown_handle},
    {M2_MOVING_ON,              io_handler_gate_action_unknown_handle},
    {M1M2_MOVING_ON,            io_handler_gate_action_unknown_handle},
    /* More output actions */
};


static void io_handler_gate_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if(MOTOR_STATUS_CHANGED == event_id){
        // gate_status_t *status = (gate_status_t *)event_data;
        
    }
}

static void io_handler_io_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    static const uint8_t inputs[] = {INPUT1_PIN, INPUT2_PIN, INPUT3_PIN, INPUT4_PIN};
    if(IO_INPUT_TRIGGERED_EVENT == event_id){
        uint8_t io = *(uint8_t *)event_data;
        switch(io){
            case INPUT1_PIN:
            case INPUT2_PIN:
            case INPUT3_PIN:
            case INPUT4_PIN:
                uint8_t input_action = 0;
                for(size_t i = 0; i<4; i++){
                    if(inputs[i] == io) input_action = device_config.input_actions[i];
                }
                for (size_t i = 0; i < sizeof(input_lookup_table) / sizeof(input_lookup_table[0]); ++i){
                    if(input_lookup_table[i].action == input_action){
                        input_lookup_table[i].func(&input_action);
                        break;
                    }
                }
                break;
            case ENDSTOP_M1_A_PIN:
            case ENDSTOP_M1_B_PIN:
                xQueueSend(gate_action_queue, &GATE_CMD(HW_STOP, M1), pdMS_TO_TICKS(100));
                break;
            case ENDSTOP_M2_A_PIN:
            case ENDSTOP_M2_B_PIN:
                xQueueSend(gate_action_queue, &GATE_CMD(HW_STOP, M2), pdMS_TO_TICKS(100));
                break;
        }
    }
}

void io_handler_init(){
    ESP_ERROR_CHECK(esp_event_handler_instance_register(GATE_EVENTS, ESP_EVENT_ANY_ID, &io_handler_gate_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IO_EVENTS, ESP_EVENT_ANY_ID, &io_handler_io_event_handler, NULL, NULL));
}

void io_handler_gate_action_handle(void *args){
    gate_command_t cmd = INPUT_ACTION_TO_GATE_COMMAND(*(uint8_t *)args);
    ESP_LOGI(CONTROL_LOG_TAG,"Input ation called: %i for motor:%i", cmd.action, cmd.id);
    xQueueSend(gate_action_queue, &cmd, pdMS_TO_TICKS(100));
}

void io_handler_gate_action_unknown_handle(void *args){
    ESP_LOGE(CONTROL_LOG_TAG,"Unknown action called");
}
