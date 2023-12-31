#include "io.h"
#include "driver/gpio.h"
#include "common/config.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/pulse_cnt.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"


static pcnt_unit_handle_t pcnt_unit_m1 = NULL;
static pcnt_unit_handle_t pcnt_unit_m2 = NULL;

static pcnt_channel_handle_t pcnt_chan_m1 = NULL;
static pcnt_channel_handle_t pcnt_chan_m2 = NULL;

static adc_cali_handle_t adc1_cali_m1_handle = NULL;
static adc_cali_handle_t adc1_cali_m2_handle = NULL;
static adc_oneshot_unit_handle_t adc1_handle = NULL;

void io_init_inputs(){

    gpio_config_t btn_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = ((1ULL<<BTN1_PIN) | (1ULL<<BTN2_PIN) | (1ULL<<BTN3_PIN)) ,
        .pull_up_en = 0,
        .pull_down_en = 0
    };
    ESP_ERROR_CHECK(gpio_config(&btn_conf));
    
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = ((1ULL<<INPUT1_PIN) | (1ULL<<INPUT2_PIN) | (1ULL<<INPUT3_PIN) | (1ULL<<INPUT4_PIN) | (1ULL<<INPUT5_PIN) | (1ULL<<INPUT6_PIN) | (1ULL<<INPUT7_PIN) | (1ULL<<INPUT8_PIN))  ,
        .pull_up_en = 0,
        .pull_down_en = 0
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
}

void io_init_outputs(){
    gpio_config_t relay_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = ((1ULL<<M1_RLY_A_PIN) | (1ULL<<M1_RLY_B_PIN) | (1ULL<<M2_RLY_A_PIN) | (1ULL<<M2_RLY_B_PIN)) ,
        .pull_up_en = 0,
        .pull_down_en = 0
    };
    ESP_ERROR_CHECK(gpio_config(&relay_conf));

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = ((1ULL<<BUZZER_PIN) | (1ULL<<OUT1_PIN) | (1ULL<<OUT2_PIN)) ,
        .pull_up_en = 0,
        .pull_down_en = 0
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
}

static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI("ADC", "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI("ADC", "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI("ADC", "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW("ADC", "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE("ADC", "Invalid arg or no memory");
    }

    return calibrated;
}

void io_init_analog(){
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN,
    };
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle));

    example_adc_calibration_init(ADC_UNIT_1, M1_SENSE_CHANNEL, ADC_ATTEN, &adc1_cali_m1_handle);
    example_adc_calibration_init(ADC_UNIT_1, M2_SENSE_CHANNEL, ADC_ATTEN, &adc1_cali_m2_handle);

    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, M1_SENSE_CHANNEL, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, M2_SENSE_CHANNEL, &config));
}

void io_init_pwm(){
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .duty_resolution  = PWM_RESOLUTION,
        .timer_num        = LEDC_TIMER_0,
        .freq_hz          = 20000, 
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel_0 = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = M1_PWM_PIN,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_0));

    ledc_channel_config_t ledc_channel_1 = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_1,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = M2_PWM_PIN,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_1));
    ESP_ERROR_CHECK(ledc_fade_func_install(0));
}

void io_init_pcnt(){
    pcnt_unit_config_t unit_config_m1 = {
        .high_limit = SHRT_MAX,
        .low_limit = SHRT_MIN,
        .intr_priority = 3,
    };
    pcnt_unit_config_t unit_config_m2 = {
        .high_limit = SHRT_MAX,
        .low_limit = SHRT_MIN,
        .intr_priority = 3,
    };
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config_m1, &pcnt_unit_m1));
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config_m2, &pcnt_unit_m2));

    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit_m1, &filter_config));
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit_m2, &filter_config));

    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = M1_PULSE_PIN,
        .level_gpio_num = -1,
    };
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit_m1, &chan_a_config, &pcnt_chan_m1));

    pcnt_chan_config_t chan_b_config = {
        .edge_gpio_num = M2_PULSE_PIN,
        .level_gpio_num = -1,
    };
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit_m2, &chan_b_config, &pcnt_chan_m2));

    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_m1, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_LEVEL_ACTION_KEEP));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_m1, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_m2, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_LEVEL_ACTION_KEEP));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_m2, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit_m1));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit_m1));
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit_m1));

    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit_m2));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit_m2));
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit_m2));
}

void io_motor_dir(motor_id_t id, uint8_t clockwise) {
    pcnt_channel_handle_t pcnt_channel;
    gpio_num_t relay_pin_a, relay_pin_b;
    uint8_t motor_dir;

    if(M1 == id){
        pcnt_channel = pcnt_chan_m1;
        relay_pin_a = M1_RLY_A_PIN;
        relay_pin_b = M1_RLY_B_PIN;
        motor_dir = atomic_load(&(device_config.m1_dir));
    } else {
        pcnt_channel = pcnt_chan_m2;
        relay_pin_a = M2_RLY_A_PIN;
        relay_pin_b = M2_RLY_B_PIN;
        motor_dir = atomic_load(&(device_config.m2_dir));
    }

    if (motor_dir == clockwise) {
        ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_channel, PCNT_CHANNEL_LEVEL_ACTION_INVERSE, PCNT_CHANNEL_LEVEL_ACTION_KEEP));
        gpio_set_level(relay_pin_a, 1);
        gpio_set_level(relay_pin_b, 0);
    } else {
        ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_channel, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
        gpio_set_level(relay_pin_a, 0);
        gpio_set_level(relay_pin_b, 1);
    }
}

void io_motor_stop(motor_id_t id){
    if(M1 == id){
        ledc_fade_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        io_motor_pwm(id, 0);
        gpio_set_level(M1_RLY_A_PIN, 0);
        gpio_set_level(M1_RLY_B_PIN, 0);
    }
    else{
        ledc_fade_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
        io_motor_pwm(id, 0);
        gpio_set_level(M2_RLY_A_PIN, 0);
        gpio_set_level(M2_RLY_B_PIN, 0);
    }

}

void io_motor_pwm(motor_id_t id, uint32_t freq){
    uint32_t frequency = freq>PWM_LIMIT?PWM_LIMIT:freq;
    if(M1 == id){
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, frequency);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    }
    else{
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, frequency);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
    }

}


void io_motor_fade(motor_id_t id, uint32_t target, uint32_t time){
    if(M1 == id){
        ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, target, time);
        ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);
    }
    else{
        ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, target, time);
        ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, LEDC_FADE_NO_WAIT);
    }

}

int32_t io_motor_get_pcnt(motor_id_t id){
    int value = 0;
    if(M1 == id){
        pcnt_unit_get_count(pcnt_unit_m1, &value);
    }
    else{
        pcnt_unit_get_count(pcnt_unit_m2, &value);
    }
    return value;
}

uint16_t io_motor_get_current(motor_id_t id){
    int adc_value = 0;
    int voltage_value = 0;
    adc_oneshot_read(adc1_handle, (M1 == id) ? M1_SENSE_CHANNEL : M2_SENSE_CHANNEL, &adc_value);
    adc_cali_raw_to_voltage((M1 == id) ? adc1_cali_m1_handle : adc1_cali_m2_handle, adc_value, &voltage_value);
    float current = (((float)voltage_value)/OP_AMP_GAIN) / (SHUNT_VALUE); 
    return (uint16_t)(current * 1000);
}


void io_buzzer(uint8_t counts, uint16_t on_period, uint16_t off_period){
       for (uint8_t i = 0; i < counts; ++i) {
        gpio_set_level(BUZZER_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(on_period));
        gpio_set_level(BUZZER_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(off_period));
    }
}