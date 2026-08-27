#include <sys/time.h>
#include <limits.h>

#include "work_queue.h"
#include "global_state.h"
#include "esp_log.h"
#include "esp_system.h"
#include "mining.h"
#include "string.h"

#include "asic.h"

static const char *TAG = "create_jobs_task";

#define QUEUE_LOW_WATER_MARK 10 // Adjust based on your requirements

static bool should_generate_more_work(GlobalState *GLOBAL_STATE, uint32_t chain_num);
static void generate_work(GlobalState *GLOBAL_STATE, mining_notify *notification, uint64_t extranonce_2, uint32_t difficulty, uint32_t chain_num);

void create_jobs_task(void *pvParameters)
{
    GlobalState *GLOBAL_STATE = (GlobalState *)pvParameters;
    uint32_t difficulty = GLOBAL_STATE->stratum_difficulty;

    ESP_LOGI(TAG, "create_jobs_task started");

    uint64_t extranonce_2 = 0;

    while (1)
    {
        mining_notify *mining_notification = (mining_notify *)queue_dequeue(&GLOBAL_STATE->stratum_queue);
        if (mining_notification == NULL) {
            ESP_LOGE(TAG, "Failed to dequeue mining notification");
            vTaskDelay(100 / portTICK_PERIOD_MS); // Wait a bit before trying again
            continue;
        }

        ESP_LOGD(TAG, "New Work Dequeued %s", mining_notification->job_id);

        if (GLOBAL_STATE->new_set_mining_difficulty_msg)
        {
            ESP_LOGI(TAG, "New job difficulty %lu", GLOBAL_STATE->stratum_difficulty);
            difficulty = GLOBAL_STATE->stratum_difficulty;
            GLOBAL_STATE->new_set_mining_difficulty_msg = false;
        }

        if (GLOBAL_STATE->new_stratum_version_rolling_msg) {
            ESP_LOGI(TAG, "Set chip version rolls %i", (int)(GLOBAL_STATE->version_mask >> 13));
            ASIC_set_version_mask(GLOBAL_STATE, GLOBAL_STATE->version_mask);
            GLOBAL_STATE->new_stratum_version_rolling_msg = false;
        }

        while (GLOBAL_STATE->stratum_queue.count < 1 && GLOBAL_STATE->abandon_work == 0)
        {
            for(uint32_t chain_num = 0; chain_num < MAX_CHAIN_NUM; chain_num++)
            {
                if(GLOBAL_STATE->chain_pluged[chain_num])
                {
                    if (should_generate_more_work(GLOBAL_STATE, chain_num))
                    {
                        generate_work(GLOBAL_STATE, mining_notification, extranonce_2, difficulty, chain_num);

                        // Increase extranonce_2 for the next job.
                        extranonce_2++;
                    }
                    else
                    {
                        // If no more work needed, wait a bit before checking again.
                        vTaskDelay(100 / portTICK_PERIOD_MS);
                    }
                }
            }
        }

        if (GLOBAL_STATE->abandon_work == 1)
        {
            GLOBAL_STATE->abandon_work = 0;
            for(uint32_t chain_num = 0; chain_num < MAX_CHAIN_NUM; chain_num++)
            {
                if(GLOBAL_STATE->chain_pluged[chain_num])
                {    
                    ESP_LOGD(TAG, "abandon work. %"PRIu32"", chain_num);
                    ASIC_jobs_queue_clear(&GLOBAL_STATE->ASIC_jobs_queue[chain_num]);
                    xSemaphoreGive(GLOBAL_STATE->ASIC_TASK_MODULE[chain_num].semaphore);
                }
            }
        }

        STRATUM_V1_free_mining_notify(mining_notification);
    }
}

static bool should_generate_more_work(GlobalState *GLOBAL_STATE, uint32_t chain_num)
{
    return GLOBAL_STATE->ASIC_jobs_queue[chain_num].count < QUEUE_LOW_WATER_MARK;
}

static void generate_work(GlobalState *GLOBAL_STATE, mining_notify *notification, uint64_t extranonce_2, uint32_t difficulty, uint32_t chain_num)
{
    int extranonce_2_len = 0;
    char extranonce_str[40] = {"\0"};

    if(xSemaphoreTake(GLOBAL_STATE->global_parameter_mutex, pdMS_TO_TICKS(2000))){
        extranonce_2_len = GLOBAL_STATE->extranonce_2_len;
        snprintf(extranonce_str, sizeof(extranonce_str), "%s", GLOBAL_STATE->extranonce_str);
        xSemaphoreGive(GLOBAL_STATE->global_parameter_mutex);
    }else{
        ESP_LOGE(TAG, "Failed to get the global_parameter_mutex.");
        goto out;
    }

    char *extranonce_2_str = extranonce_2_generate(extranonce_2, extranonce_2_len);
    if (extranonce_2_str == NULL) {
        ESP_LOGE(TAG, "Failed to generate extranonce_2");
        return;
    }

    char *coinbase_tx = construct_coinbase_tx(notification->coinbase_1, notification->coinbase_2, extranonce_str, extranonce_2_str);
    if (coinbase_tx == NULL) {
        ESP_LOGE(TAG, "Failed to construct coinbase_tx");
        free(extranonce_2_str);
        return;
    }

    char *merkle_root = calculate_merkle_root_hash(coinbase_tx, (uint8_t(*)[32])notification->merkle_branches, notification->n_merkle_branches);
    if (merkle_root == NULL) {
        ESP_LOGE(TAG, "Failed to calculate merkle_root");
        free(extranonce_2_str);
        free(coinbase_tx);
        return;
    }

#if 1
    bm_job next_job = construct_bm_job(notification, merkle_root, GLOBAL_STATE->version_mask, difficulty);
#else
    bm_job next_job = construct_ltc_job(notification, merkle_root);
#endif

    bm_job *queued_next_job = malloc(sizeof(bm_job));
    if (queued_next_job == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for queued_next_job");
        free(extranonce_2_str);
        free(coinbase_tx);
        free(merkle_root);
        return;
    }

    memcpy(queued_next_job, &next_job, sizeof(bm_job));
    queued_next_job->extranonce2 = extranonce_2_str; // Transfer ownership
    queued_next_job->jobid = strdup(notification->job_id);
    queued_next_job->version_mask = GLOBAL_STATE->version_mask;
    queue_enqueue(&GLOBAL_STATE->ASIC_jobs_queue[chain_num], queued_next_job);

    free(coinbase_tx);
    free(merkle_root);

out:
    return;
}