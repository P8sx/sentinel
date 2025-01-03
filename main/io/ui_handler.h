#ifndef UI_HANDLER_H
#define UI_HANDLER_H

#include "common/config.h"
#include "common/types.h"
#include <stdio.h>
#include <stdbool.h>
#include "esp_event.h"

ESP_EVENT_DECLARE_BASE(UI_EVENTS);

/* Queue for handling inputs from ISR from Io module*/
void ui_handler_init();
void ui_oled_handling_task(void *pvParameters);
void ui_button_handling_task(void *pvParameters);
void ui_oled_display_task(void *pvParameters);

#define BUTTON_PRESSED 1
#define BUTTON_RELESED 2
#define BUTTON_HELD 3
#define BUTTON_LONG_PRESS_DURATION_MS 2000
#define BUTTON_LONG_PRESS_REPEAT_MS 50
#define IS_BTN_PRESSED(btn, pin_num) (btn.pin == pin_num && btn.event == BUTTON_PRESSED)
#define IS_BTN_RELESED(btn, pin_num) (btn.pin == pin_num && btn.event == BUTTON_RELESED)
#define IS_BTN_HELD(btn, pin_num) (btn.pin == pin_num && btn.event == BUTTON_HELD)

extern SemaphoreHandle_t rf_learning_semaphore_mutex;
extern QueueHandle_t rf_learning_queue;

typedef struct debounce_t
{
    uint8_t pin;
    bool inverted;
    uint16_t history;
    uint32_t down_time;
    uint32_t next_long_time;
} debounce_t;
typedef struct button_event_t
{
    uint8_t pin;
    uint8_t event;
} button_event_t;

typedef enum oled_state_e
{
    SCREEN_WELCOME,
    SCREEN_HOME,
    SCREEN_OTA,
    SCREEN_MENU,
    SCREEN_GHOTA_START_CHECK,
    SCREEN_GHOTA_UPDATE_AVAILABLE,
    SCREEN_GHOTA_START_UPDATE,
    SCREEN_GHOTA_FINISH_UPDATE,
    SCREEN_GHOTA_UPDATE_FAILED,
    SCREEN_GHOTA_FIRMWARE_UPDATE_PROGRESS,
} oled_state_e;

typedef enum menu_return_result_e
{
    USER_RETURN,
    TIMER_RETURN = 0xFF,
} menu_return_result_e;

typedef enum ui_event_e
{
    UI_HOME_EVENT = 0,
} ui_event_e;

#endif