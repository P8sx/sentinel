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
#include "esp_timer.h"
#include "wireless/wifi.h"

ESP_EVENT_DEFINE_BASE(UI_EVENTS);


static TimerHandle_t ui_screen_saver_timer_handle = NULL;
static atomic_uint_fast16_t ui_screen_menu = SCREEN_WELCOME;

static SemaphoreHandle_t ui_task_mutex = NULL;

extern TaskHandle_t ui_handler_button_task_handle;
extern TaskHandle_t ui_handler_oled_display_task_handle;

extern QueueHandle_t gate_action_queue;

static QueueHandle_t button_queue = NULL;


static void ui_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if(GHOTA_EVENTS == event_base){
        xTimerReset(ui_screen_saver_timer_handle, 0);
        xSemaphoreGive(ui_task_mutex);
        vTaskResume(ui_handler_oled_display_task_handle);
        switch(event_id){
        case GHOTA_EVENT_FIRMWARE_UPDATE_PROGRESS:
            atomic_store(&ui_screen_menu, SCREEN_GHOTA_FIRMWARE_UPDATE_PROGRESS);
            break;
        case GHOTA_EVENT_START_CHECK:
            atomic_store(&ui_screen_menu, SCREEN_GHOTA_START_CHECK);
            break;
        case GHOTA_EVENT_UPDATE_AVAILABLE:
            atomic_store(&ui_screen_menu, SCREEN_GHOTA_UPDATE_AVAILABLE);
            break;
        case GHOTA_EVENT_START_UPDATE:
            atomic_store(&ui_screen_menu, SCREEN_GHOTA_START_UPDATE);
            break;
        case GHOTA_EVENT_FINISH_UPDATE:
            atomic_store(&ui_screen_menu, SCREEN_GHOTA_FINISH_UPDATE);
            break;
        case GHOTA_EVENT_UPDATE_FAILED:
            atomic_store(&ui_screen_menu, SCREEN_GHOTA_UPDATE_FAILED);
            break;
        }
    }

    if(IO_EVENTS == event_base && IO_INPUT_TRIGGERED_EVENT == event_id){
        uint8_t io = *(uint8_t *)event_data;
        switch(io){
            case BTN1_PIN:
            case BTN2_PIN:
            case BTN3_PIN:
                if(!i2c_oled_initialized()) break;
                xTimerReset(ui_screen_saver_timer_handle, 0);
                xSemaphoreGive(ui_task_mutex);
                vTaskResume(ui_handler_button_task_handle);
                vTaskResume(ui_handler_oled_display_task_handle);
                break;
        }
    }

}

void screen_saver_timer_callback(TimerHandle_t xTimer) {
    xSemaphoreTake(ui_task_mutex, 0); // Take semaphore to lock task 
    vTaskDelay(pdMS_TO_TICKS(500));
    i2c_oled_power_save(true);
    atomic_store(&ui_screen_menu, SCREEN_HOME);
}


void ui_handler_init(){
    ui_task_mutex = xSemaphoreCreateBinary();
    xSemaphoreGive(ui_task_mutex);

    ui_screen_saver_timer_handle = xTimerCreate("screen_saver", pdMS_TO_TICKS(60*1000), pdFALSE, (void*)0, screen_saver_timer_callback);
    xTimerStart(ui_screen_saver_timer_handle, 0);

    ESP_ERROR_CHECK(esp_event_handler_instance_register(GHOTA_EVENTS, ESP_EVENT_ANY_ID, &ui_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IO_EVENTS, ESP_EVENT_ANY_ID, &ui_event_handler, NULL, NULL));

    button_queue = xQueueCreate(5, sizeof(button_event_t));
}



void ui_menu_main_menu(){
    static uint8_t pos = 0;
    button_event_t btn;
    while(xQueueReceive(button_queue, &btn, pdMS_TO_TICKS(60000))){
        if(btn.pin == BTN1_PIN && btn.event == BUTTON_PRESSED) pos = pos<5 ? pos+1 : pos;
        else if(btn.pin == BTN3_PIN && btn.event == BUTTON_PRESSED) pos = pos>0 ? pos-1 : pos;
        else if(btn.pin == BTN2_PIN && btn.event == BUTTON_PRESSED){
            switch(pos){
                case 0:
                    atomic_store(&ui_screen_menu, SCREEN_HOME);
                    return;
                    break;
                case 4:
                    atomic_store(&ui_screen_menu, SCREEN_HOME);
                    ghota_start_check();
                    return;
                    break;
                case 5:
                    io_buzzer(1,50,50);
                    break;
                default:
                    break;
            }
        }            
        if(btn.event == BUTTON_RELESED) i2c_oled_menu(pos, 5, "Control", "Configuration", "Status", "OTA", "Beep");
    }
}

void ui_oled_display_task(void *pvParameters){
    button_event_t btn_event;
    while (true){
        if(uxSemaphoreGetCount(ui_task_mutex) == 0){
            ESP_LOGI("CONTROL","Suspending itself");
            i2c_oled_power_save(true);
            vTaskSuspend(NULL);
            i2c_oled_power_save(false);
        }
        switch (atomic_load(&ui_screen_menu)){
        case SCREEN_WELCOME:
            i2c_oled_welcome_screen();
            vTaskDelay(pdMS_TO_TICKS(3000));
            atomic_store(&ui_screen_menu, SCREEN_HOME);
            break;
        case SCREEN_HOME:
            static bool animation_toggle = false;
            static uint8_t counter = 0;
            counter++;
            if(counter%3 == 0){
                animation_toggle = !animation_toggle;
            }
            uint8_t screen_saver_time = (xTimerGetExpiryTime(ui_screen_saver_timer_handle)-xTaskGetTickCount())/pdMS_TO_TICKS(1000);
            i2c_oled_home_screen(screen_saver_time, io_get_soc_temp(), wifi_connected(), gate_get_state(M1), gate_get_state(M2), animation_toggle);
            break;
        case SCREEN_GHOTA_START_CHECK:
            i2c_oled_ota_start_check();
            break;
        case SCREEN_GHOTA_START_UPDATE:
            i2c_oled_ota_start_update();
            break;
        case SCREEN_GHOTA_FIRMWARE_UPDATE_PROGRESS:
            i2c_oled_ota_update(ghota_get_update_progress());
            break;
        case SCREEN_GHOTA_UPDATE_AVAILABLE:
            i2c_oled_ota_update_available();
            break;
        case SCREEN_GHOTA_UPDATE_FAILED:
            i2c_oled_ota_update_failed();
            break;
        case SCREEN_GHOTA_FINISH_UPDATE:
            i2c_oled_ota_finish_update();
            break;
        case SCREEN_MENU:
            ui_menu_main_menu();
        default:
            break;
        }
        if(xQueueReceive(button_queue, &btn_event, pdMS_TO_TICKS(100)) && btn_event.pin == BTN1_PIN){
            atomic_store(&ui_screen_menu, SCREEN_MENU);
        }
    }
    
}



debounce_t d_btn[3] = {
    {.pin = BTN1_PIN, .down_time = 0, .inverted = true, .history = 0xffff},
    {.pin = BTN2_PIN, .down_time = 0, .inverted = true, .history = 0xffff},
    {.pin = BTN3_PIN, .down_time = 0, .inverted = true, .history = 0xffff}};


static bool button_rose(debounce_t *d) {
    if ((d->history & 0b1111000000111111) == 0b0000000000111111) {
        d->history = 0xffff;
        return 1;
    }
    return 0;
}
static bool button_fell(debounce_t *d) {
    if ((d->history & 0b1111000000111111) == 0b1111000000000000) {
        d->history = 0x0000;
        return 1;
    }
    return 0;
}
static bool button_down(debounce_t *d) {
    if (d->inverted) return button_fell(d);
    return button_rose(d);
}
static bool button_up(debounce_t *d) {
    if (d->inverted) return button_rose(d);
    return button_fell(d);
}

static uint32_t millis() {
    return esp_timer_get_time() / 1000;
}

static void send_event(debounce_t db, int ev) {
    button_event_t event = {
        .pin = db.pin,
        .event = ev,
    };
    xQueueSend(button_queue, &event, 0);
}

void ui_button_handling_task(void *pvParameters){
    while (true)
    {
        for (int i = 0; i < sizeof(d_btn)/sizeof(debounce_t); i++) {
            d_btn[i].history = d_btn[i].history << 1 | gpio_get_level(d_btn[i].pin);
            if (button_up(&d_btn[i])) {
                d_btn[i].down_time = 0;
                send_event(d_btn[i], BUTTON_RELESED);
            } else if (d_btn[i].down_time && millis() >= d_btn[i].next_long_time) {
                d_btn[i].next_long_time = d_btn[i].next_long_time + BUTTON_LONG_PRESS_REPEAT_MS;
                send_event(d_btn[i], BUTTON_HELD);
            } else if (button_down(&d_btn[i]) && d_btn[i].down_time == 0) {
                d_btn[i].down_time = millis();
                d_btn[i].next_long_time = d_btn[i].down_time + BUTTON_LONG_PRESS_DURATION_MS;
                send_event(d_btn[i], BUTTON_PRESSED);
            } 
        }
        if(uxSemaphoreGetCount(ui_task_mutex) == 0){
            ESP_LOGI("BUTTON","Suspending itself");
            vTaskSuspend(NULL);
            ESP_LOGI("BUTTON","Resuming itself");

        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
}
