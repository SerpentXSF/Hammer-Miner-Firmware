#include <lwip/tcpip.h>

#include "system.h"
#include "work_queue.h"
#include "serial.h"
#include "lt0051.h"
#include <string.h>
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
        ESP_LOGD(TAG, "Ver: %08" PRIX32 " Nonce %08" PRIX32 " diff %.1f of %ld.", 
            asic_result->rolled_version, asic_result->nonce, nonce_diff, 
            GLOBAL_STATE->ASIC_TASK_MODULE[chain_num].active_jobs[job_id]->pool_diff);

        if (nonce_diff >= GLOBAL_STATE->ASIC_TASK_MODULE[chain_num].active_jobs[job_id]->pool_diff)
        {
            char * user = GLOBAL_STATE->SYSTEM_MODULE.is_using_fallback ? GLOBAL_STATE->SYSTEM_MODULE.fallback_pool_user : GLOBAL_STATE->SYSTEM_MODULE.pool_user;
            /*TODO: submit a share without rolled version.*/
            int ret = STRATUM_V1_submit_share(
                GLOBAL_STATE->transport,
                GLOBAL_STATE->send_uid++,
                user,
                GLOBAL_STATE->ASIC_TASK_MODULE[chain_num].active_jobs[job_id]->jobid,
                GLOBAL_STATE->ASIC_TASK_MODULE[chain_num].active_jobs[job_id]->extranonce2,
                GLOBAL_STATE->ASIC_TASK_MODULE[chain_num].active_jobs[job_id]->ntime,
                asic_result->nonce,
                asic_result->rolled_version ^ GLOBAL_STATE->ASIC_TASK_MODULE[chain_num].active_jobs[job_id]->version);

            if (ret < 0) {
                ESP_LOGI(TAG, "Unable to write share to transport. Closing connection. Ret: %d", ret);
                stratum_close_connection(GLOBAL_STATE);
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

