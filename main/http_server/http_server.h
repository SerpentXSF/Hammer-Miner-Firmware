#ifndef HTTP_SERVER_H_
#define HTTP_SERVER_H_
#include <esp_http_server.h>
#include "miner.h"
#include "esp_err.h"
#include "global_state.h" // 确保包含 GlobalState 定义

// 【新增】初始化日志系统（环形缓冲区和重定向）
void init_logging_system(void);

esp_err_t start_rest_server(void *pvParameters);
void stop_webserver(httpd_handle_t server);
#endif
