#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <lwip/sockets.h>
#include <esp_chip_info.h>
#include <esp_app_desc.h>
#include <esp_timer.h>

#include "asic.h"
#include "socket_api_task.h"
#include "socket_api_helper.h"
#include "ip_reporter.h"

#define TAG "TCP_SERVER"
#define PORT 8359
#define MAX_CLIENTS 5
#define BUFFER_SIZE 512

#ifdef STATISTIC_SYSTEM_FEATURE
#define UPDATE_STATISTIC_SYSTEM_INFO
#endif

typedef char* (*command_handler_t)(GlobalState *GLOBAL_STATE);

static char* handle_pools(GlobalState *GLOBAL_STATE);
static char* handle_stats(GlobalState *GLOBAL_STATE);
static char* handle_summary(GlobalState *GLOBAL_STATE);
static char* handle_misc(GlobalState *GLOBAL_STATE);
static char* handle_combined(const cJSON *command_json, GlobalState *GLOBAL_STATE);

static const struct {
    const char *command;
    command_handler_t handler;
} command_handlers[] = {
    {"pools", handle_pools},
    {"stats", handle_stats},
    {"summary", handle_summary},
    {"misc", handle_misc},
    {NULL, NULL}
};

static uint16_t id = 1;

void socket_api_task(void *pvParameters)
{
    GlobalState *GLOBAL_STATE = (GlobalState *)pvParameters;
    
    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    // 设置SO_REUSEADDR选项
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 绑定socket
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };
    
    if (bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        ESP_LOGE(TAG, "Failed to bind socket: errno %d", errno);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    // 开始监听
    if (listen(listen_sock, MAX_CLIENTS) != 0) {
        ESP_LOGE(TAG, "Failed to listen: errno %d", errno);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "TCP server started on port %d", PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) {
            ESP_LOGE(TAG, "Failed to accept connection: errno %d", errno);
            continue;
        }

        // 获取客户端IP
        char client_ip[16];
        inet_ntoa_r(client_addr.sin_addr, client_ip, sizeof(client_ip));
        ESP_LOGD(TAG, "Connection from %s", client_ip);

        // 接收数据
        char buffer[BUFFER_SIZE];
        int len = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        if (len < 0) {
            ESP_LOGE(TAG, "Receive failed: errno %d", errno);
            close(client_sock);
            continue;
        }
        buffer[len] = '\0';
        ESP_LOGD(TAG, "Received: %s", buffer);

        // 处理命令
        char *response = process_command(buffer, GLOBAL_STATE);
        
        // 发送响应
        if (response) {
            send(client_sock, response, strlen(response), 0);
            free(response);
        } else {
            const char *error = "{\"error\":\"Invalid command\"}";
            send(client_sock, error, strlen(error), 0);
        }

        close(client_sock);
    }
}

// 命令处理函数
char* process_command(const char *json_str, GlobalState *GLOBAL_STATE)
{
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return NULL;
    }

    cJSON *command = cJSON_GetObjectItem(root, "command");
    if (!command || !cJSON_IsString(command)) {
        cJSON_Delete(root);
        return NULL;
    }

    char *response = NULL;
    const char *command_str = command->valuestring;

    // 检查是否为组合命令
    if (strstr(command_str, "+")) {
        response = handle_combined(root, GLOBAL_STATE);
    } else {
        // 查找并执行单个命令处理程序
        for (int i = 0; command_handlers[i].command != NULL; i++) {
            if (strcmp(command_str, command_handlers[i].command) == 0) {
                response = command_handlers[i].handler(GLOBAL_STATE);
                break;
            }
        }
    }

    cJSON_Delete(root);
    return response;
}

// 组合命令处理
static char* handle_combined(const cJSON *command_json, GlobalState *GLOBAL_STATE)
{
    cJSON *response = cJSON_CreateObject();
    const char *command_str = cJSON_GetObjectItem(command_json, "command")->valuestring;
    
    char *token;
    char *saveptr;
    char *command_copy = strdup(command_str);
    
    token = strtok_r(command_copy, "+", &saveptr);
    while (token != NULL) {
        // 查找并执行每个子命令
        for (int i = 0; command_handlers[i].command != NULL; i++) {
            if (strcmp(token, command_handlers[i].command) == 0) {
                char *sub_response = command_handlers[i].handler(GLOBAL_STATE);
                cJSON *sub_json = cJSON_Parse(sub_response);
                if (sub_json) {
                    cJSON_AddItemToObject(response, token, sub_json);
                }
                free(sub_response);
                break;
            }
        }
        token = strtok_r(NULL, "+", &saveptr);
    }
    
    free(command_copy);
    return cJSON_PrintUnformatted(response);
}

static char* handle_misc(GlobalState *GLOBAL_STATE)
{
    char str_mac[30] = {"\0"};
    HealthMaintenceModule *module = &(GLOBAL_STATE->HEALTH_MODULE);
    
    get_mac(str_mac);
    cJSON *root = misc_response(
        id, 11, str_mac, module->control_board_temperature,
        module->out_current, module->out_voltage, module->power
    );

    char *response = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    id += 1;
    return response;
}

static char* handle_pools(GlobalState *GLOBAL_STATE)
{
    SystemModule *system_module = &(GLOBAL_STATE->SYSTEM_MODULE);
    
    cJSON *primary_pool = pool_element(
        0, system_module->pool_url, system_module->pool_port, system_module->pool_user,
        !(system_module->is_using_fallback), 0, (float)(system_module->best_nonce_diff),
        system_module->pool_tls > 0
    );
    cJSON *fallback_pool = pool_element(
        1, system_module->fallback_pool_url, system_module->fallback_pool_port, system_module->fallback_pool_user,
        system_module->is_using_fallback, 1, (float)(system_module->best_nonce_diff),
        system_module->fallback_pool_tls > 0
    );

#ifdef UPDATE_STATISTIC_SYSTEM_INFO
    uint32_t get_works = 0, get_failures = 0, remote_failures = 0; 
    float diff = 0.0;
    uint32_t diff1_shares = 0;
    uint32_t share_accpeted = 0, share_rejected = 0, share_discarded = 0, share_stale = 0;
    double diff_accepted = 0.0, diff_rejected = 0.0, diff_stale = 0.0, last_share_diff = 0.0;

    StatisticModule *statistic_module = &(GLOBAL_STATE->STATISTIC_MODULE);
    if(NULL != statistic_module){
        statistic_get_pool_info(
            statistic_module, 0, &get_works, &get_failures, &diff, &diff1_shares,
            &share_accpeted, &share_rejected, &share_discarded, &share_stale,
            &diff_accepted, &diff_accepted, &diff_stale, NULL, &last_share_diff
        );
        pool_update(primary_pool, get_works, get_failures, remote_failures, diff, 
            diff1_shares, share_accpeted, share_rejected, share_discarded, share_stale,
            diff_accepted, diff_rejected, diff_stale, last_share_diff
        );

        statistic_get_pool_info(
            statistic_module, 1, &get_works, &get_failures, &diff, &diff1_shares,
            &share_accpeted, &share_rejected, &share_discarded, &share_stale,
            &diff_accepted, &diff_accepted, &diff_stale, NULL, &last_share_diff
        );
        pool_update(fallback_pool, get_works, get_failures, remote_failures, diff, 
            diff1_shares, share_accpeted, share_rejected, share_discarded, share_stale,
            diff_accepted, diff_rejected, diff_stale, last_share_diff
        );        
    }

#else
    cJSON *used_pool = NULL, *unused_pool = NULL;
    if(!system_module->is_using_fallback){
        used_pool = primary_pool;
        unused_pool = fallback_pool;
    }else{
        used_pool = fallback_pool;
        unused_pool = primary_pool;
    }

    pool_update(used_pool, 0, 0, 0, 
        0, system_module->shares_accepted, system_module->shares_rejected, 0, 0, 
        0.0, 0.0, 0.0, 0.0, 0.0
    );
    pool_update(unused_pool, 0, 0, 0, 
        0, 0, 0, 0, 0,
        0.0, 0.0, 0.0, 0.0, 0.0
    );
#endif

    cJSON *pools_object = pools_summary(primary_pool, fallback_pool);
    cJSON *root = pools_response(id, pools_object);

    char *response = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    id += 1;
    return response;
}

char* replace_string(const char* original, const char* pattern, const char* replacement) {
    char* result;
    char* ins;
    char* tmp;
    int count = 0;
    int len_pattern = strlen(pattern);
    int len_replacement = strlen(replacement);

    // 计算需要替换的次数
    for (count = 0, tmp = (char*)original; (tmp = strstr(tmp, pattern)); ++count) {
        tmp += len_pattern;
    }

    // 分配新字符串内存
    int len_result = strlen(original) + (len_replacement - len_pattern) * count + 1;
    result = (char*)malloc(len_result);
    if (!result) return NULL;

    tmp = result;
    while (count--) {
        ins = strstr(original, pattern);
        int len_front = ins - original;
        tmp = strncpy(tmp, original, len_front) + len_front;
        tmp = strcpy(tmp, replacement) + len_replacement;
        original += len_front + len_pattern;
    }
    strcpy(tmp, original);
    return result;
}

static char* handle_stats(GlobalState *GLOBAL_STATE)
{
    HealthMaintenceModule *health_module = &(GLOBAL_STATE->HEALTH_MODULE);
    SystemModule *system_module = &(GLOBAL_STATE->SYSTEM_MODULE);
    uint32_t chain_acn[MAX_CHAIN_NUM], chain_hw[MAX_CHAIN_NUM];
    double chain_hashrate[MAX_CHAIN_NUM];

    int64_t up_time = esp_timer_get_time()/ 1000000;
    double average_hashrate = 0.0, rt_10s_hashrate = 0.0;
#ifdef UPDATE_STATISTIC_SYSTEM_INFO
    statistic_get_hashrate(&(GLOBAL_STATE->STATISTIC_MODULE), 
        &rt_10s_hashrate, &average_hashrate, up_time);
#endif
    cJSON *summary_object = stats_summary(
        esp_app_get_description()->version, GLOBAL_STATE->device_model_str,
        up_time, rt_10s_hashrate/1000000.0, average_hashrate/1000000.0,
        GLOBAL_STATE->asic_freqency, health_module->fan_rpm[0], health_module->fan_rpm[1],
        health_module->board_temperature, chain_acn, chain_hashrate, chain_hw,
        system_module->recveived_hw, 100.0*(float)system_module->recveived_hw/(float)system_module->recveived_nonce
    );
    cJSON *root = stats_response(id, summary_object);
    char *response = cJSON_PrintUnformatted(root);
    /*insert a josn bug for the blackminer tools*/
    char* result = replace_string(response, "\"},{\"STATS\"", "\"}{\"STATS\"");

    if(NULL != response)
        free(response);
    if(NULL != root)
        cJSON_Delete(root);

    id += 1;
    return result;
}

static char* handle_summary(GlobalState *GLOBAL_STATE)
{
    SystemModule *system_module = &(GLOBAL_STATE->SYSTEM_MODULE);
    double average_hashrate = 0.0, rt_10s_hashrate = 0.0;
    int64_t up_time = esp_timer_get_time()/ 1000000;
    double diff_accepted = 0.0, diff_rejected = 0.0, diff_stale = 0.0;
#ifdef UPDATE_STATISTIC_SYSTEM_INFO
    StatisticModule *statistic_module = &(GLOBAL_STATE->STATISTIC_MODULE);
    if(NULL != statistic_module){
        statistic_get_hashrate(statistic_module, &rt_10s_hashrate, &average_hashrate, up_time);
        statistic_get_diff_info(statistic_module, &diff_accepted, &diff_rejected, &diff_stale);
    }
#endif

    cJSON *summary_object = summary_summary(
        up_time, rt_10s_hashrate/1000000.0, average_hashrate/1000000.0,
        0,  0,  0,  0,  0,
        system_module->recveived_hw, 0, 0,  system_module->shares_accepted, 
        diff_accepted, diff_rejected, diff_stale
    );
    cJSON *root = summary_response(id, summary_object);
    char *response = cJSON_PrintUnformatted(root);

    if(NULL != root)
        cJSON_Delete(root);

    id += 1;
    return response;
}
