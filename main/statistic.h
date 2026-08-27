#ifndef _STATISTIC_H
#define _STATISTIC_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "miner.h"
/*
diff1 = 2**16 = 64 KiB (hash of difficulty 1.)
*/
typedef struct{
    SemaphoreHandle_t pool_mutex;
    uint32_t    get_works, get_failures, remote_failures;
    uint32_t    diff1_shares, share_accpetd, share_rejected, share_discarded, share_stale;
    uint32_t    diff;

    double      diff_accepted, diff_rejected, diff_stale, diff_discarded;
    double      last_share_diff; 
}PoolStatistic;

typedef struct{
    SemaphoreHandle_t summary_mutex;
    double      last_share_diff;

    uint32_t    nonce, hw;
    uint32_t    jobs;
    uint32_t    nonce_hash;
    uint64_t    total_hash, rt_hash; /*rt = 5s*/
    uint32_t    average_hash_start_time;

    uint32_t    get_works, get_failures, remote_failures;
    uint32_t    diff1_shares, share_accpetd, share_rejected, share_discarded, share_stale;
    double      diff;        
    double      diff_accepted, diff_rejected, diff_stale, diff_discarded;
    double      hashrate_rt;/*rt = 5s*/
}SummaryStatistic;

typedef struct{
    SemaphoreHandle_t chain_mutex;
    uint32_t    nonce, hw;
    uint32_t    jobs;
    uint32_t    nonce_hash;
    uint64_t    total_hash, rt_hash; /*rt = 5s*/

    /*float  out_voltage, out_current;*/
    double  hashrate_rt, hashrate_avg, hashrate_ideal;
    uint32_t    nonce_counter[MAX_ASICS_OF_CHAIN];
    uint64_t    chip_rt_hash[MAX_ASICS_OF_CHAIN];
    double      chip_hashrate_rt[MAX_ASICS_OF_CHAIN];
    /*TODO: core id statistc.*/
}ChainStatistic;

typedef struct{
    SummaryStatistic    summary_statistic;
    PoolStatistic       pool_statistic[MAX_POOL_NUM];
    ChainStatistic      chain_statistic[MAX_CHAIN_NUM];
}StatisticModule;

esp_err_t statistic_init_system(StatisticModule *statistic_module, uint32_t nonce_hash);
esp_err_t statistic_pool_set_diff(StatisticModule *statistic_module, uint8_t pool_id, double diff);
esp_err_t statistic_notice_getwork(StatisticModule *statistic_module, uint8_t pool_id);
esp_err_t statistic_notice_submit_share(StatisticModule *statistic_module, uint8_t pool_id, double share_diff);
esp_err_t statistic_notice_share_accept(StatisticModule *statistic_module, uint8_t pool_id);
esp_err_t statistic_notice_share_reject(StatisticModule *statistic_module, uint8_t pool_id);
esp_err_t statistic_notice_share_stale(StatisticModule *statistic_module, uint8_t pool_id);
esp_err_t statistic_notice_share_discard(StatisticModule *statistic_module, uint8_t pool_id);

esp_err_t statistic_notice_nonce(
    StatisticModule *statistic_module, uint8_t chain_index, uint8_t chip_id, uint8_t core_id, uint8_t asic_core_num
);
esp_err_t statistic_get_chip_hashrate(
    StatisticModule *statistic_module, uint8_t chain_id, uint8_t chip_id, double *chip_rt_hashrate
);
esp_err_t statistic_clear_rt_hash(StatisticModule *statistic_module, uint64_t rt_calc_interval);
esp_err_t statistic_clear_average_hash(StatisticModule *statistic_module);
esp_err_t statistic_notice_hw(StatisticModule *statistic_module, uint8_t chain_index, uint8_t chip_id, uint8_t core_id, uint8_t asic_core_num);
esp_err_t statistic_notice_job(StatisticModule *statistic_module, uint8_t chain_index);
esp_err_t statistic_get_nonce_info(StatisticModule *statistic_module, uint32_t *nonce, uint32_t *hw, uint32_t *jobs);
esp_err_t statistic_get_share_info(
    StatisticModule *statistic_module, uint32_t *diff1_share, 
    uint32_t *share_accepted, uint32_t *share_rejected, uint32_t *share_stale);
esp_err_t statistic_get_diff_info(StatisticModule *statistic_module, double *diff_accepted, double *diff_rejected, double *diff_stale);    

esp_err_t statistic_get_hashrate(StatisticModule *statistic_module, double *rt_hashrate, double *average_hashrate, uint32_t up_time);

esp_err_t statistic_get_chain_info(
    StatisticModule *statistic_module, uint8_t chain_id, uint32_t *nonce, uint32_t *hw, uint32_t *jobs, double *chain_rt_hashrate
);

esp_err_t statistic_get_pool_info(
    StatisticModule *statistic_module, uint8_t pool_id, 
    uint32_t *get_works, uint32_t *get_failures, float *diff, uint32_t *diff1_share,
    uint32_t *share_accepted, uint32_t *share_rejected, uint32_t *share_discarded, uint32_t *share_stale,
    double *diff_accepted, double *diff_rejected, double *diff_stale, double *diff_discarded, double *last_share_diff
);

#endif