#include "io.h"
#include "driver/gpio.h"
#include "common/config.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/pulse_cnt.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/temperature_sensor.h"
#include "esp_log.h"
#include "gate.h"
#include "esp_timer.h"

ESP_EVENT_DEFINE_BASE(IO_EVENTS);

static pcnt_unit_handle_t pcnt_unit_right_wing = NULL;
static pcnt_unit_handle_t pcnt_unit_left_wing = NULL;

static pcnt_channel_handle_t pcnt_chan_right_wing = NULL;
static pcnt_channel_handle_t pcnt_chan_left_wing = NULL;

static adc_cali_handle_t adc1_cali_right_wing_handle = NULL;
static adc_cali_handle_t adc1_cali_left_wing_handle = NULL;
static adc_oneshot_unit_handle_t adc1_handle = NULL;

static temperature_sensor_handle_t temp_handle = NULL;

void io_init_outputs();
void io_init_inputs();
void io_init_analog();
void io_init_pwm();
void io_init_pcnt();
void io_init_temp_sensor();

void io_init()
{
    io_init_outputs();
    io_init_inputs();
    io_init_analog();
    io_init_pwm();
    io_init_pcnt();
    io_init_temp_sensor();
}
/* All inputs form ISR are redirected to Control module using queue */
static void IRAM_ATTR input_isr_handler(void *arg)
{
    static TickType_t last_interrupt_time[11] = {0};
    TickType_t current_time = xTaskGetTickCountFromISR();

    static const uint8_t pins[] = {BTN1_PIN, BTN2_PIN, BTN3_PIN,
                                   INPUT1_PIN, INPUT2_PIN, INPUT3_PIN, INPUT4_PIN,
                                   ENDSTOP_RIGHT_WING_A_PIN, ENDSTOP_RIGHT_WING_B_PIN, ENDSTOP_LEFT_WING_A_PIN, ENDSTOP_LEFT_WING_B_PIN};
    static const TickType_t debounce_times[] = {pdMS_TO_TICKS(5000), pdMS_TO_TICKS(5000), pdMS_TO_TICKS(5000),
                                                pdMS_TO_TICKS(500), pdMS_TO_TICKS(500), pdMS_TO_TICKS(500), pdMS_TO_TICKS(500),
                                                pdMS_TO_TICKS(500), pdMS_TO_TICKS(500), pdMS_TO_TICKS(500), pdMS_TO_TICKS(500)};

    for (int i = 0; i < 11; ++i)
    {
        if (pins[i] == (intptr_t)arg && current_time - last_interrupt_time[i] > debounce_times[i])
        {
            last_interrupt_time[i] = current_time;
            esp_event_isr_post(IO_EVENTS, IO_INPUT_TRIGGERED_EVENT, &pins[i], sizeof(uint8_t), NULL);
            break;
        }
    }
}

static void IRAM_ATTR rf_isr_handler(void *arg)
{
    static volatile uint64_t last_change = 0;
    static volatile uint8_t preamble_count = 0;
    static volatile uint8_t bit_counter;
    static volatile uint8_t bit_array[66];
    static uint64_t serial_num = 0;
    volatile uint64_t cur_timestamp = esp_timer_get_time();
    volatile uint8_t cur_status = gpio_get_level(RF_RECEIVER_PIN);
    volatile uint32_t pulse_duration = cur_timestamp - last_change;
    
    last_change = cur_timestamp;
    
#define PULSE_BETWEEN(x,y) (pulse_duration > x && pulse_duration < y)

    if (preamble_count < 12)
    {
        if (cur_status == 1)
        {
            if (!(PULSE_BETWEEN(150,500) || preamble_count == 0))
            {
                preamble_count = 0;
                return;
            }
        }
        else
        {

            if (PULSE_BETWEEN(300,600))
            {
                preamble_count++;
                if (preamble_count == 12)
                {
                    bit_counter = 0;
                    return;
                }
            }
            else
            {
                preamble_count = 0;
                return;
            }
        }
    }

    if (12 == preamble_count)
    {
        if (1 == cur_status && !(PULSE_BETWEEN(250,900) || 0 == bit_counter))
        {
            preamble_count = 0;
            return;
        }
        else if(0 == cur_status)
        {

            if (PULSE_BETWEEN(250,900))
            {
                bit_array[65 - bit_counter] = (pulse_duration > 600) ? 0 : 1;
                bit_counter++;
                if (bit_counter == 66)
                {
                    preamble_count = 0;
                    serial_num = 0;
                    for (int i = 2; i < 34; i++)
                    {
                        serial_num = (serial_num << 1) + bit_array[i];
                    };
                    esp_event_isr_post(IO_EVENTS, IO_RF_EVENT, &serial_num, sizeof(uint64_t *), NULL);
                }
            }
            else
            {
                preamble_count = 0;
            }
        }
    }
}


void io_init_inputs()
{
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    gpio_config_t btn_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = ((1ULL << BTN1_PIN) | (1ULL << BTN2_PIN) | (1ULL << BTN3_PIN)),
        .pull_up_en = 0,
        .pull_down_en = 0};
    ESP_ERROR_CHECK(gpio_config(&btn_conf));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BTN1_PIN, input_isr_handler, (void *)BTN1_PIN));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BTN2_PIN, input_isr_handler, (void *)BTN2_PIN));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BTN3_PIN, input_isr_handler, (void *)BTN3_PIN));

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = ((1ULL << INPUT1_PIN) | (1ULL << INPUT2_PIN) | (1ULL << INPUT3_PIN) | (1ULL << INPUT4_PIN)),
        .pull_up_en = 0,
        .pull_down_en = 0};
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_isr_handler_add(INPUT1_PIN, input_isr_handler, (void *)INPUT1_PIN));
    ESP_ERROR_CHECK(gpio_isr_handler_add(INPUT2_PIN, input_isr_handler, (void *)INPUT2_PIN));
    ESP_ERROR_CHECK(gpio_isr_handler_add(INPUT3_PIN, input_isr_handler, (void *)INPUT3_PIN));
    ESP_ERROR_CHECK(gpio_isr_handler_add(INPUT4_PIN, input_isr_handler, (void *)INPUT4_PIN));

    gpio_config_t endstop_io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = ((1ULL << ENDSTOP_RIGHT_WING_A_PIN) | (1ULL << ENDSTOP_LEFT_WING_A_PIN) | (1ULL << ENDSTOP_RIGHT_WING_B_PIN) | (1ULL << ENDSTOP_LEFT_WING_B_PIN)),
        .pull_up_en = 0,
        .pull_down_en = 0};
    ESP_ERROR_CHECK(gpio_config(&endstop_io_conf));
    ESP_ERROR_CHECK(gpio_isr_handler_add(ENDSTOP_RIGHT_WING_A_PIN, input_isr_handler, (void *)ENDSTOP_RIGHT_WING_A_PIN));
    ESP_ERROR_CHECK(gpio_isr_handler_add(ENDSTOP_RIGHT_WING_B_PIN, input_isr_handler, (void *)ENDSTOP_RIGHT_WING_B_PIN));
    ESP_ERROR_CHECK(gpio_isr_handler_add(ENDSTOP_LEFT_WING_A_PIN, input_isr_handler, (void *)ENDSTOP_LEFT_WING_A_PIN));
    ESP_ERROR_CHECK(gpio_isr_handler_add(ENDSTOP_LEFT_WING_B_PIN, input_isr_handler, (void *)ENDSTOP_LEFT_WING_B_PIN));

    if(device_config.hw_options && HW_RF_ENABLED){
        gpio_config_t rf_config = {
            .intr_type = GPIO_INTR_ANYEDGE,
            .mode = GPIO_MODE_INPUT,
            .pin_bit_mask = (1ULL<<RF_RECEIVER_PIN),
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE
        };
        ESP_ERROR_CHECK(gpio_isr_handler_add(RF_RECEIVER_PIN, rf_isr_handler, NULL));

        ESP_ERROR_CHECK(gpio_config(&rf_config));
        ESP_LOGI(IO_LOG_TAG, "RF configured");
    }

}

void io_init_outputs()
{
    gpio_config_t relay_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = ((1ULL << RIGHT_WING_RLY_A_PIN) | (1ULL << RIGHT_WING_RLY_B_PIN) | (1ULL << LEFT_WING_RLY_A_PIN) | (1ULL << LEFT_WING_RLY_B_PIN)),
        .pull_up_en = 0,
        .pull_down_en = 0};
    ESP_ERROR_CHECK(gpio_config(&relay_conf));

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = ((1ULL << BUZZER_PIN) | (1ULL << OUT1_PIN) | (1ULL << OUT2_PIN)),
        .pull_up_en = 0,
        .pull_down_en = 0};
    ESP_ERROR_CHECK(gpio_config(&io_conf));   
}

static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

    if (!calibrated)
    {
        ESP_LOGI("ADC", "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK)
        {
            calibrated = true;
        }
    }

    *out_handle = handle;
    if (ret == ESP_OK)
    {
        ESP_LOGI("ADC", "Calibration Success");
    }
    else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated)
    {
        ESP_LOGW("ADC", "eFuse not burnt, skip software calibration");
    }
    else
    {
        ESP_LOGE("ADC", "Invalid arg or no memory");
    }

    return calibrated;
}

void io_init_analog()
{
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN,
    };
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle));

    example_adc_calibration_init(ADC_UNIT_1, RIGHT_WING_SENSE_CHANNEL, ADC_ATTEN, &adc1_cali_right_wing_handle);
    example_adc_calibration_init(ADC_UNIT_1, LEFT_WING_SENSE_CHANNEL, ADC_ATTEN, &adc1_cali_left_wing_handle);

    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, RIGHT_WING_SENSE_CHANNEL, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, LEFT_WING_SENSE_CHANNEL, &config));
}

void io_init_pwm()
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = PWM_RESOLUTION,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 20000,
        .clk_cfg = LEDC_AUTO_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel_0 = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = RIGHT_WING_PWM_PIN,
        .duty = 0,
        .hpoint = 0};
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_0));

    ledc_channel_config_t ledc_channel_1 = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = LEFT_WING_PWM_PIN,
        .duty = 0,
        .hpoint = 0};
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_1));
    ESP_ERROR_CHECK(ledc_fade_func_install(0));
}

void io_init_pcnt()
{
    pcnt_unit_config_t unit_config_right_wing = {
        .high_limit = SHRT_MAX,
        .low_limit = SHRT_MIN,
        .intr_priority = 3,
    };
    pcnt_unit_config_t unit_config_left_wing = {
        .high_limit = SHRT_MAX,
        .low_limit = SHRT_MIN,
        .intr_priority = 3,
    };
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config_right_wing, &pcnt_unit_right_wing));
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config_left_wing, &pcnt_unit_left_wing));

    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit_right_wing, &filter_config));
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit_left_wing, &filter_config));

    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = RIGHT_WING_PULSE_PIN,
        .level_gpio_num = -1,
    };
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit_right_wing, &chan_a_config, &pcnt_chan_right_wing));

    pcnt_chan_config_t chan_b_config = {
        .edge_gpio_num = LEFT_WING_PULSE_PIN,
        .level_gpio_num = -1,
    };
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit_left_wing, &chan_b_config, &pcnt_chan_left_wing));

    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_right_wing, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_LEVEL_ACTION_KEEP));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_right_wing, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_left_wing, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_LEVEL_ACTION_KEEP));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_left_wing, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit_right_wing));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit_right_wing));
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit_right_wing));

    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit_left_wing));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit_left_wing));
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit_left_wing));
}

void io_init_temp_sensor()
{
    temperature_sensor_config_t temp_sensor = {
        .range_min = -10,
        .range_max = 80,
    };
    ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor, &temp_handle));
    ESP_ERROR_CHECK(temperature_sensor_enable(temp_handle));
}

void io_wing_dir(wing_id_t id, uint8_t clockwise)
{
    pcnt_channel_handle_t pcnt_channel;
    gpio_num_t relay_pin_a, relay_pin_b;
    uint8_t wing_dir;

    if (RIGHT_WING == id)
    {
        pcnt_channel = pcnt_chan_right_wing;
        relay_pin_a = RIGHT_WING_RLY_A_PIN;
        relay_pin_b = RIGHT_WING_RLY_B_PIN;
        wing_dir = device_config.right_wing_dir;
    }
    else
    {
        pcnt_channel = pcnt_chan_left_wing;
        relay_pin_a = LEFT_WING_RLY_A_PIN;
        relay_pin_b = LEFT_WING_RLY_B_PIN;
        wing_dir = device_config.left_wing_dir;
    }

    if (wing_dir == clockwise)
    {
        ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_channel, PCNT_CHANNEL_LEVEL_ACTION_INVERSE, PCNT_CHANNEL_LEVEL_ACTION_KEEP));
        gpio_set_level(relay_pin_a, 1);
        gpio_set_level(relay_pin_b, 0);
    }
    else
    {
        ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_channel, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
        gpio_set_level(relay_pin_a, 0);
        gpio_set_level(relay_pin_b, 1);
    }
}

void io_wing_stop(wing_id_t id)
{
    if (RIGHT_WING == id)
    {
        ledc_fade_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        io_wing_pwm(id, 0);
        gpio_set_level(RIGHT_WING_RLY_A_PIN, 0);
        gpio_set_level(RIGHT_WING_RLY_B_PIN, 0);
    }
    else
    {
        ledc_fade_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
        io_wing_pwm(id, 0);
        gpio_set_level(LEFT_WING_RLY_A_PIN, 0);
        gpio_set_level(LEFT_WING_RLY_B_PIN, 0);
    }
}

void io_wing_pwm(wing_id_t id, uint32_t freq)
{
    uint32_t frequency = freq > PWM_LIMIT ? PWM_LIMIT : freq;
    if (RIGHT_WING == id)
    {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, frequency);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    }
    else
    {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, frequency);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
    }
}

void io_wing_fade(wing_id_t id, uint32_t target, uint32_t time)
{
    if (RIGHT_WING == id)
    {
        ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, target, time);
        ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);
    }
    else
    {
        ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, target, time);
        ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, LEDC_FADE_NO_WAIT);
    }
}

int32_t io_wing_get_pcnt(wing_id_t id)
{
    int value = 0;
    if (RIGHT_WING == id)
    {
        pcnt_unit_get_count(pcnt_unit_right_wing, &value);
    }
    else
    {
        pcnt_unit_get_count(pcnt_unit_left_wing, &value);
    }
    return value;
}

uint16_t io_wing_get_current(wing_id_t id)
{
    int adc_value = 0;
    int voltage_value = 0;
    adc_oneshot_read(adc1_handle, (RIGHT_WING == id) ? RIGHT_WING_SENSE_CHANNEL : LEFT_WING_SENSE_CHANNEL, &adc_value);
    adc_cali_raw_to_voltage((RIGHT_WING == id) ? adc1_cali_right_wing_handle : adc1_cali_left_wing_handle, adc_value, &voltage_value);
    float current = (((float)voltage_value) / OP_AMP_GAIN) / (SHUNT_VALUE);
    return (uint16_t)(current * 1000);
}

void io_buzzer(uint8_t counts, uint16_t on_period, uint16_t off_period)
{
    for (uint8_t i = 0; i < counts; ++i)
    {
        gpio_set_level(BUZZER_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(on_period));
        gpio_set_level(BUZZER_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(off_period));
    }
}

float io_get_soc_temp()
{
    float tsens_out;
    ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_handle, &tsens_out));
    return tsens_out;
}
