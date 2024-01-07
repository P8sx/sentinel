#include "control.h"
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
#include "esp_ghota.h"

static TimerHandle_t screen_saver_timer = NULL;
static SemaphoreHandle_t control_oled_task_mutex = NULL;
extern TaskHandle_t control_oled_task_handle;
extern QueueHandle_t gate_action_queue;
QueueHandle_t input_queue = NULL;


static void control_ghota_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_id == GHOTA_EVENT_FIRMWARE_UPDATE_PROGRESS) {
        xTimerReset(screen_saver_timer, 0);
        i2c_oled_power_save(false);
        i2c_oled_ota_update(*((int*) event_data));
    } else if(event_id == GHOTA_EVENT_START_CHECK){
        i2c_oled_ota_start_check();
    } else if(event_id == GHOTA_EVENT_UPDATE_AVAILABLE){
        i2c_oled_ota_update_available();
    } else if(event_id == GHOTA_EVENT_START_UPDATE){
        i2c_oled_ota_start_update();
    } else if(event_id == GHOTA_EVENT_FINISH_UPDATE){
        i2c_oled_ota_finish_update();
    } else if(event_id == GHOTA_EVENT_UPDATE_FAILED){
        i2c_oled_ota_update_failed();
    }
}

static void control_gate_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  
}


void screen_saver_timer_callback(TimerHandle_t xTimer) {
    i2c_oled_power_save(true);
    /* Take semaphore to lock task */
    xSemaphoreTake(control_oled_task_mutex, 0);
}

void control_init(){
    screen_saver_timer = xTimerCreate("screen_saver", pdMS_TO_TICKS(60*1000), pdFALSE, 0, screen_saver_timer_callback);
    control_oled_task_mutex = xSemaphoreCreateBinary();
    input_queue = xQueueCreate(5, sizeof(uint8_t));

    xSemaphoreGive(control_oled_task_mutex);
    xTimerStart(screen_saver_timer, 0);

    ESP_ERROR_CHECK(esp_event_handler_instance_register(GHOTA_EVENTS, ESP_EVENT_ANY_ID, &control_ghota_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(GATE_EVENTS, ESP_EVENT_ANY_ID, &control_gate_event_handler, NULL, NULL));

}


void control_oled_handling_task(void *pvParameters){
    i2c_oled_power_save(false);
	i2c_oled_welcome_screen();
    while(1){
        if(uxSemaphoreGetCount(control_oled_task_mutex) == 0){
            ESP_LOGI("CONTROL","Suspending itself");
            vTaskSuspend(NULL);
            i2c_oled_power_save(false);
        }
        ESP_LOGI("CONTROL","SR-OLED");
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

void control_gate_action_handle(void *args){
    gate_command_t cmd = INPUT_ACTION_TO_GATE_COMMAND(*(uint8_t *)args);
    ESP_LOGI(CONTROL_LOG_TAG,"Input ation called: %i for motor:%i", cmd.action, cmd.id);
    xQueueSend(gate_action_queue, &cmd, pdMS_TO_TICKS(100));
}

void control_gate_action_unknown_handle(void *args){
    ESP_LOGE(CONTROL_LOG_TAG,"Unknown action called");
}

struct input_lookup_table_t {
    enum input_action_t action;
    void (*func)(void *);
} input_lookup_table[] = {
    {M1_OPEN,          control_gate_action_handle},
    {M2_OPEN,          control_gate_action_handle},
    {M1M2_OPEN,        control_gate_action_handle},
    {M1_CLOSE,         control_gate_action_handle},
    {M2_CLOSE,         control_gate_action_handle},
    {M1M2_CLOSE,       control_gate_action_handle},
    {M1_STOP,          control_gate_action_handle},
    {M2_STOP,          control_gate_action_handle},
    {M1M2_STOP,        control_gate_action_handle},
    {M1_NEXT_STATE,    control_gate_action_handle},
    {M2_NEXT_STATE,    control_gate_action_handle},
    {M1M2_NEXT_STATE,  control_gate_action_handle},
    {UNKNOWN_ACTION,   control_gate_action_unknown_handle}
    /* More input actions */
};

struct output_lookup_table_t {
    enum output_action_t action;
    void (*func)(void *);
} output_lookup_table[] = {
    {M1_MOVING_BLINK,           control_gate_action_unknown_handle},
    {M2_MOVING_BLINK,           control_gate_action_unknown_handle},
    {M1M2_MOVING_BLINK,         control_gate_action_unknown_handle},
    {M1_MOVING_ON,              control_gate_action_unknown_handle},
    {M2_MOVING_ON,              control_gate_action_unknown_handle},
    {M1M2_MOVING_ON,            control_gate_action_unknown_handle},
    /* More output actions */
};


void control_input_handling_task(void *pvParameters){
    static uint8_t io = 0;
    while (true){
        if(xQueueReceive(input_queue, &io, portMAX_DELAY)){
            switch(io){
                case BTN1_PIN:
                case BTN2_PIN:
                case BTN3_PIN:
                    if(!i2c_oled_init_state()) break;
                    xTimerReset(screen_saver_timer, 0);
                    xSemaphoreGive(control_oled_task_mutex);
                    vTaskResume(control_oled_task_handle);
                    break;
                case INPUT1_PIN:
                case INPUT2_PIN:
                case INPUT3_PIN:
                case INPUT4_PIN:
                    static const uint8_t inputs[] = {INPUT1_PIN, INPUT2_PIN, INPUT3_PIN, INPUT4_PIN};
                    uint8_t input_action = 0;
                    for(size_t i = 0; i<4; i++){
                        if(inputs[i] == io){
                            input_action = device_config.input_actions[i];
                        }
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
        taskYIELD();
    }
    
}