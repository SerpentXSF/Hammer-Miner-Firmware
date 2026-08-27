#include "system.h"
#include "work_queue.h"
#include "serial.h"
#include "lt0051.h"
#include <string.h>
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "asic.h"
#include "lt0051.h"
#include "global_state.h"

static const char *TAG = "ASIC_task";

// static bm_job ** active_jobs; is required to keep track of the active jobs since the

void ASIC_task(void *pvParameters)
{
    uint32_t chain_num = ((AsicParam *)(pvParameters))->chain_num;
    GlobalState *GLOBAL_STATE = ((AsicParam *)(pvParameters))->p_global_state;
    uint8_t workid = 0;

    ESP_LOGI(TAG, "ASIC Job Interval: %.2f ms", GLOBAL_STATE->asic_job_frequency_ms);
    SYSTEM_notify_mining_started(GLOBAL_STATE);
    ESP_LOGI(TAG, "ASIC Ready!");

    while (1)
    {
        bm_job *next_bm_job = (bm_job *)queue_dequeue(&GLOBAL_STATE->ASIC_jobs_queue[chain_num]);

        ASIC_send_work(GLOBAL_STATE, next_bm_job, chain_num, workid);
        workid = (workid + 1) % 128;

        // Time to execute the above code is ~0.3ms
        // Delay for ASIC(s) to finish the job
        //vTaskDelay((GLOBAL_STATE->asic_job_frequency_ms - 0.3) / portTICK_PERIOD_MS);
        xSemaphoreTake(GLOBAL_STATE->ASIC_TASK_MODULE[chain_num].semaphore, 
            (GLOBAL_STATE->asic_job_frequency_ms / portTICK_PERIOD_MS));
    }
}
