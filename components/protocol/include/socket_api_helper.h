#ifndef _SOCKET_API_HELPER_H
#define _SOCKET_API_HELPER_H

#include "stdint.h"
#include "stdbool.h"

#include "cJSON.h"

cJSON *misc_response(uint16_t id, uint16_t code, const char* str_mac, int16_t envTemp, float current, float voltage, float power);

cJSON *pool_element(
    uint16_t index, const char*url, uint16_t port, const char*user, 
    bool isAlive, uint16_t priority, double best_share, bool isTls);
cJSON *pool_update(
    cJSON *pool, uint32_t get_works, uint32_t get_failures, uint32_t remote_failures, 
    float diff, uint32_t diff1_shares,
    uint32_t share_accpeted, uint32_t share_rejected, uint32_t share_discarded, uint32_t share_stale,
    double diff_accepted, double diff_rejected, double diff_stale, double last_share_diff);

cJSON *pools_summary(cJSON *primary_pool, cJSON *fallback_pool);
cJSON *pools_response(uint16_t id, cJSON *pools_summary);

cJSON *stats_summary(
    const char* compileTime, const char* device_module, 
    uint32_t uptime, double hashrate_5s, double hashrate_average,
    uint16_t freq,  uint16_t fan1, uint16_t fan2, 
    int8_t *chain_temp, uint32_t *chain_acn, double *chain_hashrate, uint32_t *chain_hw,
    uint32_t hw_num, float hw_rate    
);
cJSON *stats_response(uint16_t id, cJSON *stats_summary);

cJSON *summary_summary(
    uint32_t uptime, double hashrate_5s, double hashrate_average,
    uint32_t get_works, uint32_t get_failures, uint32_t local_work, 
    uint32_t remote_failures, uint32_t network_blocks, 
    uint32_t hw, uint32_t share_discarded, uint32_t share_stale, uint32_t best_share,
    double diff_accepted, double diff_reject, double diff_stale);
cJSON *summary_response(uint16_t id, cJSON *summary_summary);

#endif