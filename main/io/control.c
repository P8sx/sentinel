#include "control.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "common/config.h"
#include "io/io.h"

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
    ESP_LOGI(MOTOR_CONTROL_LOG_TAG,"opening M%i",motor->id);
    atomic_store(&(motor->state), OPENING);
    motor_id_t id = motor->id;

    /* Set semaphore to indicate that task can run and resume it */
    xSemaphoreGive((id == M1) ? motor_m1_mutex : motor_m2_mutex);
    vTaskResume((id == M1) ? motor_m1_task_handle : motor_m2_task_handle);

    io_motor_stop(id);
    vTaskDelay(pdMS_TO_TICKS(30));
    io_motor_dir(id,0);
    vTaskDelay(pdMS_TO_TICKS(15));
    io_motor_fade(id,1023, 2000);
}
static void control_close(motor_t *motor){
    ESP_LOGI(MOTOR_CONTROL_LOG_TAG,"closing M%i",motor->id);
    atomic_store(&(motor->state), CLOSING);
    motor_id_t id = motor->id;

    /* Set semaphore to indicate that task can run and resume it */
    xSemaphoreGive((id == M1) ? motor_m1_mutex : motor_m2_mutex);
    vTaskResume((id == M1) ? motor_m1_task_handle : motor_m2_task_handle);

    io_motor_stop(id);
    vTaskDelay(pdMS_TO_TICKS(30));
    io_motor_dir(id,1);
    vTaskDelay(pdMS_TO_TICKS(15));
    io_motor_fade(id,1023, 2000);
}
static void control_stop(motor_t *motor, uint8_t ext_stop){
    ESP_LOGI(MOTOR_CONTROL_LOG_TAG,"stopping M%i",motor->id);

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
            atomic_store(&(motor->open_pcnt), io_motor_get_pcnt(motor->id));
        } else if (state == CLOSING) {
            atomic_store(&(motor->state), CLOSED);
            atomic_store(&(motor->close_pcnt), io_motor_get_pcnt(motor->id));
        }
    }

    /* Set semaphore to stop motor task */
    xSemaphoreTake((motor->id == M1) ? motor_m1_mutex : motor_m2_mutex, 0);
    io_motor_stop(motor->id);

}

void next_state(motor_t *motor){
    switch (atomic_load(&(motor->state))){
    case OPENED:
        control_close(motor);
        break;
    case CLOSED:
        control_open(motor);
        break;
    case OPENING:
        control_stop(motor, true);
        break;
    case CLOSING:
        control_stop(motor, true);
        break;
    case STOPPED_OPENING:
        control_close(motor);
        break;
     case STOPPED_CLOSING:
        control_open(motor);  
        break;
    default:
        break;
    }
}

gate_states_t control_get_motor_state(motor_id_t id){
    return (id == M1) ? atomic_load(&(m1.state)) : atomic_load(&(m2.state));
}
int16_t control_get_motor_open_pcnt(motor_id_t id){
    return (id == M1) ? atomic_load(&(m1.open_pcnt)) : atomic_load(&(m2.open_pcnt));
}
int16_t control_get_motor_close_pcnt(motor_id_t id){
    return (id == M1) ? atomic_load(&(m1.close_pcnt)) : atomic_load(&(m2.close_pcnt));
    
}

void motor_init(){
    motor_action_queue = xQueueCreate(5, sizeof(gate_command_t));
    motor_m1_mutex = xSemaphoreCreateBinary();
    motor_m2_mutex = xSemaphoreCreateBinary();

    atomic_init(&(m1.open_pcnt), 0);
    atomic_init(&(m1.close_pcnt), 0);
    atomic_init(&(m1.state), UNKNOWN);
    atomic_init(&(m2.open_pcnt), 0);
    atomic_init(&(m2.close_pcnt), 0);
    atomic_init(&(m2.state), UNKNOWN);

    xSemaphoreTake(motor_m1_mutex, 0);
    xSemaphoreTake(motor_m2_mutex, 0);
}

void motor_action_task(void *pvParameters){
    static gate_command_t command;
    while(true){
        if (xQueueReceive(motor_action_queue, &command, portMAX_DELAY)) {
            motor_t *current_motor = (command.id == M1) ? &m1 : &m2;
            switch (command.action){
                case OPEN:
                    control_open(current_motor);
                    break;
                case CLOSE:
                    control_close(current_motor);
                    break;
                case STOP:
                    control_stop(current_motor, true);
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
    ESP_LOGI(MOTOR_LOG_TAG,"M%i Motor task initialized", id);
    while (true){
        if(uxSemaphoreGetCount((id == M1) ? motor_m1_mutex : motor_m2_mutex) == 0){
            ESP_LOGI(MOTOR_LOG_TAG,"M%i Motor task is suspending itself", id);
            vTaskSuspend(NULL);
            vTaskDelay(pdMS_TO_TICKS(200)); // Small delay for motor to ramp up
            ESP_LOGI(MOTOR_LOG_TAG,"M%i Motor task is being resumed", id);
        }
        if(io_motor_get_analog(id) == 0){
            ESP_LOGW(MOTOR_LOG_TAG,"%i1 Motor task is stopping mottor", id);
            control_stop((id == M1) ? &m1 : &m2, false);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }  
}
