#include "esp_log.h"
#include "esp_sntp.h"
//#include "freertos/task.h"
#include "lwip/ip_addr.h"
#include "lwip/inet.h"

#include "ip_reporter.h"
#include "rtc_sync.h"
#include "nvs_config.h"
#include "protocol_task.h"

#include "gpio_input_output.h"

static const char* TAG = "protocol_task";

SemaphoreHandle_t xIpReporterSemaphore;
SemaphoreHandle_t xSyncTimeSemaphore, xSyncTimeDoneSemaphore;

void ip_reporter_task(void *pvParameters)
{
    ESP_LOGI(TAG, "ip_reporter is ready....");

    while(1){
        if(pdTRUE == xSemaphoreTake(xIpReporterSemaphore, portMAX_DELAY)){
            /*special case.*/
            led_on(GPIO_YELLOW_PIN);
            send_ip_reporter();
            led_off(GPIO_YELLOW_PIN);
        }else{
            ESP_LOGW(TAG, "Wait to ip-reporter timerout.");
        }
    }
}

bool check_time_is_synced() {
    bool is_synced = true;
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // 检查是否是1970年1月1日（UNIX时间戳0）
    if (timeinfo.tm_year == (1970 - 1900) && 
        timeinfo.tm_mon == 0 && 
        timeinfo.tm_mday == 1) {
        ESP_LOGI(TAG, "System time is not synchronized (1970)");
        is_synced = false;
    }

    return is_synced;
}

static void rtc_sync(void)
{
    ESP_LOGI(TAG, "RTC Sync Start......");

    char *ntp_server = \
        nvs_config_get_string(NVS_CONFIG_NTP_SERVER, "ntp.aliyun.com");
    char *ntp_server_backup = \
        nvs_config_get_string(NVS_CONFIG_NTP_SERVER_BACKUP, "time1.cloud.tencent.com");
    char *time_zone = \
        nvs_config_get_string(NVS_CONFIG_TIME_ZONE, "CST-8");

    setenv("TZ", time_zone, 1);
    tzset();   /* setenv alone is not specified to update the active zone */
    sntp_init_sync(ntp_server,ntp_server_backup);
}

void rtc_sync_task(void *pvParameters)
{
    ESP_LOGI(TAG, "rtc-sync is ready....");

    xSemaphoreTake(xSyncTimeDoneSemaphore, portMAX_DELAY);
    rtc_sync();
    vTaskDelay(pdMS_TO_TICKS(500));
    xSemaphoreGive(xSyncTimeDoneSemaphore);
    
    /*wait the sync-time event.*/
    while(1){
        if(pdTRUE == xSemaphoreTake(xSyncTimeSemaphore, portMAX_DELAY)){
            sntp_restart();
            xSemaphoreGive(xSyncTimeDoneSemaphore);
        }else{
            ESP_LOGW(TAG, "Wait to rtc sync timeout.");
        }
    }
}

bool is_valid_ip(const char *ip_str) {
    ip4_addr_t ip4;
    if (ip4addr_aton(ip_str, &ip4)) {  // IPv4 检查
        return true;
    }

    ip6_addr_t ip6;
    if (ip6addr_aton(ip_str, &ip6)) {  // IPv6 检查
        return true;
    }

    return false;
}