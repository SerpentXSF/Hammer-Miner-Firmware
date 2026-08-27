#include "unity.h"
#include "stdbool.h"
#include "string.h"
#include "stdlib.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "ip_reporter.h"
#include "rtc_sync.h"
#include "nvs_config.h"

static const char *TAG = "test_protocol";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define WIFI_TIMEOUT_MS    10000 // 10 seconds timeout

static EventGroupHandle_t s_wifi_event_group;

static void event_handler(void* arg, esp_event_base_t event_base,
                         int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_test_connect(const char *ssid, const char *password)
{
    // Initialize event group
    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_FAIL;
    }

    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // Initialize Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                       ESP_EVENT_ANY_ID,
                                                       &event_handler,
                                                       NULL,
                                                       NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                       IP_EVENT_STA_GOT_IP,
                                                       &event_handler,
                                                       NULL,
                                                       NULL));

    // Configure Wi-Fi
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = {0},
            .password = {0},
        },
    };
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Wait for connection or timeout
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                          WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                          pdFALSE,
                                          pdFALSE,
                                          pdMS_TO_TICKS(WIFI_TIMEOUT_MS));

    esp_err_t result = ESP_OK;
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to Wi-Fi: %s", ssid);
        result = ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to Wi-Fi: %s", ssid);
        result = ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "Wi-Fi connection timeout");
        result = ESP_ERR_TIMEOUT;
    }

    return result;
}

TEST_CASE("test_connect_wifi", "[test-connect]")
{
    ESP_LOGI(TAG, "test_connect_wifi.");

    char * wifi_ssid = nvs_config_get_string(NVS_CONFIG_WIFI_SSID, "myssid");
    char * wifi_pass = nvs_config_get_string(NVS_CONFIG_WIFI_PASS, "mypass");

    esp_err_t ret = wifi_test_connect(wifi_ssid, wifi_pass);
    TEST_ASSERT_EQUAL_INT32(ret, ESP_OK);

    ESP_LOGI(TAG, "ESP_WIFI Access Point Off");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    if (NULL != wifi_ssid)
        free(wifi_ssid);
    if (NULL != wifi_pass)
        free(wifi_pass); 
}

TEST_CASE("test_ip_reporter", "[test-protocol]")
{
    ESP_LOGI(TAG, "test_ip_reporter");

    esp_err_t ret = send_ip_reporter();
    TEST_ASSERT_EQUAL_INT32(ret, ESP_OK);

    ESP_LOGI(TAG, "test_ip_reporter done.");
}

TEST_CASE("test_ntp_server", "[test-protocol]")
{
    ESP_LOGI(TAG, "test_ntp_server.");

    char *ntp_server = \
        nvs_config_get_string(NVS_CONFIG_NTP_SERVER, "ntp.aliyun.com");
    char *ntp_server_backup = \
        nvs_config_get_string(NVS_CONFIG_NTP_SERVER_BACKUP, "time1.cloud.tencent.com");

    //setenv("TZ", "CST-8", 1);
    //setenv("TZ", "UTC-5", 1);
    setenv("TZ", "EST5EDT", 1);
    sntp_init_sync(ntp_server, ntp_server_backup);
    print_time();

    ESP_LOGI(TAG, "test_ntp_server done.");
}