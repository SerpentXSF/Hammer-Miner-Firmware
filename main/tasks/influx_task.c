#include "global_state.h"
#include "system.h"
#include "device.h"

#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"

#include "nvs_config.h"
#include "vcore.h"
#include "internal_sensor.h"

#include "global_state.h"
#include "nvs_config.h"
#include "influx_task.h"
#include "ping_task.h"

static const char * TAG = "influx_task";

static Influx *influxdb = NULL;

bool last_block_found = false;

#define CONFIG_INFLUX_ENABLE 0
/* Empty by default. These carried the vendor's own telemetry endpoint and
 * a plaintext token, so any device that enabled reporting without setting
 * its own destination would have reported to them. Telemetry is opt-in and
 * has nowhere to go until the owner supplies a server. */
#define CONFIG_INFLUX_URL ""
#define CONFIG_INFLUX_TOKEN ""
#define CONFIG_INFLUX_PORT 8086
#define CONFIG_INFLUX_BUCKET ""
#define CONFIG_INFLUX_ORG ""
#define CONFIG_INFLUX_PREFIX "mainnet_stats"


// Timer callback function to increment uptime counters
void uptime_timer_callback(TimerHandle_t xTimer)
{
    // Increment uptime counters
    pthread_mutex_lock(&influxdb->lock);
    influxdb->stats.total_uptime += 1;
    influxdb->stats.uptime += 1;
    pthread_mutex_unlock(&influxdb->lock);
}

void influx_task_set_temperature(float temp, float temp2)
{
    if (!influxdb) {
        return;
    }
    pthread_mutex_lock(&influxdb->lock);
    influxdb->stats.temp = temp;
    influxdb->stats.temp2 = temp2;
    pthread_mutex_unlock(&influxdb->lock);
}

void influx_task_set_pwr(float vin, float iin, float pin, float vout, float iout, float pout)
{
    if (!influxdb) {
        return;
    }
    pthread_mutex_lock(&influxdb->lock);
    influxdb->stats.pwr_vin = vin;
    influxdb->stats.pwr_iin = iin;
    influxdb->stats.pwr_pin = pin;
    influxdb->stats.pwr_vout = vout;
    influxdb->stats.pwr_iout = iout;
    influxdb->stats.pwr_pout = pout;
    pthread_mutex_unlock(&influxdb->lock);
}

static void influx_task_fetch_from_system_module(GlobalState *module)
{
    // fetch best difficulty
    float best_diff = module->SYSTEM_MODULE.best_session_nonce_diff;

    influxdb->stats.best_difficulty = best_diff;

    if (best_diff > influxdb->stats.total_best_difficulty) {
        influxdb->stats.total_best_difficulty = best_diff;
    }

    // fetch hashrate
    influxdb->stats.hashing_speed = module->SYSTEM_MODULE.current_hashrate;

    // accepted
    influxdb->stats.accepted = module->SYSTEM_MODULE.shares_accepted;

    // rejected
    influxdb->stats.not_accepted = module->SYSTEM_MODULE.shares_rejected;

    // pool errors
    influxdb->stats.pool_errors = module->SYSTEM_MODULE.recveived_hw;

    // pool difficulty
    influxdb->stats.difficulty = module->asic_difficulty;

    // Ping RTT
    influxdb->stats.last_ping_rtt = get_last_ping_rtt();

    // found block
    // firmware sets the flag but never removes it
    // so detect the "edge"
    bool found = module->SYSTEM_MODULE.FOUND_BLOCK;
    if (found && !last_block_found) {
        influxdb->stats.blocks_found++;
        influxdb->stats.total_blocks_found++;
    }
    last_block_found = found;
}

static void forever()
{
    ESP_LOGI(TAG, "halting influx_task");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(15000));
    }
}

void influx_task(void *pvParameters)
{
    GlobalState *module = (GlobalState *) pvParameters;

    int influxEnable = nvs_config_get_u16(NVS_CONFIG_INFLUX_ENABLE, CONFIG_INFLUX_ENABLE);

    ESP_LOGI(TAG, "Influx Task start.  influxEnable: %d", influxEnable);
    if (!influxEnable) {
        ESP_LOGI(TAG, "InfluxDB is not enabled.");
        forever();
    }

    char *influxURL = nvs_config_get_string(NVS_CONFIG_INFLUX_URL, CONFIG_INFLUX_URL);
    int influxPort = nvs_config_get_u16(NVS_CONFIG_INFLUX_PORT, CONFIG_INFLUX_PORT);
    char *influxToken = nvs_config_get_string(NVS_CONFIG_INFLUX_TOKEN, CONFIG_INFLUX_TOKEN);
    char *influxBucket = nvs_config_get_string(NVS_CONFIG_INFLUX_BUCKET, CONFIG_INFLUX_BUCKET);
    char *influxOrg = nvs_config_get_string(NVS_CONFIG_INFLUX_ORG, CONFIG_INFLUX_ORG);
    char *influxPrefix = nvs_config_get_string(NVS_CONFIG_INFLUX_PREFIX, CONFIG_INFLUX_PREFIX);

    /* Telemetry has no default destination any more, so it is possible to
     * enable reporting without having said where to. Stop here rather than
     * retrying against an empty host for the lifetime of the device. */
    if (NULL == influxURL || '\0' == influxURL[0]) {
        ESP_LOGW(TAG, "InfluxDB is enabled but no server URL is configured; "
                      "reporting disabled. Set influx_url to use it.");
        forever();
    }

    ESP_LOGI(TAG, "URL: %s, port: %d, bucket: %s, org: %s, prefix: %s", influxURL, influxPort, influxBucket, influxOrg,
             influxPrefix);

    influxdb = influx_init(influxURL, influxPort, influxToken, influxBucket, influxOrg, influxPrefix);

    bool ping_ok = false;
    bool bucket_ok = false;
    bool loaded_values_ok = false;
    // c can be weird at times :weird-smiley-guy:
    while (1) {
        do {
            ping_ok = ping_ok || influx_ping(influxdb);
            if (!ping_ok) {
                ESP_LOGE(TAG, "InfluxDB not reachable!");
                break;
            }
            ESP_LOGE(TAG, "InfluxDB ping %s is reachabled!", influxURL);

            bucket_ok = bucket_ok || bucket_exists(influxdb);
            if (!bucket_ok) {
                ESP_LOGE(TAG, "Bucket not found!");
                if (!influx_create_bucket(influxdb)) {
                    ESP_LOGE(TAG, "Bucket couldn't be created!");
                    forever();
                }
                ESP_LOGE(TAG, "InfluxDB create bucket is successed!");
                break;
            }

            ESP_LOGE(TAG, "InfluxDB bucket is existed!");

            loaded_values_ok = loaded_values_ok || load_last_values(influxdb);
            if (!loaded_values_ok) {
                ESP_LOGE(TAG, "loading last values failed");
                break;
            }

            ESP_LOGE(TAG, "InfluxDB load last value success!");
        } while (0);
        if (loaded_values_ok) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(15000));
    }

    ESP_LOGI(TAG, "last values: total_uptime: %d, total_best_difficulty: %.3f, total_blocks_found: %d",
             influxdb->stats.total_uptime, influxdb->stats.total_best_difficulty, influxdb->stats.total_blocks_found);

    // Create and start the uptime timer with a 1-second period
    TimerHandle_t uptime_timer = xTimerCreate("UptimeTimer",
                                                pdMS_TO_TICKS(1000),
                                                pdTRUE,
                                                (void *) 0,
                                                uptime_timer_callback);
    if (uptime_timer != NULL) {
        xTimerStart(uptime_timer, 0);
    } else {
        ESP_LOGE(TAG, "Failed to create uptime timer");
        forever();
    }

    while (1) {
        pthread_mutex_lock(&influxdb->lock);
        influx_task_fetch_from_system_module(module);
        influx_write(influxdb);
        pthread_mutex_unlock(&influxdb->lock);
        vTaskDelay(pdMS_TO_TICKS(15000));
    }
}
