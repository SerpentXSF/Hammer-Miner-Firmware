#include "rtc_sync.h"

#include "esp_log.h"
#include "esp_sntp.h"
#include "time.h"

static const char *TAG = "rtc_sync";

// NTP时间同步完成后的回调函数
static void time_sync_notification_cb(struct timeval *tv) {
    ESP_LOGI(TAG, "ntp sync done.");
}

void sntp_init_sync(
    const char* str_ntp_server,
    const char* str_ntp_server_backup
){
    ESP_LOGI(TAG, "Initilize SNTP %s, %s.", str_ntp_server, str_ntp_server_backup);

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, str_ntp_server);
    esp_sntp_setservername(1, str_ntp_server_backup);
    esp_sntp_set_sync_interval(3600*1000);
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();

    // 等待NTP同步完成（超时时间10秒）
    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;
    const int retry_count = 10;
    while (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "wait update ntp... (%d/%d)", retry, retry_count);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    time(&now);
    localtime_r(&now, &timeinfo);
    ESP_LOGI(TAG, "Current time: %s", asctime(&timeinfo));
}

void print_time()
{
    struct tm timeinfo;
    time_t now;
    time(&now);
    localtime_r(&now, &timeinfo);
    ESP_LOGI(TAG, "Current time: %s", asctime(&timeinfo));
}

/*
// 将系统时间更新到RTC
esp_err_t update_rtc_from_system_time(void) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);  // 转换为本地时间（默认UTC，可配置时区）

    // 检查时间有效性（避免写入1970年之前的无效时间）
    if (timeinfo.tm_year < (2020 - 1900)) {  // 年份需大于2020年
        ESP_LOGE(TAG, "Get invalid time, do not update the rtc timer.");
        return ESP_FAIL;
    }

    // 转换为RTC所需的格式
    rtc_time_t rtc_time;
    rtc_time.tm_sec = timeinfo.tm_sec;
    rtc_time.tm_min = timeinfo.tm_min;
    rtc_time.tm_hour = timeinfo.tm_hour;
    rtc_time.tm_mday = timeinfo.tm_mday;
    rtc_time.tm_mon = timeinfo.tm_mon;
    rtc_time.tm_year = timeinfo.tm_year;  // 注意：tm_year是自1900年起的年数

    // 写入RTC
    esp_err_t ret = rtc_set_time(&rtc_time);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "RTC time update: %04d-%02d-%02d %02d:%02d:%02d",
                 timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
        ESP_LOGE(TAG, "RTC time failed to update: %s", esp_err_to_name(ret));
    }
    return ret;
}
*/
