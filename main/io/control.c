#include "control.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "common/config.h"
#include "io/io.h"
#include "math.h"
/* extern variables */
QueueHandle_t motor_action_queue = NULL;
extern TaskHandle_t motor_m1_task_handle;
extern TaskHandle_t motor_m2_task_handle;
/* Initial state is assumed to be closed */
static motor_t m1 = {.id = M1, .state = CLOSED};
static motor_t m2 = {.id = M2, .state = CLOSED};

static SemaphoreHandle_t motor_m1_mutex = NULL;
static SemaphoreHandle_t motor_m2_mutex = NULL;

static void control_open(motor_t *motor){
    if(atomic_load(&(motor->state)) == OPENING) return;
    
    motor_id_t id = motor->id;
    ESP_LOGI(MOTOR_CONTROL_LOG_TAG,"opening M%i",id);
    atomic_store(&(motor->state), OPENING);

    /* Set semaphore to indicate that task can run and resume it */
    xSemaphoreGive((M1 == id) ? motor_m1_mutex : motor_m2_mutex);
    vTaskResume((M1 == id) ? motor_m1_task_handle : motor_m2_task_handle);

    io_motor_stop(id);
    vTaskDelay(pdMS_TO_TICKS(30));
    io_motor_dir(id,0);
    vTaskDelay(pdMS_TO_TICKS(15));
    io_motor_fade(id,1023, 1000);
}

static void control_close(motor_t *motor){
    if(atomic_load(&(motor->state)) == CLOSING) return;
    
    motor_id_t id = motor->id;
    ESP_LOGI(MOTOR_CONTROL_LOG_TAG,"closing M%i",id);
    atomic_store(&(motor->state), CLOSING);

    /* Set semaphore to indicate that task can run and resume it */
    xSemaphoreGive((M1 == id) ? motor_m1_mutex : motor_m2_mutex);
    vTaskResume((M1 == id) ? motor_m1_task_handle : motor_m2_task_handle);

    io_motor_stop(id);
    vTaskDelay(pdMS_TO_TICKS(30));
    io_motor_dir(id,1);
    vTaskDelay(pdMS_TO_TICKS(15));
    io_motor_fade(id,1023, 1000);
}

static void control_stop(motor_t *motor, uint8_t ext_stop){
    motor_id_t id = motor->id;

    gate_states_t state = atomic_load(&(motor->state));
    /* If stop is issued by user/error etc. change state to stopped_.... */
    if (ext_stop) {
        if (state == OPENING) {
            atomic_store(&(motor->state), STOPPED_OPENING);
        } else if (state == CLOSING) {
            atomic_store(&(motor->state), STOPPED_CLOSING);
        }
    /* If stop is issued by hardware e.g. hardware endstop or reaching certain pcnt */
    } else {
        if (state == OPENING) {
            atomic_store(&(motor->state), OPENED);
            atomic_store(&(motor->open_pcnt), io_motor_get_pcnt(id));
            atomic_store(&(motor->open_pcnt_cal), true);
        } else if (state == CLOSING) {
            atomic_store(&(motor->state), CLOSED);
            atomic_store(&(motor->close_pcnt), io_motor_get_pcnt(id));
            atomic_store(&(motor->close_pcnt_cal), true);
        }
    }

    /* Set semaphore to stop motor task */
    xSemaphoreTake((M1 == id) ? motor_m1_mutex : motor_m2_mutex, 0);
    io_motor_stop(id);

}

void next_state(motor_t *motor){
    switch (atomic_load(&(motor->state))){
    case OPENED:
        control_close(motor);
        ESP_LOGI(MOTOR_CONTROL_LOG_TAG,"closing M%i",motor->id);
        break;
    case CLOSED:
        control_open(motor);
        ESP_LOGI(MOTOR_CONTROL_LOG_TAG,"opening M%i",motor->id);
        break;
    case OPENING:
        control_stop(motor, true);
        ESP_LOGI(MOTOR_CONTROL_LOG_TAG,"stopping M%i",motor->id);
        break;
    case CLOSING:
        control_stop(motor, true);
        ESP_LOGI(MOTOR_CONTROL_LOG_TAG,"stopping M%i",motor->id);
        break;
    case STOPPED_OPENING:
        control_close(motor);
        ESP_LOGI(MOTOR_CONTROL_LOG_TAG,"closing M%i",motor->id);
        break;
     case STOPPED_CLOSING:
        control_open(motor);  
        ESP_LOGI(MOTOR_CONTROL_LOG_TAG,"opening M%i",motor->id);
        break;
    default:
        break;
    }
}

gate_states_t control_get_motor_state(motor_id_t id){
    return (M1 == id) ? atomic_load(&(m1.state)) : atomic_load(&(m2.state));
}
int16_t control_get_motor_open_pcnt(motor_id_t id){
    return (M1 == id) ? atomic_load(&(m1.open_pcnt)) : atomic_load(&(m2.open_pcnt));
}
int16_t control_get_motor_close_pcnt(motor_id_t id){
    return (M1 == id) ? atomic_load(&(m1.close_pcnt)) : atomic_load(&(m2.close_pcnt));
    
}

void control_motor_init(){
    motor_action_queue = xQueueCreate(5, sizeof(gate_command_t));
    motor_m1_mutex = xSemaphoreCreateBinary();
    motor_m2_mutex = xSemaphoreCreateBinary();

    atomic_init(&(m1.open_pcnt), 0);
    atomic_init(&(m1.close_pcnt), 0);
    atomic_init(&(m1.state), UNKNOWN);
    atomic_init(&(m1.open_pcnt_cal), false);
    atomic_init(&(m1.close_pcnt_cal), false);

    atomic_init(&(m2.open_pcnt), 0);
    atomic_init(&(m2.close_pcnt), 0);
    atomic_init(&(m2.state), UNKNOWN);
    atomic_init(&(m2.open_pcnt_cal), false);
    atomic_init(&(m2.close_pcnt_cal), false);

    xSemaphoreTake(motor_m1_mutex, 0);
    xSemaphoreTake(motor_m2_mutex, 0);
}

void motor_action_task(void *pvParameters){
    static gate_command_t command;
    while(true){
        if (xQueueReceive(motor_action_queue, &command, portMAX_DELAY)) {
            motor_t *current_motor = (M1 == command.id) ? &m1 : &m2;
            switch (command.action){
                case OPEN:
                    control_open(current_motor);
                    ESP_LOGI(MOTOR_CONTROL_LOG_TAG,"opening M%i",command.id);
                    break;
                case CLOSE:
                    control_close(current_motor);
                    ESP_LOGI(MOTOR_CONTROL_LOG_TAG,"closing M%i",command.id);
                    break;
                case STOP:
                    control_stop(current_motor, true);
                    ESP_LOGI(MOTOR_CONTROL_LOG_TAG,"stopping M%i",command.id);
                    break;
                case NEXT_STATE:
                    next_state(current_motor);
                    break;
                default:
                    /* Should never reach but... */
                    control_close(current_motor);
                    break;
            }
        }
        taskYIELD();
    }
}

void motor_task(void *pvParameters){
    motor_id_t id = (motor_id_t)pvParameters;
    motor_t *motor = (M1 == id) ? &m1 : &m2;
    /* OCP Protection variables*/
    int16_t ocp_count = 0;
    int16_t cfg_ocp_count = 0;
    int16_t cfg_ocp_treshold = 0;
    
    /* No current stop debounce */
    uint8_t no_current_stop = 0;

    ESP_LOGI(MOTOR_LOG_TAG,"M%i Motor task initialized", id);

    while(true){
        /* Task is only running when motor is truning */
        if(uxSemaphoreGetCount((M1 == id) ? motor_m1_mutex : motor_m2_mutex) == 0){
            ESP_LOGI(MOTOR_LOG_TAG,"M%i Motor task is suspending itself", id);
            vTaskSuspend(NULL);
            ESP_LOGI(MOTOR_LOG_TAG,"M%i Motor task is being resumed", id);

            cfg_ocp_count = (M1 == id) ? atomic_load(&(device_config.m1_ocp_count)) : atomic_load(&(device_config.m2_ocp_count));
            cfg_ocp_treshold = (M1 == id) ? atomic_load(&(device_config.m1_ocp_treshold)) : atomic_load(&(device_config.m1_ocp_treshold));
            ocp_count = 0;
            vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for motor to ramp up
        }

        const uint16_t current = io_motor_get_current(id);

        /* Stop motor if current is 0(motor stopped by builtin endstops) */
        if(0 == current){
            no_current_stop++;
        }
        if(no_current_stop > 30){
            no_current_stop = 0;
            ESP_LOGW(MOTOR_LOG_TAG,"M%i Motor task is stopping mottor", id);
            control_stop(motor, false);
        }


        /* Stop motor if overcurrent event occured */
        if(0 != cfg_ocp_count && 0 != cfg_ocp_treshold && current > cfg_ocp_treshold){
            if(current > cfg_ocp_treshold * 1.8) ocp_count += 10;
            else if(current > cfg_ocp_treshold * 1.4) ocp_count += 8;
            else if(current > cfg_ocp_treshold * 1.2) ocp_count += 6;
            else ocp_count++;            
            ESP_LOGW(MOTOR_LOG_TAG,"M%i OCP Count increased: %i, current: %imA", id, ocp_count, current);       
        }
        if(ocp_count > cfg_ocp_count){
            ESP_LOGE(MOTOR_LOG_TAG,"M%i Motor task is stopping mottor due to overcurrent event", id);
            control_stop(motor, true); 
            io_buzzer(5, 50, 50);
        }

        /* Stop motor if endstop is reached */

        /* more stop/monitor condition to be defined */

        /* PCNT slow down*/
        // if(atomic_load(&(motor->open_pcnt_cal)) && atomic_load(&(motor->close_pcnt_cal))){
        //     int16_t open_pcnt = control_get_motor_open_pcnt(id);
        //     int16_t close_pcnt = control_get_motor_close_pcnt(id);
        //     int32_t total_distance = abs(open_pcnt) + abs(close_pcnt);

        //     gate_states_t current_state = control_get_motor_state(id);
        //     if(current_state == CLOSING){
        //         int16_t slow_down_point = close_pcnt + total_distance * 0.20;
        //         int16_t current_pcnt = io_motor_get_pcnt(id);

        //     }
        // }
        vTaskDelay(pdMS_TO_TICKS(10));
    }  
}
