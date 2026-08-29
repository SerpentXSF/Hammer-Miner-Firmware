#include <lwip/tcpip.h>

#include "system.h"
#include "work_queue.h"
#include "pool_scheduler.h"
#include "serial.h"
#include "lt0051.h"
#include <string.h>
#include <pthread.h>
#include "esp_log.h"
#include "nvs_config.h"
#include "utils.h"
#include "stratum_task.h"
#include "hashrate_monitor_task.h"
#include "asic.h"

static const char *TAG = "asic_result";

void ASIC_result_task(void *pvParameters)
{ 
    uint32_t chain_num = ((AsicParam *)(pvParameters))->chain_num;
    GlobalState *GLOBAL_STATE = ((AsicParam *)(pvParameters))->p_global_state;
    ESP_LOGI(TAG, "ASIC_result_task initialising.");

    while (1)
    {
        //task_result *asic_result = (*GLOBAL_STATE->ASIC_functions.receive_result_fn)(GLOBAL_STATE);
        task_result *asic_result = ASIC_process_work(GLOBAL_STATE, chain_num);

        if (asic_result == NULL)
        {
            continue;
        }

        if (asic_result->register_type != REGISTER_INVALID) {
            hashrate_monitor_register_read(GLOBAL_STATE, asic_result->register_type, asic_result->asic_nr, asic_result->value);
            continue;
        }

        uint8_t job_id = asic_result->job_id;

        if (GLOBAL_STATE->valid_jobs[chain_num][job_id] == 0)
        {
            ESP_LOGW(TAG, "Invalid job nonce found, 0x%02X", job_id);
            continue;
        }

        // check the nonce difficulty
        double nonce_diff = test_nonce_value(
            GLOBAL_STATE->ASIC_TASK_MODULE[chain_num].active_jobs[job_id],
            asic_result->nonce,
            asic_result->rolled_version);

        //log the ASIC response
        /* Job id first: with two pools running it is what ties a nonce back to
         * the pool that issued the work it was found against. */
        ESP_LOGD(TAG, "ID: %s, ASIC nr: %d, Core: %d/%d, ver: %08" PRIX32 " Nonce %08" PRIX32 " diff %.1f of %ld.",
            GLOBAL_STATE->ASIC_TASK_MODULE[chain_num].active_jobs[job_id]->jobid,
            asic_result->asic_nr, asic_result->core_id, asic_result->small_core_id,
            asic_result->rolled_version, asic_result->nonce, nonce_diff,
            GLOBAL_STATE->ASIC_TASK_MODULE[chain_num].active_jobs[job_id]->pool_diff);

        if (nonce_diff >= GLOBAL_STATE->ASIC_TASK_MODULE[chain_num].active_jobs[job_id]->pool_diff)
        {
            bm_job * job = GLOBAL_STATE->ASIC_TASK_MODULE[chain_num].active_jobs[job_id];

            /*
             * A share belongs to the pool that issued its job. Sending pool B's
             * work to pool A gets it rejected as an unknown job id, and would
             * also credit the wrong pool. The job carries pool_id for exactly
             * this reason.
             */
            if (GLOBAL_STATE->dual_enable && job->pool_id == POOL_B) {
                /* Hold transportB_lock across the write: the pool B task can be
                 * tearing this handle down at any moment. */
                pthread_mutex_lock(&GLOBAL_STATE->transportB_lock);
                if (GLOBAL_STATE->transportB != NULL) {
                    int retB = STRATUM_V1_submit_share(
                        GLOBAL_STATE->transportB,
                        GLOBAL_STATE->send_uidB++,
                        GLOBAL_STATE->SYSTEM_MODULE.poolB_user,
                        job->jobid,
                        job->extranonce2,
                        job->ntime,
                        asic_result->nonce,
                        asic_result->rolled_version ^ job->version);
                    if (retB < 0) {
                        ESP_LOGW(TAG, "Unable to write share to pool B. Ret: %d", retB);
                    }
                } else {
                    ESP_LOGD(TAG, "pool B share dropped: not connected");
                }
                pthread_mutex_unlock(&GLOBAL_STATE->transportB_lock);
            } else {
                char * user = GLOBAL_STATE->SYSTEM_MODULE.is_using_fallback ? GLOBAL_STATE->SYSTEM_MODULE.fallback_pool_user : GLOBAL_STATE->SYSTEM_MODULE.pool_user;
                /*TODO: submit a share without rolled version.*/
                int ret = STRATUM_V1_submit_share(
                    GLOBAL_STATE->transport,
                    GLOBAL_STATE->send_uid++,
                    user,
                    job->jobid,
                    job->extranonce2,
                    job->ntime,
                    asic_result->nonce,
                    asic_result->rolled_version ^ job->version);

                if (ret < 0) {
                    ESP_LOGI(TAG, "Unable to write share to transport. Closing connection. Ret: %d", ret);
                    stratum_close_connection(GLOBAL_STATE);
                }
            }
        }

        //SYSTEM_notify_found_nonce(GLOBAL_STATE, nonce_diff, job_id, chain_num, asic_result->chip_id, asic_result->core_id);
        if(nonce_diff < 1){
            SYSTEM_notify_hw(GLOBAL_STATE, chain_num, asic_result->chip_id, asic_result->core_id);
        }else{
            SYSTEM_notify_found_nonce(GLOBAL_STATE, nonce_diff, job_id, chain_num, asic_result->chip_id, asic_result->core_id);           
        }
    }
}

void ASIC_ltc_result_task(void *pvParameters)
{ 
    uint32_t chain_num = ((AsicParam *)(pvParameters))->chain_num;
    GlobalState *GLOBAL_STATE = ((AsicParam *)(pvParameters))->p_global_state;
    double nonce_diff = 0.0;

    ESP_LOGI(TAG, "ASIC_result_task initialising.");

    while (1)
    {
        //task_result *asic_result = (*GLOBAL_STATE->ASIC_functions.receive_result_fn)(GLOBAL_STATE);
        task_result *asic_result = ASIC_process_work(GLOBAL_STATE, chain_num);

        if (asic_result == NULL)
        {
            continue;
        }

        uint8_t job_id = asic_result->job_id;

        if (GLOBAL_STATE->valid_jobs[chain_num][job_id] == 0)
        {
            ESP_LOGW(TAG, "Invalid job nonce found, 0x%02X", job_id);
            
            #ifdef STATISTIC_SYSTEM_FEATURE            
            statistic_notice_hw(&(GLOBAL_STATE->STATISTIC_MODULE), 
                chain_num, asic_result->chip_id, asic_result->core_id, ASIC_get_small_core_count(GLOBAL_STATE));
            #endif
            continue;
        }

        // check the nonce difficulty
        nonce_diff = test_ltc_nonce_value(
            GLOBAL_STATE->ASIC_TASK_MODULE[chain_num].active_jobs[job_id],
            asic_result->nonce, asic_result->chip_id, asic_result->core_id, chain_num, job_id);

        //log the ASIC response
        ESP_LOGD(TAG, "Nonce %08" PRIX32 " diff %.1f of %ld.", asic_result->nonce, nonce_diff, 
            GLOBAL_STATE->ASIC_TASK_MODULE[chain_num].active_jobs[job_id]->pool_diff);

        if (nonce_diff >= GLOBAL_STATE->ASIC_TASK_MODULE[chain_num].active_jobs[job_id]->pool_diff)
        {
            char * user = GLOBAL_STATE->SYSTEM_MODULE.is_using_fallback ? \
                GLOBAL_STATE->SYSTEM_MODULE.fallback_pool_user : GLOBAL_STATE->SYSTEM_MODULE.pool_user;

            /*submit a share without rolled version.*/
            int ret = STRATUM_V1_submit_ltc_share(
                GLOBAL_STATE->transport,
                GLOBAL_STATE->send_uid++,
                user,
                GLOBAL_STATE->ASIC_TASK_MODULE[chain_num].active_jobs[job_id]->jobid,
                GLOBAL_STATE->ASIC_TASK_MODULE[chain_num].active_jobs[job_id]->extranonce2,
                GLOBAL_STATE->ASIC_TASK_MODULE[chain_num].active_jobs[job_id]->ntime,
                asic_result->nonce
            );
            #ifdef STATISTIC_SYSTEM_FEATURE 
            SystemModule * module = &GLOBAL_STATE->SYSTEM_MODULE;
            uint8_t pool_id = 0;
            if(module->is_using_fallback){
                pool_id = 1;
            }
            statistic_notice_submit_share(&(GLOBAL_STATE->STATISTIC_MODULE), pool_id, nonce_diff);    
            #endif

            if (ret < 0) {
                ESP_LOGI(TAG, "Unable to write share to transport. Closing connection. Ret: %d", ret);
                stratum_close_connection(GLOBAL_STATE);
            }
        }
        
        if(nonce_diff < 1){
            SYSTEM_notify_hw(GLOBAL_STATE, chain_num, asic_result->chip_id, asic_result->core_id);
            #ifdef STATISTIC_SYSTEM_FEATURE            
            statistic_notice_hw(&(GLOBAL_STATE->STATISTIC_MODULE), 
                chain_num, asic_result->chip_id, asic_result->core_id, ASIC_get_small_core_count(GLOBAL_STATE));
            #endif
        }else{
            SYSTEM_notify_found_nonce(GLOBAL_STATE, nonce_diff, job_id, chain_num, asic_result->chip_id, asic_result->core_id);
            #ifdef STATISTIC_SYSTEM_FEATURE
            statistic_notice_nonce(&(GLOBAL_STATE->STATISTIC_MODULE), 
                chain_num, asic_result->chip_id, asic_result->core_id, ASIC_get_small_core_count(GLOBAL_STATE));
            #endif            
        }
    }
}

