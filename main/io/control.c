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

static TimerHandle_t screen_saver_timer = NULL;
static SemaphoreHandle_t control_oled_task_mutex = NULL;
extern TaskHandle_t control_oled_task_handle;
extern QueueHandle_t gate_action_queue;
QueueHandle_t input_queue = NULL;

void screen_saver_timer_callback(TimerHandle_t xTimer) {
    i2c_oled_power_saver(true);
    /* Take semaphore to lock task */
    xSemaphoreTake(control_oled_task_mutex, 0);
}

void control_init(){
    screen_saver_timer = xTimerCreate("screen_saver", pdMS_TO_TICKS(60*1000), pdFALSE, 0, screen_saver_timer_callback);
    control_oled_task_mutex = xSemaphoreCreateBinary();
    input_queue = xQueueCreate(5, sizeof(uint8_t));

    xSemaphoreGive(control_oled_task_mutex);
    xTimerStart(screen_saver_timer, 0);
}


void control_oled_handling_task(void *pvParameters){
    i2c_oled_power_saver(false);
	i2c_oled_welcome_screen();
    while(1){
        if(uxSemaphoreGetCount(control_oled_task_mutex) == 0){
            ESP_LOGI("CONTROL","Suspending itself");
            vTaskSuspend(NULL);
            i2c_oled_power_saver(false);
        }
        ESP_LOGI("CONTROL","SR-OLED");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}


void control_input_handling_task(void *pvParameters){
    uint8_t io = 0;
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
                    if(device_config.input_actions[0] != UNKNOWN_ACTION){
                        xQueueSend(gate_action_queue, &INPUT_ACTION_TO_GATE_COMMAND(device_config.input_actions[0]), pdMS_TO_TICKS(100));
                    }
                    break;
                case INPUT2_PIN:
                    if(device_config.input_actions[1] != UNKNOWN_ACTION){
                        xQueueSend(gate_action_queue, &INPUT_ACTION_TO_GATE_COMMAND(device_config.input_actions[1]), pdMS_TO_TICKS(100));
                    }
                    break;
                case INPUT3_PIN:
                    if(device_config.input_actions[2] != UNKNOWN_ACTION){
                        xQueueSend(gate_action_queue, &INPUT_ACTION_TO_GATE_COMMAND(device_config.input_actions[2]), pdMS_TO_TICKS(100));
                    }
                    break;
                case INPUT4_PIN:
                    if(device_config.input_actions[3] != UNKNOWN_ACTION){
                        xQueueSend(gate_action_queue, &INPUT_ACTION_TO_GATE_COMMAND(device_config.input_actions[3]), pdMS_TO_TICKS(100));
                    }
                    break;
                case ENDSTOP_M1_A_PIN:
                case ENDSTOP_M1_B_PIN:
                    xQueueSend(gate_action_queue, &GATE_CMD(HW_STOP, M1), pdMS_TO_TICKS(100));
                    break;
                case ENDSTOP_M2_A_PIN:
                case ENDSTOP_M2_B_PIN:
                    xQueueSend(gate_action_queue, &GATE_CMD(HW_STOP, M1), pdMS_TO_TICKS(100));
                    break;
            }
        }
        taskYIELD();
    }
    
}