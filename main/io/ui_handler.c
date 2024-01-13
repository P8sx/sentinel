#include "ui_handler.h"
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

static TimerHandle_t ui_screen_saver_timer = NULL;
static SemaphoreHandle_t ui_oled_task_mutex = NULL;
extern TaskHandle_t ui_handler_oled_task_handle;
extern TaskHandle_t ui_handler_button_task_handle;
extern QueueHandle_t gate_action_queue;

static void ui_ghota_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (GHOTA_EVENT_FIRMWARE_UPDATE_PROGRESS == event_id) {
        xTimerReset(ui_screen_saver_timer, 0);
        i2c_oled_power_save(false);
        i2c_oled_ota_update(*((int*) event_data));
    } else if(GHOTA_EVENT_START_CHECK == event_id){
        i2c_oled_ota_start_check();
    } else if(GHOTA_EVENT_UPDATE_AVAILABLE == event_id){
        i2c_oled_ota_update_available();
    } else if(GHOTA_EVENT_START_UPDATE == event_id){
        i2c_oled_ota_start_update();
    } else if(GHOTA_EVENT_FINISH_UPDATE == event_id){
        i2c_oled_ota_finish_update();
    } else if(GHOTA_EVENT_UPDATE_FAILED == event_id){
        i2c_oled_ota_update_failed();
    }
}

static void ui_io_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if(IO_INPUT_TRIGGERED_EVENT == event_id){
        uint8_t io = *(uint8_t *)event_data;
        switch(io){
            case BTN1_PIN:
            case BTN2_PIN:
            case BTN3_PIN:
                if(!i2c_oled_initialized()) break;
                xTimerReset(ui_screen_saver_timer, 0);
                xSemaphoreGive(ui_oled_task_mutex);
                vTaskResume(ui_handler_oled_task_handle);
                break;
        }
    }
}

static void ui_gate_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if(MOTOR_STATUS_CHANGED == event_id){
        gate_status_t *status = (gate_status_t *)event_data;
        
    }
}


void screen_saver_timer_callback(TimerHandle_t xTimer) {
    i2c_oled_power_save(true);
    /* Take semaphore to lock task */
    xSemaphoreTake(ui_oled_task_mutex, 0);
}

void ui_handler_init(){
    ui_oled_task_mutex = xSemaphoreCreateBinary();
    xSemaphoreGive(ui_oled_task_mutex);

    if(i2c_oled_initialized()){
        ui_screen_saver_timer = xTimerCreate("screen_saver", pdMS_TO_TICKS(60*1000), pdFALSE, 0, screen_saver_timer_callback);
        xTimerStart(ui_screen_saver_timer, 0);
    }

    ESP_ERROR_CHECK(esp_event_handler_instance_register(GHOTA_EVENTS, ESP_EVENT_ANY_ID, &ui_ghota_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IO_EVENTS, ESP_EVENT_ANY_ID, &ui_io_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(GATE_EVENTS, ESP_EVENT_ANY_ID, &ui_gate_event_handler, NULL, NULL));
}


void ui_button_handling_task(void *pvParameters){
    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
}

void ui_oled_handling_task(void *pvParameters){
    i2c_oled_power_save(false);
	i2c_oled_welcome_screen();
    vTaskDelay(pdMS_TO_TICKS(3000));
    while(1){
        if(uxSemaphoreGetCount(ui_oled_task_mutex) == 0){
            ESP_LOGI("CONTROL","Suspending itself");
            vTaskSuspend(NULL);
            i2c_oled_power_save(false);
        }
        ESP_LOGI("CONTROL","SR-OLED");
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}


