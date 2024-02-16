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
QueueHandle_t wing_action_queue = NULL;
extern TaskHandle_t right_wing_task_handle;
extern TaskHandle_t left_wing_task_handle;

/* Initial state is assumed to be closed */
static wing_t right_wing = {.id = RIGHT_WING, .state = CLOSED};
static wing_t left_wing = {.id = LEFT_WING, .state = CLOSED};

static SemaphoreHandle_t wing_right_wing_mutex = NULL;
static SemaphoreHandle_t wing_left_wing_mutex = NULL;
static SemaphoreHandle_t wing_action_task_mutex = NULL;

static void wing_open(wing_t *wing);
static void wing_close(wing_t *wing);
static void wing_stop(wing_t *wing, bool hw_stop);

static void wing_ghota_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_id == GHOTA_EVENT_START_UPDATE)
    {
        ESP_LOGI(GATE_LOG_TAG, "Starting OTA update disabling wing controll");
        wing_stop(&right_wing, true);
        wing_stop(&left_wing, true);
        xSemaphoreTake(wing_action_task_mutex, portMAX_DELAY);
    }
    else if (event_id == GHOTA_EVENT_FINISH_UPDATE || event_id == GHOTA_EVENT_UPDATE_FAILED || event_id == GHOTA_EVENT_NOUPDATE_AVAILABLE)
    {
        ESP_LOGI(GATE_LOG_TAG, "Ending OTA update, enabling wing controll");
        xSemaphoreGive(wing_action_task_mutex);
    }
}

void wing_module_init()
{
    wing_action_queue = xQueueCreate(5, sizeof(wing_command_t));
    wing_right_wing_mutex = xSemaphoreCreateBinary();
    wing_left_wing_mutex = xSemaphoreCreateBinary();
    wing_action_task_mutex = xSemaphoreCreateBinary();

    io_wing_stop(RIGHT_WING);
    io_wing_stop(LEFT_WING);

    atomic_init(&(right_wing.open_pcnt), 0);
    atomic_init(&(right_wing.close_pcnt), 0);
    atomic_init(&(right_wing.state), UNKNOWN);
    atomic_init(&(right_wing.open_pcnt_cal), false);
    atomic_init(&(right_wing.close_pcnt_cal), false);

    atomic_init(&(left_wing.open_pcnt), 0);
    atomic_init(&(left_wing.close_pcnt), 0);
    atomic_init(&(left_wing.state), UNKNOWN);
    atomic_init(&(left_wing.open_pcnt_cal), false);
    atomic_init(&(left_wing.close_pcnt_cal), false);

    ESP_ERROR_CHECK(esp_event_handler_instance_register(GHOTA_EVENTS, ESP_EVENT_ANY_ID, &wing_ghota_event_handler, NULL, NULL));

    xSemaphoreTake(wing_right_wing_mutex, 0);
    xSemaphoreTake(wing_left_wing_mutex, 0);
    xSemaphoreGive(wing_action_task_mutex);
}

static void wing_open(wing_t *wing)
{
    if (atomic_load(&(wing->state)) == OPENING)
        return;

    wing_id_t id = wing->id;
    ESP_LOGI(GATE_CONTROL_LOG_TAG, "opening M%i", id);
    atomic_store(&(wing->state), OPENING);

    /* Set semaphore to indicate that task can run and resume it */
    xSemaphoreGive((RIGHT_WING == id) ? wing_right_wing_mutex : wing_left_wing_mutex);
    vTaskResume((RIGHT_WING == id) ? right_wing_task_handle : left_wing_task_handle);

    io_wing_stop(id);
    vTaskDelay(pdMS_TO_TICKS(30));
    io_wing_dir(id, 0);
    vTaskDelay(pdMS_TO_TICKS(15));
    io_wing_fade(id, 1023, 1000);

    /* Post event of status change */
    wing_info_t wing_state = {
        .id = id,
        .state = wing_get_state(id),
    };
    esp_event_post(GATE_EVENTS, WING_STATUS_CHANGED, &wing_state, sizeof(wing_info_t), pdMS_TO_TICKS(100));
}

static void wing_close(wing_t *wing)
{
    if (atomic_load(&(wing->state)) == CLOSING)
        return;

    wing_id_t id = wing->id;
    ESP_LOGI(GATE_CONTROL_LOG_TAG, "closing M%i", id);
    atomic_store(&(wing->state), CLOSING);

    /* Set semaphore to indicate that task can run and resume it */
    xSemaphoreGive((RIGHT_WING == id) ? wing_right_wing_mutex : wing_left_wing_mutex);
    vTaskResume((RIGHT_WING == id) ? right_wing_task_handle : left_wing_task_handle);

    io_wing_stop(id);
    vTaskDelay(pdMS_TO_TICKS(30));
    io_wing_dir(id, 1);
    vTaskDelay(pdMS_TO_TICKS(15));
    io_wing_fade(id, 1023, 1000);

    /* Post event of status change */
    wing_info_t wing_state = {
        .id = id,
        .state = wing_get_state(id),
    };
    esp_event_post(GATE_EVENTS, WING_STATUS_CHANGED, &wing_state, sizeof(wing_info_t), pdMS_TO_TICKS(100));
}

static void wing_stop(wing_t *wing, bool hw_stop)
{
    wing_id_t id = wing->id;

    wing_state_t state = atomic_load(&(wing->state));
    /* If stop is issued by user/error etc. change state to stopped_.... */
    if (!hw_stop)
    {
        if (state == OPENING)
        {
            atomic_store(&(wing->state), STOPPED_OPENING);
        }
        else if (state == CLOSING)
        {
            atomic_store(&(wing->state), STOPPED_CLOSING);
        }
        /* If stop is issued by hardware e.g. hardware endstop or reaching certain pcnt change to closed/opened */
    }
    else
    {
        if (state == OPENING)
        {
            atomic_store(&(wing->state), OPENED);
            atomic_store(&(wing->open_pcnt), io_wing_get_pcnt(id));
            atomic_store(&(wing->open_pcnt_cal), true);
        }
        else if (state == CLOSING)
        {
            atomic_store(&(wing->state), CLOSED);
            atomic_store(&(wing->close_pcnt), io_wing_get_pcnt(id));
            atomic_store(&(wing->close_pcnt_cal), true);
        }
    }

    /* Set semaphore to stop wing task */
    xSemaphoreTake((RIGHT_WING == id) ? wing_right_wing_mutex : wing_left_wing_mutex, 0);
    io_wing_stop(id);

    /* Post event of status change */
    wing_info_t wing_state = {
        .id = id,
        .state = wing_get_state(id),
    };
    esp_event_post(GATE_EVENTS, WING_STATUS_CHANGED, &wing_state, sizeof(wing_info_t), pdMS_TO_TICKS(100));
}

static void wing_next_state(wing_t *wing)
{
    switch (atomic_load(&(wing->state)))
    {
    case OPENED:
        wing_close(wing);
        ESP_LOGI(GATE_CONTROL_LOG_TAG, "closing M%i", wing->id);
        break;
    case CLOSED:
        wing_open(wing);
        ESP_LOGI(GATE_CONTROL_LOG_TAG, "opening M%i", wing->id);
        break;
    case OPENING:
        wing_stop(wing, false);
        ESP_LOGI(GATE_CONTROL_LOG_TAG, "stopping M%i", wing->id);
        break;
    case CLOSING:
        wing_stop(wing, false);
        ESP_LOGI(GATE_CONTROL_LOG_TAG, "stopping M%i", wing->id);
        break;
    case STOPPED_OPENING:
        wing_close(wing);
        ESP_LOGI(GATE_CONTROL_LOG_TAG, "closing M%i", wing->id);
        break;
    case STOPPED_CLOSING:
        wing_open(wing);
        ESP_LOGI(GATE_CONTROL_LOG_TAG, "opening M%i", wing->id);
        break;
    case UNKNOWN:
        wing_open(wing);
        ESP_LOGI(GATE_CONTROL_LOG_TAG, "opening M%i", wing->id);
        break;
    default:
        break;
    }
}

wing_state_t wing_get_state(wing_id_t id)
{
    wing_state_t right_wing_state = atomic_load(&(right_wing.state));
    wing_state_t left_wing_state = atomic_load(&(left_wing.state));
    if (BOTH_WING != id)
        return (RIGHT_WING == id) ? right_wing_state : left_wing_state;
    else
    {
        if (right_wing_state == left_wing_state)
            return right_wing_state;
        else if (OPENING == right_wing_state || OPENING == left_wing_state)
            return OPENING;
        else if (CLOSING == right_wing_state || CLOSING == left_wing_state)
            return CLOSING;
        else if (OPENED == right_wing_state || OPENED == left_wing_state)
            return OPENED;
        else if (CLOSED == right_wing_state && CLOSED == left_wing_state)
            return CLOSED;
        else
            return OPENED;
    }
}

int16_t wing_get_open_pcnt(wing_id_t id)
{
    return (RIGHT_WING == id) ? atomic_load(&(right_wing.open_pcnt)) : atomic_load(&(left_wing.open_pcnt));
}

int16_t wing_get_close_pcnt(wing_id_t id)
{
    return (RIGHT_WING == id) ? atomic_load(&(right_wing.close_pcnt)) : atomic_load(&(left_wing.close_pcnt));
}

void wing_action_task(void *pvParameters)
{
    static wing_command_t command;
    while (true)
    {
        if (xQueueReceive(wing_action_queue, &command, portMAX_DELAY) && xSemaphoreTake(wing_action_task_mutex, portMAX_DELAY))
        {
            wing_t *current_wing = (BOTH_WING != command.id) ? ((RIGHT_WING == command.id) ? &right_wing : &left_wing) : NULL;
            switch (command.action)
            {
            case OPEN:
                if (current_wing)
                {
                    wing_open(current_wing);
                    ESP_LOGI(GATE_CONTROL_LOG_TAG, "opening M%i", command.id);
                }
                else
                {
                    wing_open(&right_wing);
                    wing_open(&left_wing);
                    ESP_LOGI(GATE_CONTROL_LOG_TAG, "opening RIGHT_WING and LEFT_WING");
                }
                break;

            case CLOSE:
                if (current_wing)
                {
                    wing_close(current_wing);
                    ESP_LOGI(GATE_CONTROL_LOG_TAG, "closing M%i", command.id);
                }
                else
                {
                    wing_close(&right_wing);
                    wing_close(&left_wing);
                    ESP_LOGI(GATE_CONTROL_LOG_TAG, "closing RIGHT_WING and LEFT_WING");
                }
                break;

            case STOP:
                if (current_wing)
                {
                    wing_stop(current_wing, false);
                    ESP_LOGI(GATE_CONTROL_LOG_TAG, "stopping M%i", command.id);
                }
                else
                {
                    wing_stop(&right_wing, false);
                    wing_stop(&left_wing, false);
                    ESP_LOGI(GATE_CONTROL_LOG_TAG, "stopping RIGHT_WING and LEFT_WING");
                }
                break;
            case NEXT_STATE:
                if (current_wing)
                {
                    wing_next_state(current_wing);
                }
                else
                {
                    /* RIGHT_WING is leading in other to prevent action like RIGHT_WING is closing and LEFT_WING opening assign same state */
                    left_wing.state = right_wing.state;
                    wing_next_state(&right_wing);
                    wing_next_state(&left_wing);
                }
                break;

            case HW_STOP:
                if (current_wing)
                {
                    wing_stop(current_wing, true);
                    ESP_LOGI(GATE_CONTROL_LOG_TAG, "stopping M%i", command.id);
                }
                else
                {
                    wing_stop(&right_wing, true);
                    wing_stop(&left_wing, true);
                    ESP_LOGI(GATE_CONTROL_LOG_TAG, "stopping RIGHT_WING and LEFT_WING");
                }
                break;
            }

            xSemaphoreGive(wing_action_task_mutex);
        }
        taskYIELD();
    }
}

void wing_task(void *pvParameters)
{
    wing_id_t id = (wing_id_t)pvParameters;
    /* OCP Protection variables*/
    int16_t ocp_count = 0;
    int16_t cfg_ocp_count = 0;
    int16_t cfg_ocp_threshold = 0;

    /* No current stop debounce */
    uint8_t no_current_stop = 0;

    ESP_LOGI(GATE_LOG_TAG, "M%i Wing task initialized", id);

    while (true)
    {
        /* Task is only running when wing is truning */
        if (uxSemaphoreGetCount((RIGHT_WING == id) ? wing_right_wing_mutex : wing_left_wing_mutex) == 0)
        {
            ESP_LOGI(GATE_LOG_TAG, "M%i Wing task is suspending itself", id);
            vTaskSuspend(NULL);
            ESP_LOGI(GATE_LOG_TAG, "M%i Wing task is being resumed", id);

            cfg_ocp_count = (RIGHT_WING == id) ? device_config.right_wing_ocp_count : device_config.left_wing_ocp_count;
            cfg_ocp_threshold = (RIGHT_WING == id) ? device_config.right_wing_ocp_threshold : device_config.left_wing_ocp_threshold;
            ocp_count = 0;
            vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for wing to ramp up
        }

        const uint16_t current = io_wing_get_current(id);

        /* Stop wing if current is 0(wing stopped by builtin endstops) */
        if (0 == current)
        {
            no_current_stop++;
        }
        if (no_current_stop > 30)
        {
            no_current_stop = 0;
            ESP_LOGW(GATE_LOG_TAG, "M%i Wing task is stopping mottor", id);
            xQueueSendToFront(wing_action_queue, &WING_CMD(HW_STOP, id), pdMS_TO_TICKS(10));
        }

        /* Stop wing if overcurrent event occured */
        if (0 != cfg_ocp_count && 0 != cfg_ocp_threshold && current > cfg_ocp_threshold)
        {
            if (current > cfg_ocp_threshold * 1.8)
                ocp_count += 10;
            else if (current > cfg_ocp_threshold * 1.4)
                ocp_count += 8;
            else if (current > cfg_ocp_threshold * 1.2)
                ocp_count += 6;
            else
                ocp_count++;
            ESP_LOGW(GATE_LOG_TAG, "M%i OCP Count increased: %i, current: %imA", id, ocp_count, current);
        }
        if (ocp_count > cfg_ocp_count)
        {
            ESP_LOGE(GATE_LOG_TAG, "M%i Wing task is stopping mottor due to overcurrent event", id);
            xQueueSendToFront(wing_action_queue, &WING_CMD(STOP, id), pdMS_TO_TICKS(10));
            io_buzzer(5, 50, 50);
        }

        /* more stop/monitor condition to be defined */

        /* PCNT slow down*/
        // if(atomic_load(&(wing->open_pcnt_cal)) && atomic_load(&(wing->close_pcnt_cal))){
        //     int16_t open_pcnt = wing_get_wing_open_pcnt(id);
        //     int16_t close_pcnt = wing_get_wing_close_pcnt(id);
        //     int32_t total_distance = abs(open_pcnt) + abs(close_pcnt);

        //     wing_state_t current_state = wing_get_wing_state(id);
        //     if(current_state == CLOSING){
        //         int16_t slow_down_point = close_pcnt + total_distance * 0.20;
        //         int16_t current_pcnt = io_wing_get_pcnt(id);

        //     }
        // }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
