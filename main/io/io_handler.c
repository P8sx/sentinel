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
#include "ui_handler.h"
#include "esp_timer.h"

extern QueueHandle_t wing_action_queue;
TimerHandle_t out1_timer_handle = NULL;
TimerHandle_t out2_timer_handle = NULL;

static void io_handler_wing_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    wing_info_t wing_status = *(wing_info_t *)event_data;

    if (RIGHT_WING == wing_status.id)
    {
        /* When right wing is moving */
        if (OPENING == wing_status.state || CLOSING == wing_status.state)
        {
            if (RIGHT_WING_ON == device_config.output_actions[0] || ANY_WING_ON == device_config.output_actions[0])
            {
                gpio_set_level(OUT1_PIN, 1);
            }
            if (RIGHT_WING_ON == device_config.output_actions[1] || ANY_WING_ON == device_config.output_actions[1])
            {
                gpio_set_level(OUT2_PIN, 1);
            }

            if (RIGHT_WING_BLINK == device_config.output_actions[0] || ANY_WING_BLINK == device_config.output_actions[0])
            {
                xTimerStart(out1_timer_handle, 0);
            }
            if (RIGHT_WING_BLINK == device_config.output_actions[1] || ANY_WING_BLINK == device_config.output_actions[1])
            {
                xTimerStart(out2_timer_handle, 0);
            }
        }
        /* When right wing is stopped */
        else
        {
            if (RIGHT_WING_ON == device_config.output_actions[0] || ANY_WING_ON == device_config.output_actions[0])
            {
                gpio_set_level(OUT1_PIN, 0);
            }
            if (RIGHT_WING_ON == device_config.output_actions[1] || ANY_WING_ON == device_config.output_actions[1])
            {
                gpio_set_level(OUT2_PIN, 0);
            }

            if (RIGHT_WING_BLINK == device_config.output_actions[0] || ANY_WING_BLINK == device_config.output_actions[0])
            {
                xTimerStop(out1_timer_handle, 0);
                gpio_set_level(OUT1_PIN, 0);
            }
            if (RIGHT_WING_BLINK == device_config.output_actions[1] || ANY_WING_BLINK == device_config.output_actions[1])
            {
                xTimerStop(out2_timer_handle, 0);
                gpio_set_level(OUT2_PIN, 0);
            }
        }
    }

    if (LEFT_WING == wing_status.id)
    {
        /* When left wing is moving */
        if (OPENING == wing_status.state || CLOSING == wing_status.state)
        {
            if (LEFT_WING_ON == device_config.output_actions[0] || ANY_WING_ON == device_config.output_actions[0])
            {
                gpio_set_level(OUT1_PIN, 1);
            }
            if (LEFT_WING_ON == device_config.output_actions[1] || ANY_WING_ON == device_config.output_actions[1])
            {
                gpio_set_level(OUT2_PIN, 1);
            }

            if (LEFT_WING_BLINK == device_config.output_actions[0] || ANY_WING_BLINK == device_config.output_actions[0])
            {
                xTimerStart(out1_timer_handle, 0);
            }
            if (LEFT_WING_BLINK == device_config.output_actions[1] || ANY_WING_BLINK == device_config.output_actions[1])
            {
                xTimerStart(out2_timer_handle, 0);
            }
        }
        /* When left wing is stopped */
        else
        {
            if (LEFT_WING_ON == device_config.output_actions[0] || ANY_WING_ON == device_config.output_actions[0])
            {
                gpio_set_level(OUT1_PIN, 0);
            }
            if (LEFT_WING_ON == device_config.output_actions[1] || ANY_WING_ON == device_config.output_actions[1])
            {
                gpio_set_level(OUT2_PIN, 0);
            }

            if (LEFT_WING_BLINK == device_config.output_actions[0] || ANY_WING_BLINK == device_config.output_actions[0])
            {
                xTimerStop(out1_timer_handle, 0);
                gpio_set_level(OUT1_PIN, 0);
            }
            if (LEFT_WING_BLINK == device_config.output_actions[1] || ANY_WING_BLINK == device_config.output_actions[1])
            {
                xTimerStop(out2_timer_handle, 0);
                gpio_set_level(OUT2_PIN, 0);
            }
        }
    }
}

static void io_handler_io_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    static const uint8_t inputs[] = {INPUT1_PIN, INPUT2_PIN, INPUT3_PIN, INPUT4_PIN};
    uint8_t io = *(uint8_t *)event_data;
    switch (io)
    {
    case INPUT1_PIN:
    case INPUT2_PIN:
    case INPUT3_PIN:
    case INPUT4_PIN:
        uint8_t input_action = 0;
        for (size_t i = 0; i < 4; i++)
        {
            if (inputs[i] == io)
                input_action = device_config.input_actions[i];
        }
        xQueueSend(wing_action_queue, &INPUT_ACTION_TO_GATE_COMMAND(input_action), pdMS_TO_TICKS(100));
        break;
    case ENDSTOP_RIGHT_WING_A_PIN:
    case ENDSTOP_RIGHT_WING_B_PIN:
        xQueueSendToFront(wing_action_queue, &WING_CMD(HW_STOP, RIGHT_WING), pdMS_TO_TICKS(100));
        break;
    case ENDSTOP_LEFT_WING_A_PIN:
    case ENDSTOP_LEFT_WING_B_PIN:
        xQueueSendToFront(wing_action_queue, &WING_CMD(HW_STOP, LEFT_WING), pdMS_TO_TICKS(100));
        break;
    }
}
static void io_handler_io_rf_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    static uint64_t last_trigger = 0;

    uint64_t rf_code = *(uint64_t *)event_data;
    /* If learning mutex is taken then send all incoming rf_code id to learning queue */
    if (uxSemaphoreGetCount(rf_learning_semaphore_mutex) == 0)
    {
        ESP_LOGI(RF_LOG_TAG, "(LEARINING MODE)RF code received: %08x", (unsigned int)rf_code);
        xQueueSend(rf_learning_queue, &rf_code, pdMS_TO_TICKS(500));
        return;
    }

    ESP_LOGI(RF_LOG_TAG, "RF code received: %08X", (unsigned int)rf_code);
    if (esp_timer_get_time() - last_trigger > 2 * 1000 * 1000)
    {
        for (size_t i = 0; i < STATIC_CFG_NUM_OF_REMOTES; i++)
        {
            if (rf_code == remotes_config[i].id)
            {
                ESP_LOGI(RF_LOG_TAG, "RF code found");
                xQueueSend(wing_action_queue, &INPUT_ACTION_TO_GATE_COMMAND(remotes_config[i].action), pdMS_TO_TICKS(100));
                last_trigger = esp_timer_get_time();
                return;
            }
        }
    }
}

void output_timer_callback(TimerHandle_t xTimer)
{
    static bool out1_state = 0;
    static bool out2_state = 0;
    if (xTimer == out1_timer_handle)
    {
        out1_state = !out1_state;
        gpio_set_level(OUT1_PIN, out1_state);
    }
    else if (xTimer == out2_timer_handle)
    {
        out2_state = !out2_state;
        gpio_set_level(OUT2_PIN, out2_state);
    }
}

void io_handler_init()
{
    ESP_ERROR_CHECK(esp_event_handler_instance_register(GATE_EVENTS, ESP_EVENT_ANY_ID, &io_handler_wing_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IO_EVENTS, IO_INPUT_TRIGGERED_EVENT, &io_handler_io_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IO_EVENTS, IO_RF_EVENT, &io_handler_io_rf_event_handler, NULL, NULL));

    out1_timer_handle = xTimerCreate("out1_timer", pdMS_TO_TICKS(1 * 1000), pdTRUE, (void *)10, output_timer_callback);
    out2_timer_handle = xTimerCreate("out2_timer", pdMS_TO_TICKS(1 * 1000), pdTRUE, (void *)11, output_timer_callback);
}
