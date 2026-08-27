#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

#include "statistic.h"

static const char* TAG = "statistic";

static void statistic_init_pool(PoolStatistic *pool_statistic)
{
    if(NULL == pool_statistic){
        ESP_LOGE(TAG, "%s error input parameter %p.", __func__, pool_statistic);
        return;
    }
    PoolStatistic *p = pool_statistic;

    p->pool_mutex = xSemaphoreCreateMutex();
    p->get_works = p->get_failures = p->remote_failures = p->diff1_shares = p->share_accpetd = \
    p->share_discarded = p->share_stale = 0;
    p->diff_accepted = p->diff_rejected = p->diff_stale = p->diff_discarded = p->last_share_diff = p->diff = 0.0;

    return;
}

static void statistic_init_summary(SummaryStatistic *summary_statistic, uint32_t nonce_hash)
{
    if(NULL == summary_statistic){
        ESP_LOGE(TAG, "%s error input parameter %p.", __func__, summary_statistic);
        return;
    }

    SummaryStatistic *s = summary_statistic;
    s->summary_mutex = xSemaphoreCreateMutex();
    s->last_share_diff = s->diff1_shares = s->diff_rejected = s->diff_stale = s->diff_discarded = s->hashrate_rt = 0.0;
    s->nonce_hash = nonce_hash;
    s->average_hash_start_time = 0;

    s->nonce = s->hw = s->jobs = s->total_hash = s->rt_hash = 0;
    s->get_works = s->share_accpetd = s->share_rejected = s->share_discarded = s->share_stale = 0;

    return;
}

static void statistic_init_chain(ChainStatistic *chain_statistic, uint32_t nonce_hash)
{
    if(NULL == chain_statistic){
        ESP_LOGE(TAG, "%s error input parameter %p.", __func__, chain_statistic);
        return;
    }

    ChainStatistic *c = chain_statistic;
    c->chain_mutex = xSemaphoreCreateMutex();
    c->nonce = c->hw = c->jobs = 0;
    c->total_hash = c->rt_hash = UINT64_C(0);
    c->nonce_hash = nonce_hash;

    c->hashrate_rt = c->hashrate_avg = c->hashrate_ideal = 0.0;
    memset(c->nonce_counter, 0, MAX_ASICS_OF_CHAIN);
    memset(c->chip_rt_hash, 0, sizeof(c->chip_rt_hash));
    for (int i = 0; i < MAX_ASICS_OF_CHAIN; i++) {
        c->chip_hashrate_rt[i] = 0.0;
    }
    return;
}

esp_err_t statistic_init_system(StatisticModule *statistic_module, uint32_t nonce_hash)
{
    esp_err_t ret = ESP_FAIL;

    if(NULL == statistic_module){
        ESP_LOGE(TAG, "%s error input parameter %p.", __func__, statistic_module);
        goto exit;
    }

    statistic_init_summary(&(statistic_module->summary_statistic), nonce_hash);
    for(int i = 0; i < MAX_POOL_NUM; i++){
        statistic_init_pool(statistic_module->pool_statistic + i);
    }
    for(int i = 0; i < MAX_CHAIN_NUM; i++){
        statistic_init_chain(statistic_module->chain_statistic + i, nonce_hash);
    }

    ret = ESP_OK;
exit:
    return ret;
}

/*############### Pool notification. ###############*/
esp_err_t statistic_pool_set_diff(StatisticModule *statistic_module, uint8_t pool_id, double diff)
{
    esp_err_t ret = ESP_FAIL;
    PoolStatistic *pool_module = (statistic_module->pool_statistic + pool_id);
    SummaryStatistic *summary_module = &(statistic_module->summary_statistic);

    if(xSemaphoreTake(pool_module->pool_mutex, pdMS_TO_TICKS(2000))){
        pool_module->diff = diff;
        ret = ESP_OK;
        xSemaphoreGive(pool_module->pool_mutex);
    }else{
        ESP_LOGE(TAG, "Fail to get the pool_mutex.");
        goto exit;
    }

    if(xSemaphoreTake(summary_module->summary_mutex, pdMS_TO_TICKS(2000))){
        summary_module->diff = diff;
        xSemaphoreGive(summary_module->summary_mutex);
    }else{
        ESP_LOGE(TAG, "Fail to get the summary_mutex.");
        goto exit;
    }
exit:
    return ret;
}

esp_err_t statistic_notice_getwork(StatisticModule *statistic_module, uint8_t pool_id)
{
    esp_err_t ret = ESP_FAIL;
    SummaryStatistic *summary_module = &(statistic_module->summary_statistic);
    PoolStatistic *pool_module = (statistic_module->pool_statistic + pool_id);

    if(NULL == statistic_module){
        ESP_LOGE(TAG, "%s error input parameter %p.", __func__, statistic_module);
        goto exit;
    }

    if(xSemaphoreTake(summary_module->summary_mutex, pdMS_TO_TICKS(2000))){
        summary_module->get_works += 1;
        xSemaphoreGive(summary_module->summary_mutex);
    }else{
        ESP_LOGE(TAG, "Fail to get the summary_mutex.");
        goto exit;
    }

    if(xSemaphoreTake(pool_module->pool_mutex, pdMS_TO_TICKS(2000))){
        pool_module->get_works += 1;
        ret = ESP_OK;
        xSemaphoreGive(pool_module->pool_mutex);
    }else{
        ESP_LOGE(TAG, "Fail to get the pool_mutex.");
        goto exit;
    }
exit:
    return ret;
}

esp_err_t statistic_notice_submit_share(StatisticModule *statistic_module, uint8_t pool_id, double share_diff)
{
    esp_err_t ret = ESP_FAIL;
    SummaryStatistic *summary_module = &(statistic_module->summary_statistic);
    PoolStatistic *pool_module = (statistic_module->pool_statistic + pool_id);

    if(NULL == statistic_module){
        ESP_LOGE(TAG, "%s error input parameter %p.", __func__, statistic_module);
        goto exit;
    }

    if(xSemaphoreTake(summary_module->summary_mutex, pdMS_TO_TICKS(2000))){
        summary_module->diff1_shares += 1;
        xSemaphoreGive(summary_module->summary_mutex);
    }else{
        ESP_LOGE(TAG, "Fail to get the summary_mutex.");
        goto exit;
    }

    if(xSemaphoreTake(pool_module->pool_mutex, pdMS_TO_TICKS(2000))){
        pool_module->diff1_shares += 1;
        pool_module->last_share_diff = share_diff;

        ret = ESP_OK;
        xSemaphoreGive(pool_module->pool_mutex);
    }else{
        ESP_LOGE(TAG, "Fail to get the pool_mutex.");
        goto exit;
    }
    
exit:
    return ret;
}

esp_err_t statistic_notice_share_accept(StatisticModule *statistic_module, uint8_t pool_id)
{
    esp_err_t ret = ESP_FAIL;
    SummaryStatistic *summary_module = &(statistic_module->summary_statistic);
    PoolStatistic *pool_module = (statistic_module->pool_statistic + pool_id);

    if(NULL == statistic_module){
        ESP_LOGE(TAG, "%s error input parameter %p.", __func__, statistic_module);
        goto exit;
    }

    if(xSemaphoreTake(summary_module->summary_mutex, pdMS_TO_TICKS(2000))){
        summary_module->share_accpetd += 1;
        summary_module->diff_accepted += summary_module->diff;
        xSemaphoreGive(summary_module->summary_mutex);
    }else{
        ESP_LOGE(TAG, "Fail to get the summary_mutex.");
        goto exit;
    }

    if(xSemaphoreTake(pool_module->pool_mutex, pdMS_TO_TICKS(2000))){
        pool_module->share_accpetd += 1;
        pool_module->diff_accepted += pool_module->diff;
        ret = ESP_OK;
        xSemaphoreGive(pool_module->pool_mutex);
    }else{
        ESP_LOGE(TAG, "Fail to get the pool_mutex.");
        goto exit;
    }

exit:
    return ret;
}

esp_err_t statistic_notice_share_reject(StatisticModule *statistic_module, uint8_t pool_id)
{
    esp_err_t ret = ESP_FAIL;
    SummaryStatistic *summary_module = &(statistic_module->summary_statistic);
    PoolStatistic *pool_module = (statistic_module->pool_statistic + pool_id);

    if(NULL == statistic_module){
        ESP_LOGE(TAG, "%s error input parameter %p.", __func__, statistic_module);
        goto exit;
    }

    if(xSemaphoreTake(summary_module->summary_mutex, pdMS_TO_TICKS(2000))){
        summary_module->share_rejected += 1;
        summary_module->diff_rejected += summary_module->diff;
        xSemaphoreGive(summary_module->summary_mutex);
    }else{
        ESP_LOGE(TAG, "Fail to get the summary_mutex.");
        goto exit;
    }

    if(xSemaphoreTake(pool_module->pool_mutex, pdMS_TO_TICKS(2000))){
        pool_module->share_rejected += 1;
        pool_module->diff_rejected += pool_module->diff;
        ret = ESP_OK;
        xSemaphoreGive(pool_module->pool_mutex);
    }else{
        ESP_LOGE(TAG, "Fail to get the pool_mutex.");
        goto exit;
    }

exit:
    return ret;
}

esp_err_t statistic_notice_share_stale(StatisticModule *statistic_module, uint8_t pool_id)
{
    esp_err_t ret = ESP_FAIL;
    SummaryStatistic *summary_module = &(statistic_module->summary_statistic);
    PoolStatistic *pool_module = (statistic_module->pool_statistic + pool_id);

    if(NULL == statistic_module){
        ESP_LOGE(TAG, "%s error input parameter %p.", __func__, statistic_module);
        goto exit;
    }

    if(xSemaphoreTake(summary_module->summary_mutex, pdMS_TO_TICKS(2000))){
        summary_module->share_rejected += 1;
        summary_module->diff_rejected += summary_module->diff;
        xSemaphoreGive(summary_module->summary_mutex);
    }else{
        ESP_LOGE(TAG, "Fail to get the summary_mutex.");
        goto exit;
    }

    if(xSemaphoreTake(pool_module->pool_mutex, pdMS_TO_TICKS(2000))){
        pool_module->share_rejected += 1;
        pool_module->diff_rejected += summary_module->diff;
        ret = ESP_OK;
        xSemaphoreGive(pool_module->pool_mutex);
    }else{
        ESP_LOGE(TAG, "Fail to get the pool_mutex.");
        goto exit;
    }

exit:
    return ret;
}

esp_err_t statistic_notice_share_discard(StatisticModule *statistic_module, uint8_t pool_id)
{
    esp_err_t ret = ESP_FAIL;
    SummaryStatistic *summary_module = &(statistic_module->summary_statistic);
    PoolStatistic *pool_module = (statistic_module->pool_statistic + pool_id);

    if(NULL == statistic_module){
        ESP_LOGE(TAG, "%s error input parameter %p.", __func__, statistic_module);
        goto exit;
    }

    if(xSemaphoreTake(summary_module->summary_mutex, pdMS_TO_TICKS(2000))){
        summary_module->share_discarded += 1;
        summary_module->diff_discarded += summary_module->diff;
        xSemaphoreGive(summary_module->summary_mutex);
    }else{
        ESP_LOGE(TAG, "Fail to get the summary_mutex.");
        goto exit;
    }

    if(xSemaphoreTake(pool_module->pool_mutex, pdMS_TO_TICKS(2000))){
        pool_module->share_discarded += 1;
        pool_module->diff_discarded += pool_module->diff;
        ret = ESP_OK;
        xSemaphoreGive(pool_module->pool_mutex);
    }else{
        ESP_LOGE(TAG, "Fail to get the pool_mutex.");
        goto exit;
    }

exit:
    return ret;
}

/*############### chain notification ###############*/
esp_err_t statistic_notice_nonce(
    StatisticModule *statistic_module, uint8_t chain_index, uint8_t chip_id, uint8_t core_id, uint8_t asic_core_num
)
{
    esp_err_t ret = ESP_FAIL;
    SummaryStatistic *summary_module = &(statistic_module->summary_statistic);
    ChainStatistic  *chain_module = (statistic_module->chain_statistic + chain_index);

    if(NULL == statistic_module){
        ESP_LOGE(TAG, "%s error input parameter %p.", __func__, statistic_module);
        goto exit;
    }

    if(xSemaphoreTake(chain_module->chain_mutex, pdMS_TO_TICKS(2000))){
        if(core_id >= asic_core_num){
            ESP_LOGE(TAG, "Error core_id(%"PRIu8").", core_id);
            xSemaphoreGive(chain_module->chain_mutex);
            goto exit;
        }

        chain_module->nonce += 1;
        chain_module->total_hash += (uint64_t)chain_module->nonce_hash;
        chain_module->rt_hash += (uint64_t)chain_module->nonce_hash;
        if (chip_id < MAX_ASICS_OF_CHAIN) {
            chain_module->chip_rt_hash[chip_id] += (uint64_t)chain_module->nonce_hash;
        }
        xSemaphoreGive(chain_module->chain_mutex);
    }else{
        ESP_LOGE(TAG, "Fail to get the chain_mutex.");
        goto exit;
    }

    if(xSemaphoreTake(summary_module->summary_mutex, pdMS_TO_TICKS(2000))){
        summary_module->nonce += 1;
        summary_module->total_hash += (uint64_t)(summary_module->nonce_hash);
        summary_module->rt_hash += (uint64_t)(summary_module->nonce_hash);
        xSemaphoreGive(summary_module->summary_mutex);
        ret = ESP_OK;
    }else{
        ESP_LOGE(TAG, "Fail to get the summary_mutex.");
        goto exit;
    }
exit:
    return ret;
}

esp_err_t statistic_clear_rt_hash(StatisticModule *statistic_module, uint64_t rt_calc_interval)
{
    esp_err_t ret = ESP_FAIL;
    SummaryStatistic *summary_module = &(statistic_module->summary_statistic);
    ChainStatistic  *chain_module = statistic_module->chain_statistic;

    if(NULL == statistic_module){
        ESP_LOGE(TAG, "%s error input parameter %p.", __func__, statistic_module);
        goto exit;
    }

    if(xSemaphoreTake(summary_module->summary_mutex, pdMS_TO_TICKS(2000))){
        summary_module->hashrate_rt = (double)(summary_module->rt_hash/rt_calc_interval);
        summary_module->rt_hash = 0;
        xSemaphoreGive(summary_module->summary_mutex);
    }else{
        ESP_LOGE(TAG, "Fail to get the summary_mutex.");
        goto exit;
    }

    for(int i = 0; i < MAX_CHAIN_NUM; i++){
        ChainStatistic  *chain_module = statistic_module->chain_statistic + i;

        if(xSemaphoreTake(chain_module->chain_mutex, pdMS_TO_TICKS(2000))){
            chain_module->hashrate_rt = (double)(chain_module->rt_hash/rt_calc_interval);
            chain_module->rt_hash = 0;
            for (int j = 0; j < MAX_ASICS_OF_CHAIN; j++) {
                chain_module->chip_hashrate_rt[j] = (double)(chain_module->chip_rt_hash[j] / rt_calc_interval);
                chain_module->chip_rt_hash[j] = 0;
            }
            xSemaphoreGive(chain_module->chain_mutex);
            ret = ESP_OK;
        }else{
            ESP_LOGE(TAG, "Fail to get the chain_mutex 0.");
            goto exit;
        }
    }
exit:
    return ret;
}

esp_err_t statistic_clear_average_hash(StatisticModule *statistic_module)
{
    esp_err_t ret = ESP_FAIL;
    SummaryStatistic *summary_module = &(statistic_module->summary_statistic);

    if(NULL == statistic_module){
        ESP_LOGE(TAG, "%s error input parameter %p.", __func__, statistic_module);
        goto exit;
    }

    if(xSemaphoreTake(summary_module->summary_mutex, pdMS_TO_TICKS(2000))){
        summary_module->total_hash = 0;
        summary_module->average_hash_start_time = esp_timer_get_time()/1000000;
        xSemaphoreGive(summary_module->summary_mutex);
    }else{
        ESP_LOGE(TAG, "Fail to get the summary_mutex.");
        goto exit;
    }

exit:
    return ret;
}

esp_err_t statistic_notice_hw(StatisticModule *statistic_module, uint8_t chain_index, uint8_t chip_id, uint8_t core_id, uint8_t asic_core_num)
{
    esp_err_t ret = ESP_FAIL;
    SummaryStatistic *summary_module = &(statistic_module->summary_statistic);
    ChainStatistic  *chain_module = (statistic_module->chain_statistic + chain_index);

    if(NULL == statistic_module){
        ESP_LOGE(TAG, "%s error input parameter %p.", __func__, statistic_module);
        goto exit;
    }

    if(xSemaphoreTake(chain_module->chain_mutex, pdMS_TO_TICKS(2000))){
        if(core_id >= asic_core_num){
            xSemaphoreGive(chain_module->chain_mutex);
            ESP_LOGE(TAG, "Error core_id(%"PRIu8").", core_id);
            goto exit;
        }
        chain_module->hw += 1;
        xSemaphoreGive(chain_module->chain_mutex);
    }else{
        ESP_LOGE(TAG, "Fail to get the chain_mutex.");
        goto exit;
    }

    if(xSemaphoreTake(summary_module->summary_mutex, pdMS_TO_TICKS(2000))){
        summary_module->hw += 1;
        xSemaphoreGive(summary_module->summary_mutex);
        ret = ESP_OK;
    }else{
        ESP_LOGE(TAG, "Fail to get the summary_mutex.");
        goto exit;
    }

exit:
    return ret;
}

esp_err_t statistic_notice_job(StatisticModule *statistic_module, uint8_t chain_index)
{
    esp_err_t ret = ESP_FAIL;
    SummaryStatistic *summary_module = &(statistic_module->summary_statistic);
    ChainStatistic  *chain_module = (statistic_module->chain_statistic + chain_index);

    if(NULL == statistic_module){
        ESP_LOGE(TAG, "%s error input parameter %p.", __func__, statistic_module);
        goto exit;
    }

    if(xSemaphoreTake(summary_module->summary_mutex, pdMS_TO_TICKS(2000))){
        summary_module->jobs += 1;
        xSemaphoreGive(summary_module->summary_mutex);
    }else{
        ESP_LOGE(TAG, "Fail to get the summary_mutex.");
        goto exit;
    }

    if(xSemaphoreTake(chain_module->chain_mutex, pdMS_TO_TICKS(2000))){
        chain_module->jobs += 1;
        xSemaphoreGive(chain_module->chain_mutex);
        ret = ESP_OK;
    }else{
        ESP_LOGE(TAG, "Fail to get the chain_mutex.");
        goto exit;
    }

exit:
    return ret;
}

/*############### get summary ###############*/
esp_err_t statistic_get_nonce_info(StatisticModule *statistic_module, uint32_t *nonce, uint32_t *hw, uint32_t *jobs)
{
    esp_err_t ret = ESP_FAIL;
    SummaryStatistic *summary_module = &(statistic_module->summary_statistic);

    if(NULL == statistic_module){
        ESP_LOGE(TAG, "%s error input parameter %p.", __func__, statistic_module);
        goto exit;
    }

    if(xSemaphoreTake(summary_module->summary_mutex, pdMS_TO_TICKS(2000))){
        if(NULL != nonce){
            *nonce = summary_module->nonce;
        }
        if(NULL != hw){
            *hw = summary_module->hw;
        }
        if(NULL != jobs){
            *jobs = summary_module->jobs;
        }
        xSemaphoreGive(summary_module->summary_mutex);
        ret = ESP_OK;
    }else{
        ESP_LOGE(TAG, "Fail to get the summary_mutex.");
        goto exit;
    }

exit:
    return ret;
}

esp_err_t statistic_get_share_info(
    StatisticModule *statistic_module, uint32_t *diff1_share, 
    uint32_t *share_accepted, uint32_t *share_rejected, uint32_t *share_stale)
{
    esp_err_t ret = ESP_FAIL;
    SummaryStatistic *summary_module = &(statistic_module->summary_statistic);

    if(NULL == statistic_module){
        ESP_LOGE(TAG, "%s error input parameter %p.", __func__, statistic_module);
        goto exit;
    }

    if(xSemaphoreTake(summary_module->summary_mutex, pdMS_TO_TICKS(2000))){
        if(NULL != diff1_share){
            *diff1_share = summary_module->diff1_shares;
        }
        if(NULL != share_accepted){
            *share_accepted = summary_module->share_accpetd;
        }
        if(NULL != share_rejected){
            *share_rejected = summary_module->share_rejected;
        }
        if(NULL != share_stale){
            *share_stale = summary_module->share_stale;
        }
        xSemaphoreGive(summary_module->summary_mutex);
        ret = ESP_OK;
    }else{
        ESP_LOGE(TAG, "Fail to get the summary_mutex.");
        goto exit;
    }

exit:
    return ret;
}

esp_err_t statistic_get_diff_info(StatisticModule *statistic_module, double *diff_accepted, double *diff_rejected, double *diff_stale)
{
    esp_err_t ret = ESP_FAIL;
    SummaryStatistic *summary_module = &(statistic_module->summary_statistic);

    if(NULL == statistic_module){
        ESP_LOGE(TAG, "%s error input parameter %p.", __func__, statistic_module);
        goto exit;
    }

    if(xSemaphoreTake(summary_module->summary_mutex, pdMS_TO_TICKS(2000))){
        if(NULL != diff_accepted){
            *diff_accepted = summary_module->diff_accepted;
        }
        if(NULL != diff_rejected){
            *diff_rejected = summary_module->diff_rejected;
        }
        if(NULL != diff_stale){
            *diff_stale = summary_module->diff_stale;
        }
        xSemaphoreGive(summary_module->summary_mutex);
        ret = ESP_OK;
    }else{
        ESP_LOGE(TAG, "Fail to get the summary_mutex.");
        goto exit;
    }
exit:
    return ret;
}

esp_err_t statistic_get_hashrate(StatisticModule *statistic_module, double *rt_hashrate, double *average_hashrate, uint32_t up_time)
{
    esp_err_t ret = ESP_FAIL;
    SummaryStatistic *summary_module = &(statistic_module->summary_statistic);

    if(NULL == statistic_module){
        ESP_LOGE(TAG, "%s error input parameter %p.", __func__, statistic_module);
        goto exit;
    }

    if(xSemaphoreTake(summary_module->summary_mutex, pdMS_TO_TICKS(2000))){
        if(NULL != rt_hashrate){
            *rt_hashrate = summary_module->hashrate_rt;
        }
        if(NULL != average_hashrate){
            *average_hashrate = (double)(summary_module->total_hash/(up_time - summary_module->average_hash_start_time));
        }
        xSemaphoreGive(summary_module->summary_mutex);
        ret = ESP_OK;
    }else{
        ESP_LOGE(TAG, "Fail to get the summary_mutex.");
        goto exit;
    }
exit:
    return ret;
}

esp_err_t statistic_get_chain_info(
    StatisticModule *statistic_module, uint8_t chain_id, uint32_t *nonce, uint32_t *hw, uint32_t *jobs, double *chain_rt_hashrate
){
    esp_err_t ret = ESP_FAIL;
    ChainStatistic *chain_module = NULL;

    if(NULL == statistic_module){
        ESP_LOGE(TAG, "%s error input parameter %p.", __func__, statistic_module);
        goto exit;
    }

    if(chain_id >= MAX_CHAIN_NUM){
        ESP_LOGW(TAG, "%s Error chain_id %"PRIu8"", __func__, chain_id);
        goto exit;
    }else{
        chain_module = statistic_module->chain_statistic + chain_id;
    }

    if(xSemaphoreTake(chain_module->chain_mutex, pdMS_TO_TICKS(2000))){
        if(NULL != nonce){
            *nonce = chain_module->nonce;
        }
        if(NULL != hw){
            *hw = chain_module->hw;
        }
        if(NULL != jobs){
            *jobs = chain_module->jobs;
        }
        if(NULL != chain_rt_hashrate){
            *chain_rt_hashrate = chain_module->hashrate_rt;
        }

        xSemaphoreGive(chain_module->chain_mutex);
        ret = ESP_OK;
    }else{
        ESP_LOGE(TAG, "%s: fail to get the summary_mutex.", __func__);
        goto exit;
    }
exit:
    return ret;
}

esp_err_t statistic_get_pool_info(
    StatisticModule *statistic_module, uint8_t pool_id, 
    uint32_t *get_works, uint32_t *get_failures, float *diff, uint32_t *diff1_share,
    uint32_t *share_accepted, uint32_t *share_rejected, uint32_t *share_discarded, uint32_t *share_stale,
    double *diff_accepted, double *diff_rejected, double *diff_stale, double *diff_discarded, double *last_share_diff
){
    esp_err_t ret = ESP_FAIL;
    PoolStatistic *pool_module = NULL;

    if(NULL == statistic_module){
        ESP_LOGE(TAG, "%s error input parameter %p.", __func__, statistic_module);
        goto exit;
    }

    if(pool_id >= MAX_POOL_NUM){
        ESP_LOGW(TAG, "%s Error pool_id %"PRIu8"", __func__, pool_id);
        goto exit;
    }else{
        pool_module = statistic_module->pool_statistic + pool_id;
    }

    if(xSemaphoreTake(pool_module->pool_mutex, pdMS_TO_TICKS(2000))){
        if(NULL != get_works){
            *get_works = pool_module->get_works;
        }
        if(NULL != get_failures){
            *get_failures = pool_module->get_failures;
        }
        if(NULL != diff){
            *diff = pool_module->diff;
        }
        if(NULL != diff1_share){
            *diff1_share = pool_module->diff1_shares;
        }

        if(NULL != share_accepted){
            *share_accepted = pool_module->share_accpetd;
        }
        if(NULL != share_rejected){
            *share_rejected = pool_module->share_rejected;
        }
        if(NULL != share_discarded){
            *share_discarded = pool_module->share_discarded;
        }
        if(NULL != share_stale){
            *share_stale = pool_module->share_stale;
        }

        if(NULL != diff_accepted){
            *diff_accepted = pool_module->diff_accepted;
        }
        if(NULL != diff_rejected){
            *diff_rejected = pool_module->diff_rejected;
        }
        if(NULL != diff_stale){
            *diff_stale = pool_module->diff_stale;
        }
        if(NULL != diff_discarded){
            *diff_discarded = pool_module->diff_discarded;
        }
        if(NULL != last_share_diff){
            *last_share_diff = pool_module->last_share_diff;
        }
        xSemaphoreGive(pool_module->pool_mutex);
        ret = ESP_OK;
    }else{
        ESP_LOGE(TAG, "%s: Fail to get the pool_module.", __func__);
        goto exit;
    }
exit:
    return ret;

}

esp_err_t statistic_get_chip_hashrate(
    StatisticModule *statistic_module, uint8_t chain_id, uint8_t chip_id, double *chip_rt_hashrate
)
{
    esp_err_t ret = ESP_FAIL;
    ChainStatistic *chain_module = NULL;

    if(NULL == statistic_module){
        ESP_LOGE(TAG, "%s error input parameter %p.", __func__, statistic_module);
        goto exit;
    }

    if(chain_id >= MAX_CHAIN_NUM || chip_id >= MAX_ASICS_OF_CHAIN){
        ESP_LOGW(TAG, "%s Error chain_id %"PRIu8" or chip_id %"PRIu8"", __func__, chain_id, chip_id);
        goto exit;
    }else{
        chain_module = statistic_module->chain_statistic + chain_id;
    }

    if(xSemaphoreTake(chain_module->chain_mutex, pdMS_TO_TICKS(2000))){
        if(NULL != chip_rt_hashrate){
            *chip_rt_hashrate = chain_module->chip_hashrate_rt[chip_id];
        }
        xSemaphoreGive(chain_module->chain_mutex);
        ret = ESP_OK;
    }else{
        ESP_LOGE(TAG, "%s: fail to get the chain_mutex.", __func__);
        goto exit;
    }
exit:
    return ret;
}