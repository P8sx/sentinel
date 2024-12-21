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
#include "driver/uart.h"

ESP_EVENT_DEFINE_BASE(UI_EVENTS);

static atomic_uint_fast16_t ui_screen_menu = SCREEN_WELCOME;
static TimerHandle_t ui_screen_saver_timer_handle = NULL;
static SemaphoreHandle_t ui_task_mutex = NULL;
static QueueHandle_t button_queue = NULL;

extern TaskHandle_t ui_handler_button_task_handle;
extern TaskHandle_t ui_handler_oled_display_task_handle;
extern QueueHandle_t wing_action_queue;

SemaphoreHandle_t rf_learning_semaphore_mutex = NULL;
QueueHandle_t rf_learning_queue = NULL;

#define RECEIVE_FROM_BTN_QUEUE(button) xQueueReceive(button_queue, &button, pdMS_TO_TICKS(60000))

static void ui_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (GHOTA_EVENTS == event_base)
    {
        xTimerReset(ui_screen_saver_timer_handle, 0);
        xSemaphoreGive(ui_task_mutex);
        vTaskResume(ui_handler_oled_display_task_handle);
        switch (event_id)
        {
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

    if (IO_EVENTS == event_base && IO_INPUT_TRIGGERED_EVENT == event_id)
    {
        uint8_t io = *(uint8_t *)event_data;
        switch (io)
        {
        case BTN1_PIN:
        case BTN2_PIN:
        case BTN3_PIN:
            if (!i2c_oled_initialized())
                break;
            xTimerReset(ui_screen_saver_timer_handle, 0);
            xSemaphoreGive(ui_task_mutex);
            vTaskResume(ui_handler_button_task_handle);
            vTaskResume(ui_handler_oled_display_task_handle);
            break;
        }
    }
}

void screen_saver_timer_callback(TimerHandle_t xTimer)
{
    xSemaphoreTake(ui_task_mutex, 0); // Take semaphore to lock task
    vTaskDelay(pdMS_TO_TICKS(500));
    i2c_oled_power_save(true);
    atomic_store(&ui_screen_menu, SCREEN_HOME);
}

void ui_handler_init()
{
    ui_task_mutex = xSemaphoreCreateBinary();
    xSemaphoreGive(ui_task_mutex);

    ui_screen_saver_timer_handle = xTimerCreate("screen_saver", pdMS_TO_TICKS(60 * 1000), pdFALSE, (void *)0, screen_saver_timer_callback);
    xTimerStart(ui_screen_saver_timer_handle, 0);

    ESP_ERROR_CHECK(esp_event_handler_instance_register(GHOTA_EVENTS, ESP_EVENT_ANY_ID, &ui_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IO_EVENTS, ESP_EVENT_ANY_ID, &ui_event_handler, NULL, NULL));

    button_queue = xQueueCreate(5, sizeof(button_event_t));

    rf_learning_semaphore_mutex = xSemaphoreCreateBinary();
    xSemaphoreGive(rf_learning_semaphore_mutex);
    rf_learning_queue = xQueueCreate(3, sizeof(uint64_t));
}

menu_return_result_e ui_menu_config_wing_submenu(wing_id_t wing_id)
{
    const uint8_t menu_options = 3;
    uint8_t pos = 0;
    button_event_t btn;
    char option_labels[3][30];
    const char *menu_label = (wing_id == RIGHT_WING) ? "Config-> Right wing" : "Config-> Left wing";

    float ocp_threshold;
    uint16_t ocp_count;
    bool direction;

    if (wing_id == RIGHT_WING)
    {
        ocp_threshold = (float)device_config.right_wing_ocp_threshold / 1000;
        ocp_count = (float)device_config.right_wing_ocp_count;
        direction = (float)device_config.right_wing_dir;
    }
    else
    {
        ocp_threshold = (float)device_config.left_wing_ocp_threshold / 1000;
        ocp_count = (float)device_config.left_wing_ocp_count;
        direction = (float)device_config.left_wing_dir;
    }

    snprintf(option_labels[0], sizeof(option_labels[0]), "%.2fA", ocp_threshold);
    snprintf(option_labels[1], sizeof(option_labels[0]), "%i", ocp_count);
    snprintf(option_labels[2], sizeof(option_labels[0]), direction ? "false" : "true");

    while (RECEIVE_FROM_BTN_QUEUE(btn))
    {
        if (IS_BTN_PRESSED(btn, BTN1_PIN))
            pos = pos < menu_options ? pos + 1 : pos;
        else if (IS_BTN_PRESSED(btn, BTN3_PIN))
            pos = pos > 0 ? pos - 1 : pos;
        else if (IS_BTN_PRESSED(btn, BTN2_PIN))
        {
            switch (pos)
            {
            case 0:
                return USER_RETURN;
                break;
            case 1:
                while (RECEIVE_FROM_BTN_QUEUE(btn))
                {
                    if (IS_BTN_PRESSED(btn, BTN1_PIN))
                        ocp_threshold -= 0.01;
                    else if (IS_BTN_HELD(btn, BTN1_PIN))
                        ocp_threshold -= 0.05;
                    else if (IS_BTN_PRESSED(btn, BTN3_PIN))
                        ocp_threshold += 0.01;
                    else if (IS_BTN_HELD(btn, BTN3_PIN))
                        ocp_threshold += 0.05;
                    else if (IS_BTN_PRESSED(btn, BTN2_PIN))
                        break;

                    snprintf(option_labels[0], sizeof(option_labels[0]), "%.2fA", ocp_threshold);

                    if (btn.event == BUTTON_RELESED || btn.event == BUTTON_HELD)
                        i2c_oled_menu_with_params(menu_label, pos, true, menu_options * 2, "OCP Thr", option_labels[0], "OCP Count", option_labels[1], "Direction", option_labels[2]);
                }
                break;
            case 2:
                while (RECEIVE_FROM_BTN_QUEUE(btn))
                {
                    if (IS_BTN_PRESSED(btn, BTN1_PIN))
                        ocp_count--;
                    else if (IS_BTN_HELD(btn, BTN1_PIN))
                        ocp_count -= 5;
                    else if (IS_BTN_PRESSED(btn, BTN3_PIN))
                        ocp_count++;
                    else if (IS_BTN_HELD(btn, BTN3_PIN))
                        ocp_count += 5;
                    else if (IS_BTN_PRESSED(btn, BTN2_PIN))
                        break;

                    snprintf(option_labels[1], sizeof(option_labels[0]), "%i", ocp_count);

                    if (btn.event == BUTTON_RELESED || btn.event == BUTTON_HELD)
                        i2c_oled_menu_with_params(menu_label, pos, true, menu_options * 2, "OCP Thr", option_labels[0], "OCP Count", option_labels[1], "Direction", option_labels[2]);
                }
                break;
            case 3:
                while (RECEIVE_FROM_BTN_QUEUE(btn))
                {
                    if (IS_BTN_PRESSED(btn, BTN1_PIN) || IS_BTN_PRESSED(btn, BTN3_PIN))
                        direction = !direction;
                    else if (IS_BTN_PRESSED(btn, BTN2_PIN))
                        break;

                    snprintf(option_labels[2], sizeof(option_labels[0]), direction ? "false" : "true");

                    if (btn.event == BUTTON_RELESED)
                        i2c_oled_menu_with_params(menu_label, pos, true, menu_options * 2, "OCP Thr", option_labels[0], "OCP Count", option_labels[1], "Direction", option_labels[2]);
                }
                break;
            default:
                break;
            }
            config_update_wing_settings(wing_id, direction, ocp_threshold * 1000, ocp_count);
            io_buzzer(3, 20, 20);
        }
        if (btn.event == BUTTON_RELESED)
            i2c_oled_menu_with_params(menu_label, pos, false, menu_options * 2, "OCP Thr", option_labels[0], "OCP Count", option_labels[1], "Direction", option_labels[2]);
    }
    return TIMER_RETURN;
}

menu_return_result_e ui_menu_config_delay_submenu()
{
    const uint8_t menu_options = 3;
    uint8_t pos = 0;
    button_event_t btn;
    char option_labels[3][30];
    const char *menu_label = "Config-> Delay";

    bool wing_delay = device_config.wing_delay;
    bool wing_delay_dir = device_config.wing_delay_dir;
    uint32_t wing_delay_time = device_config.wing_delay_time;

    snprintf(option_labels[0], sizeof(option_labels[0]), wing_delay ? "true" : "false");
    snprintf(option_labels[1], sizeof(option_labels[0]), wing_delay_dir ? "right" : "left");
    snprintf(option_labels[2], sizeof(option_labels[0]), "%lu ms", wing_delay_time);

    while (RECEIVE_FROM_BTN_QUEUE(btn))
    {
        if (IS_BTN_PRESSED(btn, BTN1_PIN))
            pos = pos < menu_options ? pos + 1 : pos;
        else if (IS_BTN_PRESSED(btn, BTN3_PIN))
            pos = pos > 0 ? pos - 1 : pos;
        else if (IS_BTN_PRESSED(btn, BTN2_PIN))
        {
            switch (pos)
            {
            case 0:
                return USER_RETURN;
                break;
            case 1:
                while (RECEIVE_FROM_BTN_QUEUE(btn))
                {
                    if (IS_BTN_PRESSED(btn, BTN1_PIN) || IS_BTN_PRESSED(btn, BTN3_PIN))
                        wing_delay = !wing_delay;
                    else if (IS_BTN_PRESSED(btn, BTN2_PIN))
                        break;

                    snprintf(option_labels[0], sizeof(option_labels[0]), wing_delay ? "false" : "true");

                    if (btn.event == BUTTON_RELESED)
                        i2c_oled_menu_with_params(menu_label, pos, true, menu_options * 2, "Enabled", option_labels[0], "Direction", option_labels[1], "Time", option_labels[2]);
                }
                break;
            case 2:
                while (RECEIVE_FROM_BTN_QUEUE(btn))
                {
                    if (IS_BTN_PRESSED(btn, BTN1_PIN) || IS_BTN_PRESSED(btn, BTN3_PIN))
                        wing_delay_dir = !wing_delay_dir;
                    else if (IS_BTN_PRESSED(btn, BTN2_PIN))
                        break;

                    snprintf(option_labels[1], sizeof(option_labels[0]), wing_delay_dir ? "right" : "left");

                    if (btn.event == BUTTON_RELESED)
                        i2c_oled_menu_with_params(menu_label, pos, true, menu_options * 2, "Enabled", option_labels[0], "Direction", option_labels[1], "Time", option_labels[2]);
                }
                break;
            case 3:
                while (RECEIVE_FROM_BTN_QUEUE(btn))
                {
                    if (IS_BTN_PRESSED(btn, BTN1_PIN))
                        wing_delay_time -= 50;
                    else if (IS_BTN_HELD(btn, BTN1_PIN))
                        wing_delay_time -= 100;
                    else if (IS_BTN_PRESSED(btn, BTN3_PIN))
                        wing_delay_time += 50;
                    else if (IS_BTN_HELD(btn, BTN3_PIN))
                        wing_delay_time += 100;
                    else if (IS_BTN_PRESSED(btn, BTN2_PIN))
                        break;

                    snprintf(option_labels[2], sizeof(option_labels[0]), "%lu ms", wing_delay_time);

                    if (btn.event == BUTTON_RELESED || btn.event == BUTTON_HELD)
                        i2c_oled_menu_with_params(menu_label, pos, true, menu_options * 2, "Enabled", option_labels[0], "Direction", option_labels[1], "Time", option_labels[2]);
                }
                break;
            default:
                break;
            }
            config_update_delay_settings(wing_delay, wing_delay_dir, wing_delay_time);
            io_buzzer(3, 20, 20);
        }
        if (btn.event == BUTTON_RELESED)
            i2c_oled_menu_with_params(menu_label, pos, false, menu_options * 2, "Enabled", option_labels[0], "Direction", option_labels[1], "Time", option_labels[2]);
    }
    return TIMER_RETURN;
}

menu_return_result_e ui_menu_config_input_submenu()
{
    const uint8_t menu_options = 4;
    uint8_t pos = 0;
    button_event_t btn;

    uint8_t current_actions[4] = {device_config.input_actions[0], device_config.input_actions[1], device_config.input_actions[2], device_config.input_actions[3]};
    char option_labels[4][30];

    snprintf(option_labels[0], sizeof(option_labels[0]), "%s", INPUT_ACTION_TO_STRING(current_actions[0]));
    snprintf(option_labels[1], sizeof(option_labels[0]), "%s", INPUT_ACTION_TO_STRING(current_actions[1]));
    snprintf(option_labels[2], sizeof(option_labels[0]), "%s", INPUT_ACTION_TO_STRING(current_actions[2]));
    snprintf(option_labels[3], sizeof(option_labels[0]), "%s", INPUT_ACTION_TO_STRING(current_actions[3]));

    while (RECEIVE_FROM_BTN_QUEUE(btn))
    {
        if (IS_BTN_PRESSED(btn, BTN1_PIN))
            pos = pos < menu_options ? pos + 1 : pos;
        else if (IS_BTN_PRESSED(btn, BTN3_PIN))
            pos = pos > 0 ? pos - 1 : pos;
        else if (IS_BTN_PRESSED(btn, BTN2_PIN))
        {
            if (0 == pos)
                return USER_RETURN;
            uint8_t in_sel = pos - 1;

            while (RECEIVE_FROM_BTN_QUEUE(btn))
            {
                if (IS_BTN_PRESSED(btn, BTN1_PIN))
                    current_actions[in_sel] = NEXT_INPUT_ACTION(current_actions[in_sel]);
                else if (IS_BTN_PRESSED(btn, BTN3_PIN))
                    current_actions[in_sel] = NEXT_INPUT_ACTION(current_actions[in_sel]);
                else if (IS_BTN_PRESSED(btn, BTN2_PIN))
                    break;

                snprintf(option_labels[in_sel], sizeof(option_labels[0]), "%s", INPUT_ACTION_TO_STRING(current_actions[in_sel]));
                if (btn.event == BUTTON_RELESED || btn.event == BUTTON_HELD)
                    i2c_oled_menu_with_params("Config->Input", pos, true, menu_options * 2, "IN1", option_labels[0], "IN2", option_labels[1], "IN3", option_labels[2], "IN4", option_labels[3]);
            }

            config_update_input_settings(current_actions[0], current_actions[1], current_actions[2], current_actions[3]);
            io_buzzer(3, 20, 20);
        }
        if (btn.event == BUTTON_RELESED)
            i2c_oled_menu_with_params("Config->Input", pos, false, menu_options * 2, "IN1", option_labels[0], "IN2", option_labels[1], "IN3", option_labels[2], "IN4", option_labels[3]);
    }
    return TIMER_RETURN;
}

menu_return_result_e ui_menu_config_output_submenu()
{
    const uint8_t menu_options = 2;
    uint8_t pos = 0;
    button_event_t btn;

    uint8_t current_actions[2] = {device_config.output_actions[0], device_config.output_actions[1]};
    char option_labels[2][40];

    snprintf(option_labels[0], sizeof(option_labels[0]), "%s", OUTPUT_ACTION_TO_STRING(current_actions[0]));
    snprintf(option_labels[1], sizeof(option_labels[0]), "%s", OUTPUT_ACTION_TO_STRING(current_actions[1]));

    while (RECEIVE_FROM_BTN_QUEUE(btn))
    {
        if (IS_BTN_PRESSED(btn, BTN1_PIN))
            pos = pos < menu_options ? pos + 1 : pos;
        else if (IS_BTN_PRESSED(btn, BTN3_PIN))
            pos = pos > 0 ? pos - 1 : pos;
        else if (IS_BTN_PRESSED(btn, BTN2_PIN))
        {
            if (0 == pos)
                return USER_RETURN;
            uint8_t in_sel = pos - 1;
            while (RECEIVE_FROM_BTN_QUEUE(btn))
            {
                if (IS_BTN_PRESSED(btn, BTN1_PIN))
                    current_actions[in_sel] = NEXT_OUTPUT_ACTION(current_actions[in_sel]);
                else if (IS_BTN_PRESSED(btn, BTN3_PIN))
                    current_actions[in_sel] = NEXT_OUTPUT_ACTION(current_actions[in_sel]);
                else if (IS_BTN_PRESSED(btn, BTN2_PIN))
                    break;

                snprintf(option_labels[in_sel], sizeof(option_labels[0]), "%s", OUTPUT_ACTION_TO_STRING(current_actions[in_sel]));
                if (btn.event == BUTTON_RELESED || btn.event == BUTTON_HELD)
                    i2c_oled_menu_with_params("Config->Output", pos, true, menu_options * 2, "OUT1", option_labels[0], "OUT2", option_labels[1]);
            }

            config_update_output_settings(current_actions[0], current_actions[1]);
            io_buzzer(3, 20, 20);
        }
        if (btn.event == BUTTON_RELESED)
            i2c_oled_menu_with_params("Config->Output", pos, false, menu_options * 2, "OUT1", option_labels[0], "OUT2", option_labels[1]);
    }
    return TIMER_RETURN;
}

menu_return_result_e ui_menu_config_rf_submenu()
{
    const uint8_t menu_options = 2;
    uint8_t pos = 0;
    button_event_t btn;
    while (RECEIVE_FROM_BTN_QUEUE(btn))
    {
        if (IS_BTN_PRESSED(btn, BTN1_PIN))
            pos = pos < menu_options ? pos + 1 : pos;
        else if (IS_BTN_PRESSED(btn, BTN3_PIN))
            pos = pos > 0 ? pos - 1 : pos;
        else if (IS_BTN_PRESSED(btn, BTN2_PIN))
        {
            menu_return_result_e return_result = 0;
            switch (pos)
            {
            case 0:
                return USER_RETURN;
                break;
            case 1:
            {
                xSemaphoreTake(rf_learning_semaphore_mutex, pdMS_TO_TICKS(1000));
                uint64_t rf_code = 0, initial_rf_code = 0;
                uint8_t correct_rf_codes = 0;
                uint8_t failed_rf_codes = 0;
                uint8_t rf_action = INPUT_UNKNOWN_ACTION;
                char rf_label[64] = "";
                i2c_oled_generic_info_screen(2, "RF Learning", "Press remote key");

                xQueueReset(rf_learning_queue);
                while (xQueueReceive(rf_learning_queue, &rf_code, pdMS_TO_TICKS(10 * 1000)))
                {
                    if (10 == correct_rf_codes)
                    {
                        if (config_check_remote(rf_code))
                        {
                            i2c_oled_generic_info_screen(4, "RF Learning", "This remote is already", "configured, aborting", rf_label);
                            vTaskDelay(pdMS_TO_TICKS(2000));
                            break;
                        }
                        i2c_oled_generic_info_screen(4, "RF Learning", "Learning", "success", rf_label);
                        vTaskDelay(pdMS_TO_TICKS(2000));

                        while (RECEIVE_FROM_BTN_QUEUE(btn))
                        {
                            if (IS_BTN_PRESSED(btn, BTN1_PIN))
                                rf_action = NEXT_INPUT_ACTION(rf_action);
                            else if (IS_BTN_PRESSED(btn, BTN3_PIN))
                                rf_action = NEXT_INPUT_ACTION(rf_action);
                            else if (IS_BTN_PRESSED(btn, BTN2_PIN))
                                break;

                            char label[64];
                            snprintf(label, sizeof(label), "%s", INPUT_ACTION_TO_STRING(rf_action));
                            if (btn.event == BUTTON_RELESED || btn.event == BUTTON_HELD)
                                i2c_oled_generic_info_screen(4, "RF Learning", "Please select", label, rf_label);
                        }
                        if (INPUT_UNKNOWN_ACTION == rf_action)
                        {
                            i2c_oled_generic_info_screen(3, "RF Learning", "No action selected", "remote not added");
                            vTaskDelay(pdMS_TO_TICKS(2000));
                        }
                        else
                        {
                            bool add_status = config_add_remote(initial_rf_code, rf_action);
                            if (add_status)
                            {
                                i2c_oled_generic_info_screen(4, "RF Learning", "Remote added", "successfuly", rf_label);
                                io_buzzer(3, 20, 20);
                            }
                            else
                            {
                                i2c_oled_generic_info_screen(4, "RF Learning", "Remote add", "failed(memory full)", rf_label);
                            }
                            vTaskDelay(pdMS_TO_TICKS(2000));
                        }
                        break;
                    }
                    if (0 == initial_rf_code)
                    {
                        initial_rf_code = rf_code;
                        snprintf(rf_label, sizeof(rf_label), "ID: %08X", (unsigned int)initial_rf_code);
                        i2c_oled_generic_info_screen(4, "RF Learning", "Learning", "started", rf_label);
                    }
                    if (rf_code == initial_rf_code)
                    {
                        correct_rf_codes++;
                        char label[64];
                        ESP_LOGI("SADASDASd", "ID: %08X", (unsigned int)initial_rf_code);
                        snprintf(label, sizeof(label), "progress: %i%%", 10 * correct_rf_codes);
                        i2c_oled_generic_info_screen(4, "RF Learning", "Learning", label, rf_label);
                    }
                    else
                    {
                        failed_rf_codes++;
                    }
                    if (failed_rf_codes > 5)
                    {
                        i2c_oled_generic_info_screen(3, "RF Learning", "Learning failed", "try again");
                        vTaskDelay(pdMS_TO_TICKS(2000));
                        break;
                    }
                }
                xSemaphoreGive(rf_learning_semaphore_mutex);
            }
            break;
            case 2:
                while (RECEIVE_FROM_BTN_QUEUE(btn))
                {
                    uint64_t rf_code = config_get_next_remote(0);

                    if (0 == rf_code)
                    {
                        i2c_oled_generic_info_screen(2, "RF Learning", "No RF remote detected");
                        vTaskDelay(pdMS_TO_TICKS(2000));
                        break;
                    }

                    if (IS_BTN_PRESSED(btn, BTN1_PIN) || IS_BTN_PRESSED(btn, BTN3_PIN))
                    {
                        rf_code = config_get_next_remote(rf_code);
                        rf_code == 0 ? config_get_next_remote(0) : config_get_next_remote(0);
                    }
                    else if (IS_BTN_PRESSED(btn, BTN2_PIN))
                    {
                        config_remove_remote(rf_code);
                        io_buzzer(3, 20, 20);

                        char label[64];
                        snprintf(label, sizeof(label), "ID: %08X", (unsigned int)rf_code);
                        i2c_oled_generic_info_screen(3, "RF Learning", "RF removed", label);

                        vTaskDelay(pdMS_TO_TICKS(2000));
                        break;
                    }

                    char label[64];
                    snprintf(label, sizeof(label), "-> ID: %08X", (unsigned int)rf_code);
                    if (btn.event == BUTTON_RELESED || btn.event == BUTTON_HELD)
                        i2c_oled_generic_info_screen(4, "RF Learning", "Please select RF", "code to remove", label);
                }
                break;
            default:
                break;
            }

            if (return_result == 0xFF)
                return TIMER_RETURN;
        }
        if (btn.event == BUTTON_RELESED)
            i2c_oled_menu("Config->RF", pos, menu_options, "Add", "Remove");
    }
    return TIMER_RETURN;
}

menu_return_result_e ui_menu_config_modbus_submenu()
{
    const uint8_t menu_options = 3;
    uint8_t pos = 0;
    button_event_t btn;

    char option_labels[3][30];
    const char menu_label[] = "Config->MODBUS";

    const uint32_t baud_list[11] = {4800, 7200, 9600, 14400, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
    const uint8_t baud_count = sizeof(baud_list) / sizeof(uint32_t);

    uint8_t selected_baud = 0;
    for (selected_baud = 0; selected_baud < baud_count && baud_list[selected_baud] != device_config.modbus_baudrate; ++selected_baud)
        ;

    uint8_t slave_id = device_config.modbus_slave_id;
    uint8_t parity = device_config.modbus_parity;
    uint32_t baudrate = device_config.modbus_baudrate;

    snprintf(option_labels[0], sizeof(option_labels[0]), "%i", slave_id);
    snprintf(option_labels[1], sizeof(option_labels[0]), "%s", parity == UART_PARITY_DISABLE ? "NONE" : parity == UART_PARITY_EVEN ? "EVEN"
                                                                                                                                   : "ODD");
    snprintf(option_labels[2], sizeof(option_labels[0]), "%i", (int)baud_list[selected_baud]);

    while (RECEIVE_FROM_BTN_QUEUE(btn))
    {
        if (IS_BTN_PRESSED(btn, BTN1_PIN))
            pos = pos < menu_options ? pos + 1 : pos;
        else if (IS_BTN_PRESSED(btn, BTN3_PIN))
            pos = pos > 0 ? pos - 1 : pos;
        else if (IS_BTN_PRESSED(btn, BTN2_PIN))
        {
            switch (pos)
            {
            case 0:
                if (baudrate != device_config.modbus_baudrate || parity != device_config.modbus_parity || slave_id != device_config.modbus_slave_id)
                {
                    config_update_modbus_settings(slave_id, parity, baudrate);
                    io_buzzer(3, 20, 20);
                    char timer_str[10];
                    for (int i = 3; i > 0; i--)
                    {
                        snprintf(timer_str, sizeof(timer_str), "%is", i);
                        i2c_oled_generic_info_screen(3, "MODBUS Update", "Rebooting in:", timer_str);
                        vTaskDelay(pdMS_TO_TICKS(1000));
                    }
                    esp_restart();
                }
                return USER_RETURN;
                break;
            case 1:
                while (RECEIVE_FROM_BTN_QUEUE(btn))
                {
                    if (IS_BTN_PRESSED(btn, BTN1_PIN))
                        slave_id--;
                    else if (IS_BTN_HELD(btn, BTN1_PIN))
                        slave_id -= 10;
                    else if (IS_BTN_PRESSED(btn, BTN3_PIN))
                        slave_id++;
                    else if (IS_BTN_HELD(btn, BTN3_PIN))
                        slave_id += 10;
                    else if (IS_BTN_PRESSED(btn, BTN2_PIN))
                        break;

                    snprintf(option_labels[0], sizeof(option_labels[0]), "%i", slave_id);
                    if (btn.event == BUTTON_RELESED || btn.event == BUTTON_HELD)
                        i2c_oled_menu_with_params(menu_label, pos, true, menu_options * 2, "Slave ID", option_labels[0], "Parity", option_labels[1], "Baudrate", option_labels[2]);
                }
                break;
            case 2:
                while (RECEIVE_FROM_BTN_QUEUE(btn))
                {
                    if (IS_BTN_PRESSED(btn, BTN1_PIN))
                    {
                        if (parity == UART_PARITY_DISABLE)
                            parity = UART_PARITY_EVEN;
                        else if (parity == UART_PARITY_EVEN)
                            parity = UART_PARITY_ODD;
                        else if (parity == UART_PARITY_ODD)
                            parity = UART_PARITY_DISABLE;
                    }
                    else if (IS_BTN_PRESSED(btn, BTN3_PIN))
                    {
                        if (parity == UART_PARITY_DISABLE)
                            parity = UART_PARITY_ODD;
                        else if (parity == UART_PARITY_ODD)
                            parity = UART_PARITY_EVEN;
                        else if (parity == UART_PARITY_EVEN)
                            parity = UART_PARITY_DISABLE;
                    }
                    else if (IS_BTN_PRESSED(btn, BTN2_PIN))
                        break;

                    snprintf(option_labels[1], sizeof(option_labels[0]), "%s", parity == UART_PARITY_DISABLE ? "NONE" : parity == UART_PARITY_EVEN ? "EVEN"
                                                                                                                                                   : "ODD");
                    if (btn.event == BUTTON_RELESED || btn.event == BUTTON_HELD)
                        i2c_oled_menu_with_params(menu_label, pos, true, menu_options * 2, "Slave ID", option_labels[0], "Parity", option_labels[1], "Baudrate", option_labels[2]);
                }
                break;
            case 3:
                while (RECEIVE_FROM_BTN_QUEUE(btn))
                {
                    if (IS_BTN_PRESSED(btn, BTN1_PIN))
                        selected_baud = selected_baud > 0 ? selected_baud - 1 : baud_count - 1;
                    else if (IS_BTN_PRESSED(btn, BTN3_PIN))
                        selected_baud = selected_baud < baud_count - 1 ? selected_baud + 1 : 0;
                    else if (IS_BTN_PRESSED(btn, BTN2_PIN))
                    {
                        baudrate = baud_list[selected_baud];
                        break;
                    }

                    snprintf(option_labels[2], sizeof(option_labels[0]), "%i", (int)baud_list[selected_baud]);
                    if (btn.event == BUTTON_RELESED || btn.event == BUTTON_HELD)
                        i2c_oled_menu_with_params(menu_label, pos, true, menu_options * 2, "Slave ID", option_labels[0], "Parity", option_labels[1], "Baudrate", option_labels[2]);
                }
                break;
            default:
                break;
            }
        }
        if (btn.event == BUTTON_RELESED)
            i2c_oled_menu_with_params(menu_label, pos, false, menu_options * 2, "Slave ID", option_labels[0], "Parity", option_labels[1], "Baudrate", option_labels[2]);
    }
    return TIMER_RETURN;
}

menu_return_result_e ui_menu_config_menu()
{

    const uint8_t menu_options = 7;
    uint8_t pos = 0;
    button_event_t btn;
    while (RECEIVE_FROM_BTN_QUEUE(btn))
    {
        if (IS_BTN_PRESSED(btn, BTN1_PIN))
            pos = pos < menu_options ? pos + 1 : pos;
        else if (IS_BTN_PRESSED(btn, BTN3_PIN))
            pos = pos > 0 ? pos - 1 : pos;
        else if (IS_BTN_PRESSED(btn, BTN2_PIN))
        {
            menu_return_result_e return_result = 0;
            switch (pos)
            {
            case 0:
                return USER_RETURN;
                break;
            case 1:
                return_result = ui_menu_config_wing_submenu(RIGHT_WING);
                break;
            case 2:
                return_result = ui_menu_config_wing_submenu(LEFT_WING);
                break;
            case 3:
                return_result = ui_menu_config_delay_submenu();
                break;
            case 4:
                return_result = ui_menu_config_input_submenu();
                break;
            case 5:
                return_result = ui_menu_config_output_submenu();
                break;
            case 6:
                return_result = ui_menu_config_rf_submenu();
                break;
            case 7:
                return_result = ui_menu_config_modbus_submenu();
                break;
            default:
                break;
            }
            if (return_result == 0xFF)
                return TIMER_RETURN;
        }
        if (btn.event == BUTTON_RELESED)
        {
            i2c_oled_menu("Config", pos, menu_options, "Right wing", "Left wing", "Delay", "Input", "Output", "RF", "MODBUS");
        }
    }
    return TIMER_RETURN;
}

menu_return_result_e ui_menu_control_submenu(wing_id_t wing_id)
{
    const uint8_t menu_options = 4;
    uint8_t pos = 1;
    button_event_t btn;
    while (RECEIVE_FROM_BTN_QUEUE(btn))
    {
        if (IS_BTN_PRESSED(btn, BTN1_PIN))
            pos = pos < menu_options ? pos + 1 : pos;
        else if (IS_BTN_PRESSED(btn, BTN3_PIN))
            pos = pos > 0 ? pos - 1 : pos;
        else if (IS_BTN_PRESSED(btn, BTN2_PIN))
        {
            switch (pos)
            {
            case 0:
                return USER_RETURN;
                break;
            case 1:
                xQueueSend(wing_action_queue, &WING_CMD(NEXT_STATE, wing_id), pdMS_TO_TICKS(1000));
                break;
            case 2:
                xQueueSend(wing_action_queue, &WING_CMD(OPEN, wing_id), pdMS_TO_TICKS(1000));
                break;
            case 3:
                xQueueSend(wing_action_queue, &WING_CMD(CLOSE, wing_id), pdMS_TO_TICKS(1000));
                break;
            case 4:
                xQueueSend(wing_action_queue, &WING_CMD(STOP, wing_id), pdMS_TO_TICKS(1000));
                break;
            default:
                break;
            }
        }
        if (btn.event == BUTTON_RELESED)
        {
            const char *wing_label = (wing_id == RIGHT_WING) ? "Control->Right wing" : (wing_id == LEFT_WING) ? "Control->Left wing"
                                                                                                              : "Control->Both wing";
            i2c_oled_menu(wing_label, pos, menu_options, "Next state", "Open", "Close", "Stop");
        }
    }
    return TIMER_RETURN;
}

menu_return_result_e ui_menu_control_menu()
{
    const uint8_t menu_options = 3;
    uint8_t pos = 1;
    button_event_t btn;
    while (RECEIVE_FROM_BTN_QUEUE(btn))
    {
        if (IS_BTN_PRESSED(btn, BTN1_PIN))
            pos = pos < menu_options ? pos + 1 : pos;
        else if (IS_BTN_PRESSED(btn, BTN3_PIN))
            pos = pos > 0 ? pos - 1 : pos;
        else if (IS_BTN_PRESSED(btn, BTN2_PIN))
        {
            menu_return_result_e return_result = 0;
            switch (pos)
            {
            case 0:
                return USER_RETURN;
                break;
            case 1:
                return_result = ui_menu_control_submenu(BOTH_WING);
                break;
            case 2:
                return_result = ui_menu_control_submenu(RIGHT_WING);
                break;
            case 3:
                return_result = ui_menu_control_submenu(LEFT_WING);
                break;
            default:
                break;
            }
            if (return_result == TIMER_RETURN)
                return TIMER_RETURN;
            pos = 0;
        }
        if (btn.event == BUTTON_RELESED)
            i2c_oled_menu("Control", pos, menu_options, "Both wings", "Right wing", "Left wing");
    }
    return TIMER_RETURN;
}

menu_return_result_e ui_menu_status_menu()
{
    button_event_t btn;
    i2c_oled_menu_status();
    while (RECEIVE_FROM_BTN_QUEUE(btn))
    {
        if (IS_BTN_PRESSED(btn, BTN2_PIN))
        {
            return USER_RETURN;
        }
    }
    return TIMER_RETURN;
}

menu_return_result_e ui_menu_main_menu()
{
    uint8_t pos = 0;
    button_event_t btn;
    while (RECEIVE_FROM_BTN_QUEUE(btn))
    {
        if (IS_BTN_PRESSED(btn, BTN1_PIN))
            pos = pos < 5 ? pos + 1 : pos;
        else if (IS_BTN_PRESSED(btn, BTN3_PIN))
            pos = pos > 0 ? pos - 1 : pos;
        else if (IS_BTN_PRESSED(btn, BTN2_PIN))
        {
            menu_return_result_e return_result = 0;
            switch (pos)
            {
            case 0:
                atomic_store(&ui_screen_menu, SCREEN_HOME);
                return USER_RETURN;
                break;
            case 1:
                return_result = ui_menu_control_menu();
                break;
            case 2:
                return_result = ui_menu_config_menu();
                break;
            case 3:
                return_result = ui_menu_status_menu();
                break;
            case 4:
                atomic_store(&ui_screen_menu, SCREEN_HOME);
                ghota_start_check();
                return 0xFF;
                break;
            case 5:
                io_buzzer(1, 50, 50);
                break;
            default:
                break;
            }
            if (return_result == 0xFF)
                return TIMER_RETURN;
        }
        if (btn.event == BUTTON_RELESED)
            i2c_oled_menu("Main menu", pos, 5, "Control", "Configuration", "Status", "OTA", "Beep");
    }
    return TIMER_RETURN;
}

void ui_oled_display_task(void *pvParameters)
{
    button_event_t btn_event;
    while (true)
    {
        if (uxSemaphoreGetCount(ui_task_mutex) == 0)
        {
            ESP_LOGI("CONTROL", "Suspending itself");
            i2c_oled_power_save(true);
            vTaskSuspend(NULL);
            i2c_oled_power_save(false);
        }
        switch (atomic_load(&ui_screen_menu))
        {
        case SCREEN_WELCOME:
            i2c_oled_welcome_screen();
            vTaskDelay(pdMS_TO_TICKS(3000));
            atomic_store(&ui_screen_menu, SCREEN_HOME);
            break;
        case SCREEN_HOME:
            static bool animation_toggle = false;
            static uint8_t counter = 0;
            counter++;
            if (counter % 3 == 0)
            {
                animation_toggle = !animation_toggle;
            }
            uint8_t screen_saver_time = (xTimerGetExpiryTime(ui_screen_saver_timer_handle) - xTaskGetTickCount()) / pdMS_TO_TICKS(1000);
            i2c_oled_home_screen(screen_saver_time, io_get_soc_temp(), wifi_connected(), wing_get_state(RIGHT_WING), wing_get_state(LEFT_WING), animation_toggle);
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
            atomic_store(&ui_screen_menu, SCREEN_HOME);
        default:
            break;
        }
        if (xQueueReceive(button_queue, &btn_event, pdMS_TO_TICKS(100)) && btn_event.pin == BTN1_PIN)
        {
            atomic_store(&ui_screen_menu, SCREEN_MENU);
        }
    }
}

debounce_t d_btn[3] = {
    {.pin = BTN1_PIN, .down_time = 0, .inverted = true, .history = 0xffff},
    {.pin = BTN2_PIN, .down_time = 0, .inverted = true, .history = 0xffff},
    {.pin = BTN3_PIN, .down_time = 0, .inverted = true, .history = 0xffff}};

static bool button_rose(debounce_t *d)
{
    if ((d->history & 0b1111000000111111) == 0b0000000000111111)
    {
        d->history = 0xffff;
        return 1;
    }
    return 0;
}
static bool button_fell(debounce_t *d)
{
    if ((d->history & 0b1111000000111111) == 0b1111000000000000)
    {
        d->history = 0x0000;
        return 1;
    }
    return 0;
}
static bool button_down(debounce_t *d)
{
    if (d->inverted)
        return button_fell(d);
    return button_rose(d);
}
static bool button_up(debounce_t *d)
{
    if (d->inverted)
        return button_rose(d);
    return button_fell(d);
}

static uint32_t millis()
{
    return esp_timer_get_time() / 1000;
}

static void send_event(debounce_t db, int ev)
{
    button_event_t event = {
        .pin = db.pin,
        .event = ev,
    };
    xQueueSend(button_queue, &event, 0);
}

void ui_button_handling_task(void *pvParameters)
{
    while (true)
    {
        for (int i = 0; i < sizeof(d_btn) / sizeof(debounce_t); i++)
        {
            d_btn[i].history = d_btn[i].history << 1 | gpio_get_level(d_btn[i].pin);
            if (button_up(&d_btn[i]))
            {
                d_btn[i].down_time = 0;
                send_event(d_btn[i], BUTTON_RELESED);
            }
            else if (d_btn[i].down_time && millis() >= d_btn[i].next_long_time)
            {
                d_btn[i].next_long_time = d_btn[i].next_long_time + BUTTON_LONG_PRESS_REPEAT_MS;
                send_event(d_btn[i], BUTTON_HELD);
            }
            else if (button_down(&d_btn[i]) && d_btn[i].down_time == 0)
            {
                d_btn[i].down_time = millis();
                d_btn[i].next_long_time = d_btn[i].down_time + BUTTON_LONG_PRESS_DURATION_MS;
                send_event(d_btn[i], BUTTON_PRESSED);
            }
        }
        if (uxSemaphoreGetCount(ui_task_mutex) == 0)
        {
            ESP_LOGI("BUTTON", "Suspending itself");
            vTaskSuspend(NULL);
            ESP_LOGI("BUTTON", "Resuming itself");
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
