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

static TimerHandle_t screen_saver_timer = NULL;
static SemaphoreHandle_t control_input_task_mutex = NULL;
extern TaskHandle_t control_input_task_handle;

void screen_saver_timer_callback(TimerHandle_t xTimer) {
    i2c_oled_power_saver(true);
    xSemaphoreTake(control_input_task_mutex, 0);
}

void control_init(){
    screen_saver_timer = xTimerCreate("screen_saver", pdMS_TO_TICKS(60*1000), pdFALSE, 0, screen_saver_timer_callback);
    control_input_task_mutex = xSemaphoreCreateBinary();
    xSemaphoreGive(control_input_task_mutex);
    xTimerStart(screen_saver_timer, 0);
}

void IRAM_ATTR input_isr_handler(void* arg){
    static TickType_t last_interrupt_time[4] = {0};
    TickType_t current_time = xTaskGetTickCountFromISR();
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

     if (INPUT1_PIN == arg && current_time - last_interrupt_time[0] > pdMS_TO_TICKS(500)) {
        last_interrupt_time[0] = current_time;
    }
    else if (INPUT1_PIN == arg && current_time - last_interrupt_time[1] > pdMS_TO_TICKS(500)) {
        last_interrupt_time[1] = current_time;
    }
    else if (INPUT1_PIN == arg && current_time - last_interrupt_time[2] > pdMS_TO_TICKS(500)) {
        last_interrupt_time[2] = current_time;
    }
    else if (INPUT1_PIN == arg && current_time - last_interrupt_time[3] > pdMS_TO_TICKS(500)) {
        last_interrupt_time[3] = current_time;
    }

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

void IRAM_ATTR button_isr_handler(void* arg){
    static TickType_t last_interrupt_time = 0;
    TickType_t current_time = xTaskGetTickCountFromISR();
    if(current_time - last_interrupt_time > pdMS_TO_TICKS(15000)){
        last_interrupt_time = current_time;
        return;
    }

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xTimerResetFromISR(screen_saver_timer, &xHigherPriorityTaskWoken);
    xSemaphoreGiveFromISR(control_input_task_mutex, &xHigherPriorityTaskWoken);
    xHigherPriorityTaskWoken = xTaskResumeFromISR(control_input_task_handle);

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

void IRAM_ATTR endstop_isr_handler(void* arg){
    static TickType_t last_interrupt_time[4] = {0};
    TickType_t current_time = xTaskGetTickCountFromISR();
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

     if (ENDSTOP_M1_A_PIN == arg && current_time - last_interrupt_time[0] > pdMS_TO_TICKS(500)) {
        xQueueSendToFrontFromISR(gate_action_queue, GATE_CMD(HW_STOP, M1), &xHigherPriorityTaskWoken);
        last_interrupt_time[0] = current_time;
    }
    else if (ENDSTOP_M1_B_PIN == arg && current_time - last_interrupt_time[1] > pdMS_TO_TICKS(500)) {
        xQueueSendToFrontFromISR(gate_action_queue, GATE_CMD(HW_STOP, M1), &xHigherPriorityTaskWoken);
        last_interrupt_time[1] = current_time;
    }
    else if (ENDSTOP_M2_A_PIN == arg && current_time - last_interrupt_time[2] > pdMS_TO_TICKS(500)) {
        xQueueSendToFrontFromISR(gate_action_queue, GATE_CMD(HW_STOP, M2), &xHigherPriorityTaskWoken);
        last_interrupt_time[2] = current_time;
    }
    else if (ENDSTOP_M2_B_PIN == arg && current_time - last_interrupt_time[3] > pdMS_TO_TICKS(500)) {
        xQueueSendToFrontFromISR(gate_action_queue, GATE_CMD(HW_STOP, M2), &xHigherPriorityTaskWoken);
        last_interrupt_time[3] = current_time;
    }

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}


void control_input_task(void *pvParameters){
    while(1){
        if(uxSemaphoreGetCount(control_input_task_mutex) == 0){
            ESP_LOGI("CONTROL","Suspending itself");
            vTaskSuspend(NULL);
            i2c_oled_power_saver(false);
        }
        ESP_LOGI("CONTROL","SROLED");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}