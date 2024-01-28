#include "modbus.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "common/config.h"
#include "mbcontroller.h"
#include "esp_event.h"
#include "common/types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "io/io.h"
#include "io/gate.h"

// struct
// {
//     uint8_t discrete_input0 : 1;
//     uint8_t discrete_input1 : 1;
// } discrete_reg_params; // Read-only boolean
// struct
// {
//     uint8_t coils_port0 : 1;
//     uint8_t coils_port1 : 1;
// } coil_reg_params; // Read-write boolean

struct
{
    uint16_t right_wing_state;
    uint16_t left_wing_state;
} input_reg_params; // Read-only
struct
{
    uint16_t input_action;
    uint16_t rf_code_lsb_0;
    uint16_t rf_code_msb_0;
    uint16_t rf_code_lsb_1;
    uint16_t rf_code_msb_1;
} holding_reg_params; // Read-write

static portMUX_TYPE param_lock = portMUX_INITIALIZER_UNLOCKED;

static void modbus_io_rf_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    /* Only a hardware that has RF receiver moutned on PCB will write last RF code to register */
    if (device_config.hw_options && HW_RF_ENABLED)
    {
        uint64_t rf_code = *(uint64_t *)event_data;
        portENTER_CRITICAL(&param_lock);
        holding_reg_params.rf_code_lsb_0 = (uint16_t)(rf_code & 0xFFFF);
        holding_reg_params.rf_code_msb_0 = (uint16_t)((rf_code >> 16) & 0xFFFF);
        holding_reg_params.rf_code_lsb_1 = (uint16_t)((rf_code >> 32) & 0xFFFF);
        holding_reg_params.rf_code_msb_1 = (uint16_t)((rf_code >> 48) & 0xFFFF);
        portEXIT_CRITICAL(&param_lock);
    }
}

static void modbus_wing_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    wing_info_t wing_status = *(wing_info_t *)event_data;
    portENTER_CRITICAL(&param_lock);
    if (RIGHT_WING == wing_status.id)
    {
        input_reg_params.right_wing_state = wing_status.state;
    }
    else
    {
        input_reg_params.left_wing_state = wing_status.state;
    }
    portEXIT_CRITICAL(&param_lock);
}

void modbus_init()
{
    mb_communication_info_t comm_info = {
        .mode = MB_MODE_RTU,
        .port = UART_NUM_2,
        .slave_addr = device_config.modbus_slave_id,
        .baudrate = device_config.modbus_baudrate,
        .parity = device_config.modbus_parity,
    };

    void *mbc_slave_handler = NULL;
    ESP_ERROR_CHECK(mbc_slave_init(MB_PORT_SERIAL_SLAVE, &mbc_slave_handler));
    ESP_ERROR_CHECK(mbc_slave_setup((void *)&comm_info));

    mb_register_area_descriptor_t holding_reg_area = {
        .type = MB_PARAM_HOLDING,
        .start_offset = 0x0000,
        .address = (void *)&holding_reg_params,
        .size = sizeof(holding_reg_params),
    };
    ESP_ERROR_CHECK(mbc_slave_set_descriptor(holding_reg_area));

    mb_register_area_descriptor_t input_reg_area = {
        .type = MB_PARAM_INPUT,
        .start_offset = 0x0000,
        .address = (void *)&input_reg_params,
        .size = sizeof(input_reg_params),
    };
    ESP_ERROR_CHECK(mbc_slave_set_descriptor(input_reg_area));

    // mb_register_area_descriptor_t coil_reg_area = {
    //     .type = MB_PARAM_COIL,
    //     .start_offset = 0x0000,
    //     .address = (void *)&coil_reg_params,
    //     .size = sizeof(coil_reg_params),
    // };
    // ESP_ERROR_CHECK(mbc_slave_set_descriptor(coil_reg_area));

    // mb_register_area_descriptor_t discrete_reg_area = {
    //     .type = MB_PARAM_DISCRETE,
    //     .start_offset = 0x0000,
    //     .address = (void *)&discrete_reg_params,
    //     .size = sizeof(discrete_reg_params),
    // };
    // ESP_ERROR_CHECK(mbc_slave_set_descriptor(discrete_reg_area));

    ESP_ERROR_CHECK(mbc_slave_start());

    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, RS485_TX_PIN, RS485_RX_PIN, RS485_RTS_PIN, -1));
    ESP_ERROR_CHECK(uart_set_mode(UART_NUM_2, UART_MODE_RS485_HALF_DUPLEX));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(GATE_EVENTS, ESP_EVENT_ANY_ID, &modbus_wing_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IO_EVENTS, IO_RF_EVENT, &modbus_io_rf_event_handler, NULL, NULL));
}

void modbus_task(void *arg)
{
    mb_param_info_t reg_info;
    while (true)
    {
        (void)mbc_slave_check_event(MB_EVENT_HOLDING_REG_RD | MB_EVENT_HOLDING_REG_WR);
        ESP_ERROR_CHECK_WITHOUT_ABORT(mbc_slave_get_param_info(&reg_info, 10));
        ESP_LOGI(MODBUS_LOG_TAG, "INFO (%" PRIu32 " us), ADDR:%u, TYPE:%u, INST_ADDR:0x%" PRIx32 ", SIZE:%u", reg_info.time_stamp, (unsigned)reg_info.mb_offset, (unsigned)reg_info.type, (uint32_t)reg_info.address, (unsigned)reg_info.size);

        ESP_LOGI(MODBUS_LOG_TAG, "INST_ADDR:0x%" PRIx32 "", (uint32_t)&holding_reg_params.input_action);
        ESP_LOGI(MODBUS_LOG_TAG, "INST_ADDR:0x%" PRIx32 "", (uint32_t)&holding_reg_params.rf_code_lsb_0);
        ESP_LOGI(MODBUS_LOG_TAG, "INST_ADDR:0x%" PRIx32 "", (uint32_t)&holding_reg_params.rf_code_msb_0);
        ESP_LOGI(MODBUS_LOG_TAG, "INST_ADDR:0x%" PRIx32 "", (uint32_t)&holding_reg_params.rf_code_lsb_1);
        ESP_LOGI(MODBUS_LOG_TAG, "INST_ADDR:0x%" PRIx32 "", (uint32_t)&holding_reg_params.rf_code_msb_1);

        /* If master readed last RF code in register zero it out */
        if (reg_info.type & MB_EVENT_HOLDING_REG_RD && (uint8_t *)&holding_reg_params.rf_code_lsb_0 == reg_info.address)
        {
            ESP_LOGI(MODBUS_LOG_TAG, "RF code readed");
            portENTER_CRITICAL(&param_lock);
            holding_reg_params.rf_code_lsb_0 = 0;
            holding_reg_params.rf_code_msb_0 = 0;
            holding_reg_params.rf_code_lsb_1 = 0;
            holding_reg_params.rf_code_msb_1 = 0;
            portEXIT_CRITICAL(&param_lock);
        }
        /* If master written rf code to register post RF event and zero it out */
        else if (reg_info.type & MB_EVENT_HOLDING_REG_WR && (uint8_t *)&holding_reg_params.rf_code_lsb_0 == reg_info.address)
        {
            ESP_LOGI(MODBUS_LOG_TAG, "New RF code written");
            uint64_t rf_code = 0;
            portENTER_CRITICAL(&param_lock);
            rf_code |= (uint64_t)holding_reg_params.rf_code_lsb_0;
            holding_reg_params.rf_code_lsb_0 = 0;
            rf_code |= ((uint64_t)holding_reg_params.rf_code_msb_0 << 16);
            holding_reg_params.rf_code_msb_0 = 0;
            rf_code |= ((uint64_t)holding_reg_params.rf_code_lsb_1 << 32);
            holding_reg_params.rf_code_lsb_1 = 0;
            rf_code |= ((uint64_t)holding_reg_params.rf_code_msb_1 << 48);
            holding_reg_params.rf_code_msb_1 = 0;
            portEXIT_CRITICAL(&param_lock);
            esp_event_post(IO_EVENTS, IO_RF_EVENT, &rf_code, sizeof(uint64_t *), pdMS_TO_TICKS(200));
        }
        else if (reg_info.type & MB_EVENT_HOLDING_REG_WR && (uint8_t *)&holding_reg_params.input_action == reg_info.address)
        {
            ESP_LOGI(MODBUS_LOG_TAG, "New Input action written");
            portENTER_CRITICAL(&param_lock);
            wing_command_t cmd = INPUT_ACTION_TO_GATE_COMMAND(holding_reg_params.input_action);
            portEXIT_CRITICAL(&param_lock);
            xQueueSend(wing_action_queue, &cmd, pdMS_TO_TICKS(200));
        }
    }
}