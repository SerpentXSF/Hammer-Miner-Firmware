#include "http_task.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/event_groups.h"
#include "global_state.h"
#include "lvgl_screen.h"

static const char *TAG = "http_task";

#define MAX_HTTP_OUTPUT_BUFFER (2048+128)
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#define LTC_STATS_API    "https://api.blockchair.com/litecoin/stats"
#define DOGE_STATS_API   "https://api.blockchair.com/dogecoin/stats"
#define BTC_STATS_API    "https://api.blockchair.com/bitcoin/stats"

#define LTC_INFO    "https://api.blockcypher.com/v1/ltc/main"
#define DOGE_INFO   "https://api.blockcypher.com/v1/doge/main"
#define BTC_INFO    "https://api.blockcypher.com/v1/btc/main"

//#define COIN_PRICE  "https://api.coingecko.com/api/v3/simple/price?ids=litecoin,dogecoin&vs_currencies=usd"
#define COIN_PRICE  "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=usd"

#define DOGE_COIN_PRICE  "https://api.coingecko.com/api/v3/simple/price?ids=dogecoin&vs_currencies=usd"
#define LITE_COIN_PRICE  "https://api.coingecko.com/api/v3/simple/price?ids=litecoin&vs_currencies=usd"

static int responseLength = 0;
//static char local_response_buffer[MAX_HTTP_OUTPUT_BUFFER + 1];
static char *local_response_buffer = NULL;
static coin_info new_coin_info = {
        .doge_price = "$--",
        .ltc_price = "$--",
        .doge_total_hashrate = "3.01", /*3.01 PH/s*/
        .ltc_total_hashrate = "1213.77", /*2.70 PH/s */
        .doge_block_height = 5881803,
        .ltc_block_height = 2970107,
        .halving_blocks = "3360000",
        .halving_progress = "53.58%",
        .global_ltc_diff = "125.86T",
        .global_doge_diff = "36.430M"
    };

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            // Clean the buffer in case of a new request
            if (responseLength == 0 && evt->user_data) {
                // we are just starting to copy the output data into the use
                memset(evt->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
            }
            /*
             *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
             *  However, event handler can also be used in case chunked encoding is used.
             */
            //if (!esp_http_client_is_chunked_response(evt->client)) 
            {
                // If user_data buffer is configured, copy the response into the buffer
                int copy_len = 0;
                if (evt->user_data) {
                    // The last byte in evt->user_data is kept for the NULL character in case of out-of-bound access.
                    copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - responseLength));
                    if (copy_len) {
                        memcpy(evt->user_data + responseLength, evt->data, copy_len);
						responseLength += copy_len;
                    }
                }
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        default:
            break;
    }
    return ESP_OK;
}

int http_rest_btc_stats(void)
{
    esp_err_t err;

    esp_http_client_config_t config = {
        .url = BTC_STATS_API,
        .event_handler = _http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .user_data = local_response_buffer,
        .buffer_size = 2048,
        .timeout_ms = 10000,
    };
    ESP_LOGD(TAG, "HTTP request ltc stats");
    esp_http_client_handle_t client = esp_http_client_init(&config);
	if (!client) 
	{
        ESP_LOGE(TAG, "Failed to initialize HTTP client.");
        return 0;
    }

	responseLength = 0;
    // GET
    //esp_http_client_set_url(client, LTC_STATS_API);
    //esp_http_client_set_method(client, HTTP_METHOD_GET);
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGD(TAG, "HTTP GET Status = %d, content_length = %"PRId64,
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    if(err != ESP_OK)
    {
        return 0;
    }

    if (responseLength == 0) {
        ESP_LOGE(TAG, "Empty response received!");
        return 0;
    }

    //ESP_LOGI(TAG, "Received JSON: %s", local_response_buffer);

    cJSON *root = cJSON_Parse(local_response_buffer);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return 0;
    }
    cJSON *stats_data = cJSON_GetObjectItem(root, "data");
    if (!stats_data) {
        cJSON_Delete(root);
        ESP_LOGE(TAG, "Failed to parse JSON data");
        return 0;
    }

    cJSON *info = cJSON_GetObjectItem(stats_data, "blocks");
    if (!info) {
        cJSON_Delete(root);
        ESP_LOGE(TAG, "Failed to parse blocks");
        return 0;
    }

    new_coin_info.ltc_block_height = info->valueint;
    // next_halving_height = 210000 * (current_height // 210000 + 1)
    // (((m_blockHeigh / HALVING_BLOCKS) + 1) * HALVING_BLOCKS) - m_blockHeigh
    //uint32_t halving_blocks =  (new_coin_info.ltc_block_height / 840000 + 1) * 840000 - new_coin_info.ltc_block_height;
    uint32_t halving_blocks =  (new_coin_info.ltc_block_height / 210000 + 1) * 210000 - new_coin_info.ltc_block_height;
    snprintf(new_coin_info.halving_blocks, 20, "%"PRIu32"", halving_blocks); 
    // (HALVING_BLOCKS - getBlocksToHalving()) * 100 / HALVING_BLOCKS;
    //float halving_progress = ((float)840000 - halving_blocks)*100/840000;
    float halving_progress = ((float)210000 - halving_blocks)*100/210000;
    snprintf(new_coin_info.halving_progress, 20, "%.2f%%", halving_progress); 

    info = cJSON_GetObjectItem(stats_data, "difficulty");
    if (!info) {
        cJSON_Delete(root);
        ESP_LOGE(TAG, "Failed to parse difficulty");
        return 0;
    }

    snprintf(new_coin_info.global_ltc_diff, 20, "%.2fT", (float)(info->valuedouble/1e12));

    info = cJSON_GetObjectItem(stats_data, "hashrate_24h");
    if (!info) {
        cJSON_Delete(root);
        ESP_LOGE(TAG, "Failed to parse hashrate_24h");
        return 0;
    }

    uint64_t hashrate = strtoull(info->valuestring, NULL, 10);
    ESP_LOGD(TAG, "hashrate_24h %s, %"PRIu64"",info->valuestring, hashrate);

    snprintf(new_coin_info.ltc_total_hashrate, 20, "%.2f", (float)((double)hashrate/1e18));

    info = cJSON_GetObjectItem(stats_data, "market_price_usd");
    if (!info) {
        cJSON_Delete(root);
        ESP_LOGE(TAG, "Failed to parse market_price_usd");
        return 0;
    }

    snprintf(new_coin_info.ltc_price, 20, "$%.0f", info->valuedouble);

    cJSON_Delete(root);

    ESP_LOGD(TAG, "ltc block height: %d", new_coin_info.ltc_block_height);
    return 1;
}

int http_rest_doge_stats(void)
{
    esp_err_t err;

    esp_http_client_config_t config = {
        .url = DOGE_STATS_API,
        .event_handler = _http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .user_data = local_response_buffer,
        .buffer_size = 2048,
        .timeout_ms = 10000,
    };
    ESP_LOGD(TAG, "HTTP request doge stats");
    esp_http_client_handle_t client = esp_http_client_init(&config);
	if (!client) 
	{
        ESP_LOGE(TAG, "Failed to initialize HTTP client.");
        return 0;
    }

	responseLength = 0;
    // GET
    //esp_http_client_set_url(client, DOGE_STATS_API);
    //esp_http_client_set_method(client, HTTP_METHOD_GET);
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGD(TAG, "HTTP GET Status = %d, content_length = %"PRId64,
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    if(err != ESP_OK)
    {
        return 0;
    }

    if (responseLength == 0) {
        ESP_LOGE(TAG, "Empty response received!");
        return 0;
    }

    //ESP_LOGI(TAG, "Received JSON: %s", local_response_buffer);

    cJSON *root = cJSON_Parse(local_response_buffer);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return 0;
    }
    cJSON *stats_data = cJSON_GetObjectItem(root, "data");
    if (!stats_data) {
        cJSON_Delete(root);
        ESP_LOGE(TAG, "Failed to parse JSON data");
        return 0;
    }

    cJSON *info = cJSON_GetObjectItem(stats_data, "blocks");
    if (!info) {
        cJSON_Delete(root);
        ESP_LOGE(TAG, "Failed to parse blocks");
        return 0;
    }

    new_coin_info.doge_block_height = info->valueint;
    // next_halving_height = 210000 * (current_height // 210000 + 1)
    // (((m_blockHeigh / HALVING_BLOCKS) + 1) * HALVING_BLOCKS) - m_blockHeigh
    //uint32_t halving_blocks =  (new_coin_info.ltc_block_height / 840000 + 1) * 840000 - new_coin_info.ltc_block_height;
    //snprintf(new_coin_info.halving_blocks, 20, "%"PRIu32"", halving_blocks); 
    // (HALVING_BLOCKS - getBlocksToHalving()) * 100 / HALVING_BLOCKS;
    //float halving_progress = ((float)840000 - halving_blocks)*100/840000;
    //snprintf(new_coin_info.halving_progress, 20, "%.2f%%", halving_progress); 

    info = cJSON_GetObjectItem(stats_data, "difficulty");
    if (!info) {
        cJSON_Delete(root);
        ESP_LOGE(TAG, "Failed to parse difficulty");
        return 0;
    }

    snprintf(new_coin_info.global_doge_diff, 20, "%.2fM", (float)(info->valuedouble/1e6));

    info = cJSON_GetObjectItem(stats_data, "hashrate_24h");
    if (!info) {
        cJSON_Delete(root);
        ESP_LOGE(TAG, "Failed to parse hashrate_24h");
        return 0;
    }
    uint64_t hashrate = strtoull(info->valuestring, NULL, 10);
    ESP_LOGD(TAG, "hashrate_24h %s, %"PRIu64"",info->valuestring, hashrate);
    snprintf(new_coin_info.doge_total_hashrate, 20, "%.2f", (float)((float)hashrate/1e15));

    info = cJSON_GetObjectItem(stats_data, "market_price_usd");
    if (!info) {
        cJSON_Delete(root);
        ESP_LOGE(TAG, "Failed to parse market_price_usd");
        return 0;
    }
    snprintf(new_coin_info.doge_price, 20, "%.3f", info->valuedouble);

    cJSON_Delete(root);

    ESP_LOGD(TAG, "doge block height: %d", new_coin_info.doge_block_height);
    return 1;
}

int http_rest_btc(void)
{
    esp_err_t err;

    esp_http_client_config_t config = {
        .url = BTC_INFO,
        .event_handler = _http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .user_data = local_response_buffer,
        .buffer_size = 2048,
        .timeout_ms = 10000,
    };
    ESP_LOGD(TAG, "HTTP request ltc");
    esp_http_client_handle_t client = esp_http_client_init(&config);
	if (!client) 
	{
        ESP_LOGE(TAG, "Failed to initialize HTTP client.");
        return 0;
    }

	responseLength = 0;
    // GET
    //esp_http_client_set_url(client, LTC_INFO);
    //esp_http_client_set_method(client, HTTP_METHOD_GET);
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGD(TAG, "HTTP GET Status = %d, content_length = %"PRId64,
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    if(err != ESP_OK)
    {
        return 0;
    }

    if (responseLength == 0) {
        ESP_LOGE(TAG, "Empty response received!");
        return 0;
    }

    //ESP_LOGD(TAG, "Received JSON: %s", local_response_buffer);

    cJSON *root = cJSON_Parse(local_response_buffer);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return 0;
    }
    cJSON *height = cJSON_GetObjectItem(root, "height");
    if (!height || !cJSON_IsNumber(height)) {
        cJSON_Delete(root);
        return 0;
    }

    new_coin_info.ltc_block_height = height->valueint;
    // next_halving_height = 210000 * (current_height // 210000 + 1)
    // (((m_blockHeigh / HALVING_BLOCKS) + 1) * HALVING_BLOCKS) - m_blockHeigh
    //uint32_t halving_blocks =  (new_coin_info.ltc_block_height / 840000 + 1) * 840000 - new_coin_info.ltc_block_height;
    uint32_t halving_blocks =  (new_coin_info.ltc_block_height / 210000 + 1) * 210000 - new_coin_info.ltc_block_height;
    snprintf(new_coin_info.halving_blocks, 20, "%"PRIu32"", halving_blocks); 
    // (HALVING_BLOCKS - getBlocksToHalving()) * 100 / HALVING_BLOCKS;
    //float halving_progress = ((float)840000 - halving_blocks)*100/840000;
    float halving_progress = ((float)210000 - halving_blocks)*100/210000;
    snprintf(new_coin_info.halving_progress, 20, "%.2f%%", halving_progress); 
    cJSON_Delete(root);
    ESP_LOGD(TAG, "ltc block height: %d", new_coin_info.ltc_block_height);
    return 1;
}

int http_rest_doge(void)
{
    esp_err_t err;

    esp_http_client_config_t config = {
        .url = DOGE_INFO,
        .event_handler = _http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .user_data = local_response_buffer,
        .buffer_size = 2048,
        .timeout_ms = 10000,        
    };
    ESP_LOGD(TAG, "HTTP request with doge");
    esp_http_client_handle_t client = esp_http_client_init(&config);
	if (!client) 
	{
        ESP_LOGE(TAG, "Failed to initialize HTTP client.");
        return 0;
    }

	responseLength = 0;
	// GET
    //esp_http_client_set_url(client, DOGE_INFO);
    //esp_http_client_set_method(client, HTTP_METHOD_GET);
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGD(TAG, "HTTP GET Status = %d, content_length = %"PRId64,
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    if(err != ESP_OK)
    {
        return 0;
    }

    if (responseLength == 0) {
        ESP_LOGE(TAG, "Empty response received!");
        return 0;
    }

    //ESP_LOGD(TAG, "Received JSON: %s", local_response_buffer);

    cJSON *root = cJSON_Parse(local_response_buffer);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return 0;
    }
    cJSON *height = cJSON_GetObjectItem(root, "height");
    if (!height || !cJSON_IsNumber(height)) {
        cJSON_Delete(root);
        return 0;
    }

    new_coin_info.doge_block_height = height->valueint;
    cJSON_Delete(root);
    ESP_LOGD(TAG, "doge block height: %d", new_coin_info.doge_block_height);
    return 1;
}

int http_rest_price(void)
{
    esp_err_t err;

    esp_http_client_config_t config = {
        .url = COIN_PRICE,
        .event_handler = _http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .user_data = local_response_buffer,
        .buffer_size = 2048,
        .timeout_ms = 10000,        
    };
    ESP_LOGD(TAG, "HTTP request with price");
    esp_http_client_handle_t client = esp_http_client_init(&config);
	if (!client) 
	{
        ESP_LOGE(TAG, "Failed to initialize HTTP client.");
        return 0;
    }

	responseLength = 0;
	// GET
    //esp_http_client_set_url(client, COIN_PRICE);
    //esp_http_client_set_method(client, HTTP_METHOD_GET);
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %"PRId64,
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    if(err != ESP_OK)
    {
        return 0;
    }

    if (responseLength == 0) {
        ESP_LOGE(TAG, "Empty response received!");
        return 0;
    }

    ESP_LOGD(TAG, "Received JSON: %s", local_response_buffer);
    //{"dogecoin":{"usd":0.176592},"litecoin":{"usd":102.02}}
    cJSON *root = cJSON_Parse(local_response_buffer);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return 0;
    }
    cJSON *coin = cJSON_GetObjectItem(root, "dogecoin");
    cJSON *price = NULL;
    if (coin) {
        price = cJSON_GetObjectItem(coin, "usd");
        if(price)
        {
            snprintf(new_coin_info.doge_price, 20, "%.3f", price->valuedouble);
        }
    }

    coin = cJSON_GetObjectItem(root, "litecoin");
    if (coin) {
        price = cJSON_GetObjectItem(coin, "usd");
        if(price)
        {
            snprintf(new_coin_info.ltc_price, 20, "%.2f", price->valuedouble);
        }
    }

    coin = cJSON_GetObjectItem(root, "bitcoin");
    if (coin) {
        price = cJSON_GetObjectItem(coin, "usd");
        if(price)
        {
            snprintf(new_coin_info.ltc_price, 20, "$%.0f", price->valuedouble);
        }
    }

    cJSON_Delete(root);
    ESP_LOGD(TAG, "price %s,%s", new_coin_info.doge_price,new_coin_info.ltc_price);
    return 1;
}

// FreeRTOS task function
void http_task(void *pvParameters) 
{
    ESP_LOGI(TAG, "http task started");

	//GlobalState * GLOBAL_STATE = (GlobalState *) pvParameters;
    static int ltc_sta_ok = 0;

    local_response_buffer = heap_caps_calloc(1, MAX_HTTP_OUTPUT_BUFFER+16, MALLOC_CAP_SPIRAM);
    if(local_response_buffer == NULL)
    {
        ESP_LOGE(TAG, "buffer null");
        vTaskDelete(NULL);
        return;
    }

    while (1) 
	{
        vTaskDelay(5000 / portTICK_PERIOD_MS);

        if(http_rest_btc_stats())
        {
            vTaskDelay(500 / portTICK_PERIOD_MS);
            ltc_sta_ok = 1;
        }
        else
        {
            vTaskDelay(500 / portTICK_PERIOD_MS);
            ltc_sta_ok = 0;

            http_rest_btc();

            vTaskDelay(500 / portTICK_PERIOD_MS);

            http_rest_price();
        }

        refresh_coin_data(new_coin_info);

        vTaskDelay(300000 / portTICK_PERIOD_MS);
        vTaskDelay(300000 / portTICK_PERIOD_MS);
        vTaskDelay(300000 / portTICK_PERIOD_MS);
        if(ltc_sta_ok)
        {
            vTaskDelay(300000 / portTICK_PERIOD_MS);
            vTaskDelay(300000 / portTICK_PERIOD_MS);
        }
    }
}

