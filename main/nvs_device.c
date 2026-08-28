#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs_config.h"
#include "nvs_device.h"

#include "connect.h"
#include "global_state.h"
#include "asic.h"

static const char * TAG = "nvs_device";

esp_err_t NVSDevice_init(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if(err != ESP_OK)
    {
        ESP_LOGW(TAG, "ERR %X", err);
    }
    return err;
}

esp_err_t NVSDevice_parse_config(GlobalState * GLOBAL_STATE) 
{
    GLOBAL_STATE->device_model_str = nvs_config_get_string(NVS_CONFIG_DEVICE_MODEL, CONFIG_DEVICE_MODULE);
    if (strcmp(GLOBAL_STATE->device_model_str, "BC04") == 0) {
        ESP_LOGI(TAG, "DEVICE Model: BC04");
        GLOBAL_STATE->device_model = DEVICE_BC04;
        GLOBAL_STATE->voltage_domain = 2;
        GLOBAL_STATE->asic_difficulty = BC04_LOTTO_ASIC_DIFFICULTY;
        GLOBAL_STATE->asic_vol_max = CONFIG_TPS546_VOUT_MAX;
        GLOBAL_STATE->asic_vol_min = CONFIG_TPS546_VOUT_MIN;
		GLOBAL_STATE->asic_vol_default = nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE_DEF, CONFIG_ASIC_VOLTAGE*4);
        GLOBAL_STATE->asic_job_frequency_ms = 300;
        GLOBAL_STATE->asic_vol_default = GLOBAL_STATE->asic_vol_default > 480 ? 480 : GLOBAL_STATE->asic_vol_default;
    }
    else if (strcmp(GLOBAL_STATE->device_model_str, "BC08") == 0) {
        ESP_LOGI(TAG, "DEVICE Model: BC08");
        GLOBAL_STATE->device_model = DEVICE_BC08;
        GLOBAL_STATE->voltage_domain = 2;
        GLOBAL_STATE->asic_difficulty = BC08_LOTTO_ASIC_DIFFICULTY;
        GLOBAL_STATE->asic_vol_max = CONFIG_TPS546_VOUT_MAX;
        GLOBAL_STATE->asic_vol_min = CONFIG_TPS546_VOUT_MIN;
		GLOBAL_STATE->asic_vol_default = nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE_DEF, CONFIG_ASIC_VOLTAGE*4);
        GLOBAL_STATE->asic_job_frequency_ms = 300;
        GLOBAL_STATE->asic_vol_default = GLOBAL_STATE->asic_vol_default > 480 ? 480 : GLOBAL_STATE->asic_vol_default;
    }
    else if (strcmp(GLOBAL_STATE->device_model_str, "BC06") == 0) {
        ESP_LOGI(TAG, "DEVICE Model: BC06");
        GLOBAL_STATE->device_model = DEVICE_BC06;
        GLOBAL_STATE->voltage_domain = 2;
        GLOBAL_STATE->asic_difficulty = BC06_LOTTO_ASIC_DIFFICULTY;
        GLOBAL_STATE->asic_vol_max = CONFIG_TPS546_VOUT_MAX*3/4;
        GLOBAL_STATE->asic_vol_min = CONFIG_TPS546_VOUT_MIN;
		GLOBAL_STATE->asic_vol_default = nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE_DEF, CONFIG_ASIC_VOLTAGE*3);
        GLOBAL_STATE->asic_job_frequency_ms = 300;
    }
    else if (strcmp(GLOBAL_STATE->device_model_str, "BC01") == 0) {
        ESP_LOGI(TAG, "DEVICE Model: BC01");
        GLOBAL_STATE->device_model = DEVICE_BC01;
        GLOBAL_STATE->voltage_domain = 2;
        GLOBAL_STATE->asic_difficulty = BC01_LOTTO_ASIC_DIFFICULTY;
        /* CONFIG_TPS546_VOUT_MAX is 130 in the BC01 configuration, i.e.
         * 1.30 V for a single-ASIC domain. The 520 this tree inherited was
         * the BC04's, whose core domain is a series string of four. */
        GLOBAL_STATE->asic_vol_max = CONFIG_TPS546_VOUT_MAX;
        GLOBAL_STATE->asic_vol_min = CONFIG_TPS546_VOUT_MIN;
		GLOBAL_STATE->asic_vol_default = nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE_DEF, CONFIG_ASIC_VOLTAGE);
        GLOBAL_STATE->asic_job_frequency_ms = 300;
    }
    else if (strcmp(GLOBAL_STATE->device_model_str, "BC01-Pro") == 0) {
        ESP_LOGI(TAG, "DEVICE Model: BC01-Pro");
        GLOBAL_STATE->device_model = DEVICE_BC01_Pro;
        GLOBAL_STATE->voltage_domain = 2;
        GLOBAL_STATE->asic_difficulty = BC01_LOTTO_ASIC_DIFFICULTY;
        GLOBAL_STATE->asic_vol_max = 115;
        GLOBAL_STATE->asic_vol_min = 90;
        GLOBAL_STATE->asic_vol_default = nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE_DEF, 95);
        GLOBAL_STATE->asic_job_frequency_ms = 300;
    }
    else if (strcmp(GLOBAL_STATE->device_model_str, "BC02") == 0) {
        ESP_LOGI(TAG, "DEVICE Model: BC02");
        GLOBAL_STATE->device_model = DEVICE_BC02;
        GLOBAL_STATE->voltage_domain = 2;
        GLOBAL_STATE->asic_difficulty = BC02_LOTTO_ASIC_DIFFICULTY;
        GLOBAL_STATE->asic_vol_max = CONFIG_TPS546_VOUT_MAX * 2;
        GLOBAL_STATE->asic_vol_min = CONFIG_TPS546_VOUT_MIN;
		GLOBAL_STATE->asic_vol_default = nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE_DEF, CONFIG_ASIC_VOLTAGE);
        GLOBAL_STATE->asic_job_frequency_ms = 300;
    }
    else {
        ESP_LOGE(TAG, "Invalid DEVICE Model: %s", GLOBAL_STATE->device_model_str);
    }

    GLOBAL_STATE->asic_model_str = nvs_config_get_string(NVS_CONFIG_ASIC_MODEL, CONFIG_ASIC_MODULE);
    GLOBAL_STATE->board_version = atoi((nvs_config_get_string(NVS_CONFIG_BOARD_VERSION, "000")+1));
    ESP_LOGI(TAG, "ASIC Model: %s", GLOBAL_STATE->asic_model_str);
    ESP_LOGI(TAG, "Board Version: %d", GLOBAL_STATE->board_version);

    if(0 == strcmp(GLOBAL_STATE->asic_model_str, "lt0051")){
        GLOBAL_STATE->asic_model = ASIC_LT0051;
        GLOBAL_STATE->asic_job_frequency_ms = 500;
    }

    return ESP_OK;
}
