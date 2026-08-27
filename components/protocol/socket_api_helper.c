#include "esp_log.h"
#include "time.h"
#include "stdbool.h"
#include "stdio.h"

#include "socket_api_helper.h"

/*[{"STATUS":"S","When":1754293553,"Code":11,"Msg":"summary","Descrption":"ccminer2.3.3"}]*/
cJSON *api_status(uint16_t code, const char *msg)
{
    cJSON *status_array = cJSON_CreateArray();
    cJSON *status_object = cJSON_CreateObject();

    cJSON_AddNumberToObject(status_object, "Code", code);
    cJSON_AddStringToObject(status_object, "STATUS", "S");
    time_t now;
    time(&now);
    cJSON_AddNumberToObject(status_object, "When", (long long)now);
    cJSON_AddStringToObject(status_object, "Msg", msg);
    cJSON_AddStringToObject(status_object, "Description", "volc-miner1.0.0");

    cJSON_AddItemToArray(status_array, status_object);

    return status_array;
}

cJSON *misc_summary(const char* str_mac, int16_t envTemp, float current, float voltage, float power)
{
    cJSON *summary_array = cJSON_CreateArray();
    cJSON *summary_object = cJSON_CreateObject();

    cJSON_AddStringToObject(summary_object, "MAC", str_mac);
    cJSON_AddNumberToObject(summary_object, "envTemp", envTemp);
    cJSON_AddNumberToObject(summary_object, "current", current);
    cJSON_AddNumberToObject(summary_object, "voltage", voltage);
    cJSON_AddNumberToObject(summary_object, "power", power);

    cJSON_AddItemToArray(summary_array, summary_object);

    return summary_array;
}

cJSON *misc_response(
    uint16_t id, uint16_t code, const char* str_mac, 
    int16_t envTemp, float current, float voltage, float power)
{
    cJSON *misc_object = cJSON_CreateObject(); 
    cJSON *status_object = api_status(code, "Summary");
    cJSON *summary_object = misc_summary(str_mac, envTemp, current, voltage, power);   

    cJSON_AddNumberToObject(misc_object, "id", id);
    cJSON_AddItemToObject(misc_object, "STATUS", status_object);
    cJSON_AddItemToObject(misc_object, "SUMMARY", summary_object);
    
    return misc_object;
}

/*
{"POOL":0,"URL":"stratum+tcp://192.168.8.254:28801","Status":"Alive","Priority":0,"Quota":808464432,
"Long Poll":"N","Getworks":10,"Accepted":28,"Rejected":0,"Discarded":75,"Stale":0,"Get Failures":0,
"Remote Failures":0,"User":"dkw1225.96Chip","Last Share Time":"0:00:04","Diff":"1048576.0000",
"Diff1 Shares":4168,"Proxy Type":"","Proxy":"","Difficulty Accepted":9699328.00000000,
"Difficulty Rejected":0.00000000,"Difficulty Stale":0.00000000,"Last Share Difficulty":1048576.00000000,
"Has Stratum":true,"Stratum Active":true,"Stratum URL":"192.168.8.254","Has GBT":false,"Best Share":"  0.0000",
"Pool Rejected%":0.0000,"Pool Stale%":0.0000}
*/
cJSON *pool_element(
    uint16_t index, const char*url, uint16_t port, const char*user, 
    bool isAlive, uint16_t priority, double best_share, bool isTls)
{
    cJSON *pool = cJSON_CreateObject();

    cJSON_AddNumberToObject(pool, "POOL", index);
    char stratum_url[50];
    snprintf(stratum_url, 50, "%s://%s:%u", isTls ? "stratum+ssl" : "stratum+tcp", url, port);
    cJSON_AddStringToObject(pool, "URL", stratum_url);
    if(isAlive)
        cJSON_AddStringToObject(pool, "Status", "Alive");
    else
        cJSON_AddStringToObject(pool, "Status", "Dead");
    cJSON_AddNumberToObject(pool, "Priority", priority);
    cJSON_AddNumberToObject(pool, "Quota", 808464432);
    cJSON_AddStringToObject(pool, "Long Poll", "N");
    cJSON_AddStringToObject(pool, "User", user);
    cJSON_AddStringToObject(pool, "Stratum URL", url);

    cJSON_AddStringToObject(pool, "Proxy Type", "");
    cJSON_AddStringToObject(pool, "Proxy", "");

    cJSON_AddBoolToObject(pool, "Has Stratum", true);
    cJSON_AddBoolToObject(pool, "Stratum Active", true);
    cJSON_AddBoolToObject(pool, "Has GBT", true);

    cJSON_AddNumberToObject(pool, "Best Share", best_share);

    return pool;
}

cJSON *pool_update(
    cJSON *pool, uint32_t get_works, uint32_t get_failures, uint32_t remote_failures, 
    float diff, uint32_t diff1_shares,
    uint32_t share_accpeted, uint32_t share_rejected, uint32_t share_discarded, uint32_t share_stale,
    double diff_accepted, double diff_rejected, double diff_stale, double last_share_diff)
{
    cJSON_AddNumberToObject(pool, "Getworks", get_works);
    cJSON_AddNumberToObject(pool, "Get Failures", get_failures);
    cJSON_AddNumberToObject(pool, "Remote Failures", remote_failures);

    cJSON_AddNumberToObject(pool, "Diff", diff);
    cJSON_AddNumberToObject(pool, "Diff1 Shares", diff1_shares);
    cJSON_AddNumberToObject(pool, "Accepted", share_accpeted);
    cJSON_AddNumberToObject(pool, "Rejected", share_rejected);
    cJSON_AddNumberToObject(pool, "Discarded", share_discarded);
    cJSON_AddNumberToObject(pool, "Stale", share_stale);

    cJSON_AddNumberToObject(pool, "Difficulty Accepted", diff_accepted);
    cJSON_AddNumberToObject(pool, "Difficulty Rejected", diff_rejected);
    cJSON_AddNumberToObject(pool, "Difficulty Stale", diff_stale);

    cJSON_AddNumberToObject(pool, "Pool Rejected%", diff_rejected*100/diff_accepted);
    cJSON_AddNumberToObject(pool, "Pool Stale%", diff_stale*100/diff_accepted);

    return pool;
}

cJSON *pools_summary(cJSON *primary_pool, cJSON *fallback_pool)
{
    cJSON *pools = cJSON_CreateArray();

    cJSON_AddItemToArray(pools, primary_pool);
    cJSON_AddItemToArray(pools, fallback_pool);

    return pools;
}

cJSON *pools_response(uint16_t id, cJSON *pools_summary)
{
    cJSON *pools_response = cJSON_CreateObject();
    cJSON *pools_status = api_status(7, "2 Pool(s)");
    
    cJSON_AddNumberToObject(pools_response, "id", id);
    if(NULL != pools_status)
        cJSON_AddItemToObject(pools_response, "STATUS", pools_status);
    if(NULL != pools_summary)
        cJSON_AddItemToObject(pools_response, "POOLS", pools_summary);

    return pools_response;
}

cJSON *stats_summary(
    const char* compileTime, const char* device_module, 
    uint32_t uptime, double hashrate_5s, double hashrate_average,
    uint16_t freq,  uint16_t fan1, uint16_t fan2, 
    int8_t *chain_temp, uint32_t *chain_acn, double *chain_hashrate, uint32_t *chain_hw,
    uint32_t hw_num, float hw_rate
){
    cJSON *stats_array = cJSON_CreateArray();
    uint8_t real_chain_num = 2;
    uint8_t max_chain_num = 9;

    cJSON *stats_object = cJSON_CreateObject();
    cJSON_AddStringToObject(stats_object, "volc-miner", "1.0.0");
    cJSON_AddStringToObject(stats_object, "Miner", "1.0.0.0");
    cJSON_AddStringToObject(stats_object, "CompileTime", compileTime);
    cJSON_AddStringToObject(stats_object, "Type", device_module);

    cJSON *stats2_object = cJSON_CreateObject();
    cJSON_AddNumberToObject(stats2_object, "STATS", 0);
    cJSON_AddStringToObject(stats2_object, "ID", "A30"); 
    cJSON_AddNumberToObject(stats2_object, "Elapsed", uptime);
    cJSON_AddNumberToObject(stats2_object, "Calls", 0);
    cJSON_AddNumberToObject(stats2_object, "Wait", 0.0);
    cJSON_AddNumberToObject(stats2_object, "Min", 0.0);
    cJSON_AddNumberToObject(stats2_object, "Max", 99999999.0);

    cJSON_AddNumberToObject(stats2_object, "MHS 5s", hashrate_5s);
    cJSON_AddNumberToObject(stats2_object, "MHS av", hashrate_average);

    cJSON_AddNumberToObject(stats2_object, "pfanm", 2);
    cJSON_AddNumberToObject(stats2_object, "pfa0", 8184);
    cJSON_AddNumberToObject(stats2_object, "pfa1", 8184);
    cJSON_AddNumberToObject(stats2_object, "pfa2", 0);
    cJSON_AddNumberToObject(stats2_object, "pfa3", 0);

    char str_freq[20] = "\0";
    snprintf(str_freq, 20, "%u", freq);
    cJSON_AddStringToObject(stats2_object, "frequency", str_freq);
    
    cJSON_AddNumberToObject(stats2_object, "fan_num", 2);
    cJSON_AddNumberToObject(stats2_object, "fan1", fan1);
    cJSON_AddNumberToObject(stats2_object, "fan2", fan2);
    
    cJSON_AddNumberToObject(stats2_object, "temp_num", real_chain_num);
    int8_t temp_max = 0;
    for(uint8_t i = 0; i < max_chain_num; i++){
        char tmp_name[20];
        snprintf(tmp_name, 20, "temp%d", i+1);
        
        if(i < real_chain_num){
            cJSON_AddNumberToObject(stats2_object, tmp_name, chain_temp[i]);
            if(chain_temp[i] > temp_max)
                temp_max = chain_temp[i];
        }else{
            cJSON_AddNumberToObject(stats2_object, tmp_name, 0);
        }
    }
    cJSON_AddNumberToObject(stats2_object, "temp_max", temp_max);
    cJSON_AddNumberToObject(stats2_object, "Device Hardware%", hw_rate);
    cJSON_AddNumberToObject(stats2_object, "no_matching_work", hw_num);

    for(uint8_t i = 0; i < max_chain_num; i++){
        char tmp_name[20];
        snprintf(tmp_name, 20, "chain_acn%d", i+1);
        
        if(i < real_chain_num){
            cJSON_AddNumberToObject(stats2_object, tmp_name, chain_acn[i]);
        }else{
            cJSON_AddNumberToObject(stats2_object, tmp_name, 0);
        }
    }

    for(uint8_t i = 0; i < max_chain_num; i++){
        char tmp_name[20];
        snprintf(tmp_name, 20, "chain_acn%d", i+1);
        
        if(i < real_chain_num){
            cJSON_AddNumberToObject(stats2_object, tmp_name, chain_acn[i]);
        }else{
            cJSON_AddNumberToObject(stats2_object, tmp_name, 0);
        }
    }

    for(uint8_t i = 0; i < max_chain_num; i++){
        char tmp_name[20];
        snprintf(tmp_name, 20, "chain_acs%d", i+1);
        
        if(i < real_chain_num){
            if(24 == chain_acn[i])
                cJSON_AddStringToObject(stats2_object, tmp_name, "oooooo oooooo oooooo oooooo");
        }else{
            cJSON_AddStringToObject(stats2_object, tmp_name, "");
        }
    }

    for(uint8_t i = 0; i < max_chain_num; i++){
        char tmp_name[20];
        snprintf(tmp_name, 20, "chain_hw%d", i+1);
        
        if(i < real_chain_num){
            cJSON_AddNumberToObject(stats2_object, tmp_name, chain_hw[i]);
        }else{
            cJSON_AddNumberToObject(stats2_object, tmp_name, 0);
        }
    }

    for(uint8_t i = 0; i < max_chain_num; i++){
        char tmp_name[20];
        snprintf(tmp_name, 20, "chain_rate%d", i+1);
        
        if(i < real_chain_num){
            char str_rate[20];
            snprintf(str_rate, 20, "%4.4f", chain_hashrate[i]);
            cJSON_AddStringToObject(stats2_object, tmp_name, str_rate);
        }else{
            cJSON_AddStringToObject(stats2_object, tmp_name, "");
        }
    }

    cJSON_AddItemToArray(stats_array, stats_object);
    cJSON_AddItemToArray(stats_array, stats2_object);

    return stats_array;
}

cJSON *stats_response(uint16_t id, cJSON *stats_summary)
{
    cJSON *stats_response = cJSON_CreateObject();
    cJSON *stats_status = api_status(70, "volc-miner stats");

    cJSON_AddNumberToObject(stats_response, "id", id);
    if(NULL != stats_status)
        cJSON_AddItemToObject(stats_response, "STATUS", stats_status);
    if(NULL != stats_summary)
        cJSON_AddItemToObject(stats_response, "STATS", stats_summary);

    return stats_response;
}

cJSON *summary_summary(
    uint32_t uptime, double hashrate_5s, double hashrate_average,
    uint32_t get_works, uint32_t get_failures, uint32_t local_work, 
    uint32_t remote_failures, uint32_t network_blocks, uint32_t hw, 
    uint32_t share_discarded, uint32_t share_stale, uint32_t best_share,
    double diff_accepted, double diff_reject, double diff_stale
){
    cJSON *summary_array = cJSON_CreateArray();
    cJSON *summary_object = cJSON_CreateObject();

    cJSON_AddNumberToObject(summary_object, "Elapsed", uptime);
    cJSON_AddNumberToObject(summary_object, "MHS 5s", hashrate_5s);
    cJSON_AddNumberToObject(summary_object, "MHS av", hashrate_average);

    cJSON_AddNumberToObject(summary_object, "Found Blocks", 0);
    cJSON_AddNumberToObject(summary_object, "Getworks", get_works);
    cJSON_AddStringToObject(summary_object, "Accepted", "  0.0000");
    cJSON_AddStringToObject(summary_object, "Rejected", "  0.0000");   

    cJSON_AddNumberToObject(summary_object, "Hardware Errors", hw);
    cJSON_AddNumberToObject(summary_object, "Utility", 0.0);
    cJSON_AddNumberToObject(summary_object, "Discarded", share_discarded);
    cJSON_AddNumberToObject(summary_object, "Stale", share_stale);
    cJSON_AddNumberToObject(summary_object, "Get Failures", get_failures);
    cJSON_AddNumberToObject(summary_object, "Local Work", local_work);
    cJSON_AddNumberToObject(summary_object, "Remote Failures", remote_failures);
    cJSON_AddNumberToObject(summary_object, "Network Blocks", network_blocks);
    
    cJSON_AddNumberToObject(summary_object, "Total MH", 0.0);
    cJSON_AddNumberToObject(summary_object, "Work Utility", 0.0);

    cJSON_AddNumberToObject(summary_object, "Difficulty Accepted", diff_accepted);
    cJSON_AddNumberToObject(summary_object, "Difficulty Rejected", diff_reject);
    cJSON_AddNumberToObject(summary_object, "Difficulty Stale", diff_stale);
    cJSON_AddNumberToObject(summary_object, "Best Share", best_share);

    cJSON_AddNumberToObject(summary_object, "Device Hardware%", 0.0);
    cJSON_AddNumberToObject(summary_object, "Device Rejected%", 0.0);
    cJSON_AddNumberToObject(summary_object, "Pool Rejected%",   0.0);
    cJSON_AddNumberToObject(summary_object, "Pool Stale%",      0.0);

    time_t now;
    time(&now);
    cJSON_AddNumberToObject(summary_object, "Last getwork", (long long)now);
    cJSON_AddItemToArray(summary_array, summary_object);

    return summary_array;
}

cJSON *summary_response(uint16_t id, cJSON *summary_summary)
{
    cJSON *summary_response = cJSON_CreateObject();
    cJSON *summary_status = api_status(11, "Summary");

    cJSON_AddNumberToObject(summary_response, "id", id);
    if(NULL != summary_status)
        cJSON_AddItemToObject(summary_response, "STATUS", summary_status);
    if(NULL != summary_summary)
        cJSON_AddItemToObject(summary_response, "SUMMARY", summary_summary);

    return summary_response;
}
