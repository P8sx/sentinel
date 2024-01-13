#include "gate.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "common/config.h"
#include "io/io.h"
#include "math.h"
#include "esp_ghota.h"
#include "esp_err.h"

ESP_EVENT_DEFINE_BASE(GATE_EVENTS);
/* extern variables */
QueueHandle_t gate_action_queue = NULL;
extern TaskHandle_t gate_m1_task_handle;
extern TaskHandle_t gate_m2_task_handle;

/* Initial state is assumed to be closed */
static gate_t m1 = {.id = M1, .state = CLOSED};
static gate_t m2 = {.id = M2, .state = CLOSED};

static SemaphoreHandle_t gate_m1_mutex = NULL;
static SemaphoreHandle_t gate_m2_mutex = NULL;
static SemaphoreHandle_t gate_action_task_mutex = NULL;


static void gate_open(gate_t *motor);
static void gate_close(gate_t *motor);
static void gate_stop(gate_t *motor, bool hw_stop);

static void gate_ghota_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);


void gate_module_init(){
    gate_action_queue = xQueueCreate(5, sizeof(gate_command_t));
    gate_m1_mutex = xSemaphoreCreateBinary();
    gate_m2_mutex = xSemaphoreCreateBinary();
    gate_action_task_mutex = xSemaphoreCreateBinary();

    io_motor_stop(M1);
    io_motor_stop(M2);

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

    ESP_ERROR_CHECK(esp_event_handler_instance_register(GHOTA_EVENTS, ESP_EVENT_ANY_ID, &gate_ghota_event_handler, NULL, NULL));

    xSemaphoreTake(gate_m1_mutex, 0);
    xSemaphoreTake(gate_m2_mutex, 0);
    xSemaphoreGive(gate_action_task_mutex);
}

static void gate_open(gate_t *motor){
    if(atomic_load(&(motor->state)) == OPENING) return;
    
    motor_id_t id = motor->id;
    ESP_LOGI(GATE_CONTROL_LOG_TAG,"opening M%i",id);
    atomic_store(&(motor->state), OPENING);

    /* Set semaphore to indicate that task can run and resume it */
    xSemaphoreGive((M1 == id) ? gate_m1_mutex : gate_m2_mutex);
    vTaskResume((M1 == id) ? gate_m1_task_handle : gate_m2_task_handle);

    io_motor_stop(id);
    vTaskDelay(pdMS_TO_TICKS(30));
    io_motor_dir(id,0);
    vTaskDelay(pdMS_TO_TICKS(15));
    io_motor_fade(id,1023, 1000);

    /* Post event of status change */
    gate_status_t gate_state = {
        .id = id,
        .state = gate_get_state(id),
    };
    esp_event_post(GATE_EVENTS, MOTOR_STATUS_CHANGED, &gate_state, sizeof(gate_status_t), pdMS_TO_TICKS(100));
}

static void gate_close(gate_t *motor){
    if(atomic_load(&(motor->state)) == CLOSING) return;
    
    motor_id_t id = motor->id;
    ESP_LOGI(GATE_CONTROL_LOG_TAG,"closing M%i",id);
    atomic_store(&(motor->state), CLOSING);

    /* Set semaphore to indicate that task can run and resume it */
    xSemaphoreGive((M1 == id) ? gate_m1_mutex : gate_m2_mutex);
    vTaskResume((M1 == id) ? gate_m1_task_handle : gate_m2_task_handle);

    io_motor_stop(id);
    vTaskDelay(pdMS_TO_TICKS(30));
    io_motor_dir(id,1);
    vTaskDelay(pdMS_TO_TICKS(15));
    io_motor_fade(id,1023, 1000);

    /* Post event of status change */
    gate_status_t gate_state = {
        .id = id,
        .state = gate_get_state(id),
    };
    esp_event_post(GATE_EVENTS, MOTOR_STATUS_CHANGED, &gate_state, sizeof(gate_status_t), pdMS_TO_TICKS(100));
}

static void gate_stop(gate_t *motor, bool hw_stop){
    motor_id_t id = motor->id;

    gate_state_t state = atomic_load(&(motor->state));
    /* If stop is issued by user/error etc. change state to stopped_.... */
    if (!hw_stop) {
        if (state == OPENING) {
            atomic_store(&(motor->state), STOPPED_OPENING);
        } else if (state == CLOSING) {
            atomic_store(&(motor->state), STOPPED_CLOSING);
        }
    /* If stop is issued by hardware e.g. hardware endstop or reaching certain pcnt change to closed/opened */
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
    xSemaphoreTake((M1 == id) ? gate_m1_mutex : gate_m2_mutex, 0);
    io_motor_stop(id);

    /* Post event of status change */
    gate_status_t gate_state = {
        .id = id,
        .state = gate_get_state(id),
    };
    esp_event_post(GATE_EVENTS, MOTOR_STATUS_CHANGED, &gate_state, sizeof(gate_status_t), pdMS_TO_TICKS(100));
}

static void gate_next_state(gate_t *motor){
    switch (atomic_load(&(motor->state))){
    case OPENED:
        gate_close(motor);
        ESP_LOGI(GATE_CONTROL_LOG_TAG,"closing M%i",motor->id);
        break;
    case CLOSED:
        gate_open(motor);
        ESP_LOGI(GATE_CONTROL_LOG_TAG,"opening M%i",motor->id);
        break;
    case OPENING:
        gate_stop(motor, false);
        ESP_LOGI(GATE_CONTROL_LOG_TAG,"stopping M%i",motor->id);
        break;
    case CLOSING:
        gate_stop(motor, false);
        ESP_LOGI(GATE_CONTROL_LOG_TAG,"stopping M%i",motor->id);
        break;
    case STOPPED_OPENING:
        gate_close(motor);
        ESP_LOGI(GATE_CONTROL_LOG_TAG,"closing M%i",motor->id);
        break;
    case STOPPED_CLOSING:
        gate_open(motor);  
        ESP_LOGI(GATE_CONTROL_LOG_TAG,"opening M%i",motor->id);
        break;
    case UNKNOWN:
        gate_open(motor);  
        ESP_LOGI(GATE_CONTROL_LOG_TAG,"opening M%i",motor->id);
        break;    
    default:
        break;
    }
}



static void gate_ghota_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_id == GHOTA_EVENT_START_UPDATE) {
        ESP_LOGI(GATE_LOG_TAG, "Starting OTA update disabling motor controll");
        gate_stop(&m1, true);
        gate_stop(&m2, true);
        xSemaphoreTake(gate_action_task_mutex, portMAX_DELAY);
    } else if (event_id == GHOTA_EVENT_FINISH_UPDATE || event_id == GHOTA_EVENT_UPDATE_FAILED || event_id == GHOTA_EVENT_NOUPDATE_AVAILABLE) {
        ESP_LOGI(GATE_LOG_TAG, "Ending OTA update, enabling motor controll");
        xSemaphoreGive(gate_action_task_mutex);
    }
}



gate_state_t gate_get_state(motor_id_t id){
    return (M1 == id) ? atomic_load(&(m1.state)) : atomic_load(&(m2.state));
}

int16_t gate_get_open_pcnt(motor_id_t id){
    return (M1 == id) ? atomic_load(&(m1.open_pcnt)) : atomic_load(&(m2.open_pcnt));
}

int16_t gate_get_close_pcnt(motor_id_t id){
    return (M1 == id) ? atomic_load(&(m1.close_pcnt)) : atomic_load(&(m2.close_pcnt));
    
}


void gate_action_task(void *pvParameters) {
    static gate_command_t command;
    while (true) {
        if (xQueueReceive(gate_action_queue, &command, portMAX_DELAY) && xSemaphoreTake(gate_action_task_mutex, portMAX_DELAY)) {
            gate_t *current_motor = (M1M2 != command.id) ? ((M1 == command.id) ? &m1 : &m2) : NULL;
            switch (command.action) {
                case OPEN:
                    if (current_motor) {
                        gate_open(current_motor);
                        ESP_LOGI(GATE_CONTROL_LOG_TAG, "opening M%i", command.id);
                    } else {
                        gate_open(&m1);
                        gate_open(&m2);
                        ESP_LOGI(GATE_CONTROL_LOG_TAG, "opening M1 and M2");
                    }
                    break;

                case CLOSE:
                    if (current_motor) {
                        gate_close(current_motor);
                        ESP_LOGI(GATE_CONTROL_LOG_TAG, "closing M%i", command.id);
                    } else {
                        gate_close(&m1);
                        gate_close(&m2);
                        ESP_LOGI(GATE_CONTROL_LOG_TAG, "closing M1 and M2");
                    }
                    break;

                case STOP:
                    if (current_motor) {
                        gate_stop(current_motor, false);
                        ESP_LOGI(GATE_CONTROL_LOG_TAG, "stopping M%i", command.id);
                    } else {
                        gate_stop(&m1, false);
                        gate_stop(&m2, false);
                        ESP_LOGI(GATE_CONTROL_LOG_TAG, "stopping M1 and M2");
                    }
                    break;

                case NEXT_STATE:
                    if (current_motor) {
                        gate_next_state(current_motor);
                    } else {
                        /* M1 is leading in other to prevent action like M1 is closing and M2 opening assign same state */
                        m2.state = m1.state;
                        gate_next_state(&m1);
                        gate_next_state(&m2);
                    }
                    break;

                case HW_STOP:
                    if (current_motor) {
                        gate_stop(current_motor, true);
                        ESP_LOGI(GATE_CONTROL_LOG_TAG, "stopping M%i", command.id);
                    } else {
                        gate_stop(&m1, true);
                        gate_stop(&m2, true);
                        ESP_LOGI(GATE_CONTROL_LOG_TAG, "stopping M1 and M2");
                    }
                    break;

                default:
                    /* Should never reach but... */
                    if (current_motor) {
                        gate_close(current_motor);
                    } else {
                        gate_close(&m1);
                        gate_close(&m2);
                    }
                    break;
            }

            xSemaphoreGive(gate_action_task_mutex);
        }
        taskYIELD();
    }
}


void gate_task(void *pvParameters){
    motor_id_t id = (motor_id_t)pvParameters;
    /* OCP Protection variables*/
    int16_t ocp_count = 0;
    int16_t cfg_ocp_count = 0;
    int16_t cfg_ocp_treshold = 0;
    
    /* No current stop debounce */
    uint8_t no_current_stop = 0;

    ESP_LOGI(GATE_LOG_TAG,"M%i Motor task initialized", id);

    while(true){
        /* Task is only running when motor is truning */
        if(uxSemaphoreGetCount((M1 == id) ? gate_m1_mutex : gate_m2_mutex) == 0){
            ESP_LOGI(GATE_LOG_TAG,"M%i Motor task is suspending itself", id);
            vTaskSuspend(NULL);
            ESP_LOGI(GATE_LOG_TAG,"M%i Motor task is being resumed", id);

            cfg_ocp_count = (M1 == id) ? device_config.m1_ocp_count : device_config.m2_ocp_count;
            cfg_ocp_treshold = (M1 == id) ? device_config.m1_ocp_treshold : device_config.m2_ocp_treshold;
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
            ESP_LOGW(GATE_LOG_TAG,"M%i Motor task is stopping mottor", id);
            xQueueSendToFront(gate_action_queue, &GATE_CMD(HW_STOP, id), pdMS_TO_TICKS(10));
        }


        /* Stop motor if overcurrent event occured */
        if(0 != cfg_ocp_count && 0 != cfg_ocp_treshold && current > cfg_ocp_treshold){
            if(current > cfg_ocp_treshold * 1.8) ocp_count += 10;
            else if(current > cfg_ocp_treshold * 1.4) ocp_count += 8;
            else if(current > cfg_ocp_treshold * 1.2) ocp_count += 6;
            else ocp_count++;            
            ESP_LOGW(GATE_LOG_TAG,"M%i OCP Count increased: %i, current: %imA", id, ocp_count, current);       
        }
        if(ocp_count > cfg_ocp_count){
            ESP_LOGE(GATE_LOG_TAG,"M%i Motor task is stopping mottor due to overcurrent event", id);
            xQueueSendToFront(gate_action_queue, &GATE_CMD(STOP, id), pdMS_TO_TICKS(10));
            io_buzzer(5, 50, 50);
        }

        /* more stop/monitor condition to be defined */

        /* PCNT slow down*/
        // if(atomic_load(&(motor->open_pcnt_cal)) && atomic_load(&(motor->close_pcnt_cal))){
        //     int16_t open_pcnt = gate_get_motor_open_pcnt(id);
        //     int16_t close_pcnt = gate_get_motor_close_pcnt(id);
        //     int32_t total_distance = abs(open_pcnt) + abs(close_pcnt);

        //     gate_state_t current_state = gate_get_gate_state(id);
        //     if(current_state == CLOSING){
        //         int16_t slow_down_point = close_pcnt + total_distance * 0.20;
        //         int16_t current_pcnt = io_motor_get_pcnt(id);

        //     }
        // }
        vTaskDelay(pdMS_TO_TICKS(10));
    }  
}
