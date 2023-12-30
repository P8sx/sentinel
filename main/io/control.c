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

static SemaphoreHandle_t motor_action_mutex = NULL;

void control_open(motor_t *motor){
    motor->state = OPENING;
    xSemaphoreGive(motor_action_mutex);
    ESP_LOGI(MOTOR_CONTROL_LOG_TAG,"opening M%i",motor->id);
    if(motor->id == M1){
        vTaskResume(motor_m1_task_handle);
        io_m1_stop();
        vTaskDelay(pdMS_TO_TICKS(30));
        io_m1_dir(0);
        vTaskDelay(pdMS_TO_TICKS(15));
        io_m1_fade(1023, 2000);
    }
    else{
        vTaskResume(motor_m2_task_handle);
        io_m2_stop();
        vTaskDelay(pdMS_TO_TICKS(30));
        io_m2_dir(0);
        vTaskDelay(pdMS_TO_TICKS(15));
        io_m2_fade(1023, 2000);
    }
    /* wrum wrum open */
}
void control_close(motor_t *motor){
    motor->state = CLOSING;
    xSemaphoreGive(motor_action_mutex);
    ESP_LOGI(MOTOR_CONTROL_LOG_TAG,"closing M%i",motor->id);
    if(motor->id == M1){
        vTaskResume(motor_m1_task_handle);
        io_m1_stop();
        vTaskDelay(pdMS_TO_TICKS(30));
        io_m1_dir(1);
        vTaskDelay(pdMS_TO_TICKS(15));
        io_m1_fade(1023, 2000);
    }
    else{
        vTaskResume(motor_m2_task_handle);
        io_m2_stop();
        vTaskDelay(pdMS_TO_TICKS(30));
        io_m2_dir(1);
        vTaskDelay(pdMS_TO_TICKS(15));
        io_m2_fade(1023, 2000);
    }
}
void control_stop(motor_t *motor){
    if(motor->state == OPENING) {
        motor->state = OPENED;
        motor->open_pcnt = (motor->id == M1) ? io_get_pcnt_m1() : io_get_pcnt_m2();
    }
    else if (motor->state == CLOSING) {
        motor->state = CLOSED;
        motor->close_pcnt = (motor->id == M1) ? io_get_pcnt_m1() : io_get_pcnt_m2();
    }
    xSemaphoreGive(motor_action_mutex);
    ESP_LOGI(MOTOR_CONTROL_LOG_TAG,"stopping M%i",motor->id);
    if(motor->id == M1){
        io_m1_stop();
    }
    else{
        io_m2_stop();
    }
}
void next_state(motor_t *motor){
    switch (motor->state)    {
    case OPENED:
        control_close(motor);
        break;
    case CLOSED:
        control_open(motor);
        break;
    case OPENING:
        motor->state = STOPPED_OPENING;
        control_stop(motor);
        break;
    case CLOSING:
        motor->state = STOPPED_CLOSING;
        control_stop(motor);
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

states_t control_get_motor_state(motor_id_t id){
    states_t state = UNKNOWN;
    if(xSemaphoreTake(motor_action_mutex, pdMS_TO_TICKS(10))){
        state = (id == M1) ? m1.state : m2.state;
        xSemaphoreGive(motor_action_mutex);
        return state;
    }
    ESP_LOGW(MOTOR_CONTROL_LOG_TAG, "Unable to acquire semaphore");
    return state;
}


void motor_action_task(void *pvParameters){
    motor_action_queue = xQueueCreate(5, sizeof(motor_command_t));
    motor_action_mutex = xSemaphoreCreateMutex();
    xSemaphoreGive(motor_action_mutex);
    
    static motor_command_t command;
    while(true){
        if (xQueueReceive(motor_action_queue, &command, portMAX_DELAY) && xSemaphoreTake(motor_action_mutex, pdMS_TO_TICKS(5))) {
            motor_t *current_motor = (command.id == M1) ? &m1 : &m2;
            switch (command.action){
                case OPEN:
                    control_open(current_motor);
                    break;
                case CLOSE:
                    control_close(current_motor);
                    break;
                case STOP:
                    control_stop(current_motor);
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
void motor_m1_task(void *pvParameters){
    while (true){
        if(io_get_analog_m1() == 0){
            motor_command_t cmd = {.action = STOP, .id = M1};
            xQueueSendToFront(motor_action_queue, &cmd, portMAX_DELAY);
        }
        if(io_get_analog_m1() == 0){
            ESP_LOGI(MOTOR_LOG_TAG,"M1 Motor task is suspending itself");
            vTaskSuspend(NULL);
            vTaskDelay(pdMS_TO_TICKS(200));
            ESP_LOGI(MOTOR_LOG_TAG,"M1 Motor task is being resumed");
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
void motor_m2_task(void *pvParameters){
    while (true){
        if(io_get_analog_m2() == 0){
            motor_command_t cmd = {.action = STOP, .id = M2};
            xQueueSendToFront(motor_action_queue, &cmd, portMAX_DELAY);
        }

        if(io_get_analog_m2() == 0){
            ESP_LOGI(MOTOR_LOG_TAG,"M2 Motor task is suspending itself");
            vTaskSuspend(NULL);
            vTaskDelay(pdMS_TO_TICKS(200));
            ESP_LOGI(MOTOR_LOG_TAG,"M2 Motor task is being resumed");
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
