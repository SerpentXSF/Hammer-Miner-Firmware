#include <stdbool.h>

#include "device.h"
#include "vcore.h"
#include "nvs_config.h"
#include "miner.h"
#include "system.h"
#include "eeprom.h"
#include "asic.h"
#include "lvgl_porting.h"
#include "esp_log.h"
#include "main.h"

#define TAG "device"

/*
 * Per-model I2C thermal sensor layout.
 *
 * Some boards cannot be told apart by their sensor address alone: BC01,
 * BC02 and BC04 all answer at 0x48. Where a board IS distinguishable, a
 * missing sensor is treated as evidence the configured model is wrong and
 * the firmware reconfigures itself and reboots. That is what `fallback`
 * expresses; models with no distinguishing address leave it NULL and only
 * log, because guessing would trade a wrong reading for a boot loop.
 *
 * The vendor release carried BC04, BC06 and BC08 as three near-identical
 * copies of the same block, and sent everything else -- including BC01 and
 * BC02, which nvs_device.c already configures -- down an else branch that
 * reset the model to "DC04". No such model exists in nvs_device.c, so a
 * BC01 running that build would reset, fail to match again, and loop.
 */
typedef struct {
    DeviceModel  model;
    const char  *name;
    uint8_t      primary_addr;
    uint8_t      secondary_addr;   /* 0 when the board has one sensor */
    const char  *fallback;         /* model to adopt if the sensor is absent */
} device_thermal_profile_t;

static const device_thermal_profile_t device_thermal_profiles[] = {
    { DEVICE_BC01, "BC01", TMP75_I2CADDR_BC01,   0,                    NULL   },
    { DEVICE_BC02, "BC02", TMP75_I2CADDR_BC01,   0,                    NULL   },
    { DEVICE_BC04, "BC04", TMP75_I2CADDR_BC04,   0,                    "BC08" },
    { DEVICE_BC06, "BC06", TMP75_I2CADDR_BC08_1, TMP75_I2CADDR_BC08_2, "BC04" },
    { DEVICE_BC08, "BC08", TMP75_I2CADDR_BC08_1, TMP75_I2CADDR_BC08_2, "BC04" },
};

static const device_thermal_profile_t *find_thermal_profile(DeviceModel model)
{
    for (size_t i = 0; i < sizeof(device_thermal_profiles) / sizeof(device_thermal_profiles[0]); i++) {
        if (device_thermal_profiles[i].model == model) {
            return &device_thermal_profiles[i];
        }
    }
    return NULL;
}

esp_err_t init_all_i2c_dev(GlobalState *GLOBAL_STATE)
{
    esp_err_t ret = ESP_FAIL;

    ESP_LOGI(TAG, "Initialize all the i2c dev.");

    ESP_ERROR_CHECK(ret = EMC2302_init());
    uint16_t Polarity = nvs_config_get_u16(NVS_CONFIG_INVERT_FAN_POLARITY, 0);
    ret = EMC2302_installed(Polarity);
    if (ESP_OK != ret) {
        return ret;
    }

    const device_thermal_profile_t *profile = find_thermal_profile(GLOBAL_STATE->device_model);
    if (NULL == profile) {
        ESP_LOGE(TAG, "DEVICE model %s unknown; supported models are BC01, BC02, BC04, BC06, BC08",
                 GLOBAL_STATE->device_model_str);
        return ESP_ERR_NOT_SUPPORTED;
    }

    ret = TMP75_init(profile->primary_addr, 0);
    ret = TMP75_installed(0);
    if (ESP_OK != ret) {
        if (NULL != profile->fallback) {
            ESP_LOGE(TAG, "DEVICE model %s identify mismatch, reset model to %s",
                     profile->name, profile->fallback);
            nvs_config_set_string(NVS_CONFIG_DEVICE_MODEL, profile->fallback);
            vTaskDelay(pdMS_TO_TICKS(1000));
            restart_with_reason("Device model identify mismatch");
        }
        ESP_LOGE(TAG, "DEVICE model %s: temperature sensor not responding at 0x%02x",
                 profile->name, profile->primary_addr);
        return ret;
    }

    if (0 != profile->secondary_addr) {
        ret = TMP75_init(profile->secondary_addr, 1);
        ret = TMP75_installed(1);
        if (ESP_OK != ret) {
            ESP_LOGE(TAG, "DEVICE model %s temp2 error", profile->name);
        }
    }

    return ret;
}

/* True when the board carries a second thermal sensor. */
static bool device_has_second_sensor(DeviceModel model)
{
    const device_thermal_profile_t *profile = find_thermal_profile(model);
    return NULL != profile && 0 != profile->secondary_addr;
}

esp_err_t read_hash_board_temperature(GlobalState *GLOBAL_STATE)
{
    esp_err_t ret = ESP_OK;

    GLOBAL_STATE->HEALTH_MODULE.board_temperature[0] = TMP75_read_temperature(0);
    if (device_has_second_sensor(GLOBAL_STATE->device_model)) {
        GLOBAL_STATE->HEALTH_MODULE.board_temperature[1] = TMP75_read_temperature(1);
    }
    return ret;
}

esp_err_t power_on_hashboard(GlobalState *GLOBAL_STATE)
{
    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "vcore power on hashboard, set voltage %d", GLOBAL_STATE->asic_vol_default);
    float core_voltage = (float)GLOBAL_STATE->asic_vol_default / 100.0;
    VCORE_set_voltage(core_voltage);

    return ret;
}

esp_err_t power_off_hashboard(GlobalState *GLOBAL_STATE)
{
    esp_err_t ret = ESP_OK;

    GLOBAL_STATE->asic_vol_default = 0;
    ESP_LOGI(TAG, "vcore power off hashboard, set voltage 0");
    float core_voltage = 0;
    VCORE_set_voltage(core_voltage);

    return ret;
}

esp_err_t read_power_information(GlobalState *GLOBAL_STATE)
{
    esp_err_t ret = ESP_OK;

    ret = VCORE_update_power(GLOBAL_STATE);


    return ret;
}

int read_power_temp(void)
{
    return VCORE_get_temp();
}

esp_err_t set_fan_pwm(GlobalState *GLOBAL_STATE, uint8_t pwm_percent)
{
    esp_err_t ret = ESP_OK;

    ret = EMC2302_set_fan_speed(pwm_percent);

    return ret;
}

esp_err_t read_fan_rpm(GlobalState *GLOBAL_STATE)
{
    esp_err_t ret = ESP_OK;

    ESP_ERROR_CHECK(EMC2302_get_fan_speed(GLOBAL_STATE->HEALTH_MODULE.fan_rpm));
    GLOBAL_STATE->HEALTH_MODULE.fan_rpm[1] = 0;

    return ret;
}

void reset_hash_board(GlobalState *GLOBAL_STATE)
{
    ESP_LOGI(TAG, "Reset Chain 0.");
    ESP_ERROR_CHECK(reset_pin_init(0));
    reset_pin_low(0);
    volc_delay(500);
    reset_pin_high(0);
    volc_delay(500);
}

void hashboard_reset_pin_init_and_low(GlobalState *GLOBAL_STATE)
{
    ESP_LOGI(TAG, " reset pin init.");
    ESP_ERROR_CHECK(reset_pin_init(0));
    reset_pin_low(0);
    volc_delay(300);
}

void dev_display_init(GlobalState *GLOBAL_STATE)
{
    uint16_t invertScreen = 0;
    if(GLOBAL_STATE->board_version > 1)
    {
        invertScreen = 1;
    }
    invertScreen = nvs_config_get_u16(NVS_CONFIG_FLIP_SCREEN, invertScreen);
    display_s3_init(invertScreen);
}

/*init all the peripherals of esp32 control board, the flow should be passed.*/
esp_err_t init_all_peripherals(GlobalState *GLOBAL_STATE)
{
    esp_err_t ret = ESP_OK;

    start_internal_temperature_sensor();

	init_all_i2c_dev(GLOBAL_STATE);

    //SYSTEM_get_config_by_boot_mode(GLOBAL_STATE);

    /*vcore related sensor init*/
    ESP_ERROR_CHECK(VCORE_init(GLOBAL_STATE));

    /*init the power.*/
    power_on_hashboard(GLOBAL_STATE);
    volc_delay(3000);

    /*reset the hashboard*/
    reset_hash_board(GLOBAL_STATE);

    GLOBAL_STATE->interface_initalized = true;
    return ret;
}

#ifdef STATISTIC_SYSTEM_FEATURE
esp_err_t statistic_init_system_by_device(GlobalState *GLOBAL_STATE)
{
    esp_err_t ret = ESP_OK;

    ret = statistic_init_system(&(GLOBAL_STATE->STATISTIC_MODULE), VOLCMINER_LOTTO_TM_CACULATE_DEFINE);
}
#endif
