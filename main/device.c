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

esp_err_t init_all_i2c_dev(GlobalState *GLOBAL_STATE)
{
    esp_err_t ret = ESP_FAIL;

    ESP_LOGI(TAG, "Initialize all the i2c dev.");

	ESP_ERROR_CHECK(ret = EMC2302_init());
    uint16_t  Polarity = nvs_config_get_u16(NVS_CONFIG_INVERT_FAN_POLARITY, 0);
	ret = EMC2302_installed(Polarity);
	if(ESP_OK != ret)
	{
		return ret;
	}

	if(DEVICE_BC04 == GLOBAL_STATE->device_model)
	{
		ret = TMP75_init(TMP75_I2CADDR_BC04, 0);
		ret = TMP75_installed(0);
		if(ESP_OK != ret)
		{
			ESP_LOGE(TAG, "DEVICE model BC04 identify mismatch, reset model to BC08");
			nvs_config_set_string(NVS_CONFIG_DEVICE_MODEL, "BC08");
			vTaskDelay(pdMS_TO_TICKS(1000));
			restart_with_reason("BC04 model identify mismatch, reset to BC08");
		}
    	return ret;
	}
    else if(DEVICE_BC08 == GLOBAL_STATE->device_model)
	{
		ret = TMP75_init(TMP75_I2CADDR_BC08_1, 0);
		ret = TMP75_installed(0);
		if(ESP_OK != ret)
		{
			ESP_LOGE(TAG, "DEVICE model BC08 identify mismatch, reset model to BC04");
			nvs_config_set_string(NVS_CONFIG_DEVICE_MODEL, "BC04");
			vTaskDelay(pdMS_TO_TICKS(1000));
			restart_with_reason("BC08 model identify mismatch, reset to BC04");
		}
        else
        {
            ret = TMP75_init(TMP75_I2CADDR_BC08_2, 1);
            ret = TMP75_installed(1);
            if(ESP_OK != ret)
            {
                ESP_LOGE(TAG, "DEVICE model BC08 temp2 error");
            }
        }
    	return ret;
	}
    else if(DEVICE_BC06 == GLOBAL_STATE->device_model)
	{
		ret = TMP75_init(TMP75_I2CADDR_BC08_1, 0);
		ret = TMP75_installed(0);
		if(ESP_OK != ret)
		{
			ESP_LOGE(TAG, "DEVICE model BC06 identify mismatch, reset model to BC04");
			nvs_config_set_string(NVS_CONFIG_DEVICE_MODEL, "BC04");
			vTaskDelay(pdMS_TO_TICKS(1000));
			restart_with_reason("BC06 model identify mismatch, reset to BC04");
		}
        else
        {
            ret = TMP75_init(TMP75_I2CADDR_BC08_2, 1);
            ret = TMP75_installed(1);
            if(ESP_OK != ret)
            {
                ESP_LOGE(TAG, "DEVICE model BC06 temp2 error");
            }
        }
    	return ret;
	}   
    else
    {
		ESP_LOGE(TAG, "DEVICE model %s unknow, reset model to DC04", GLOBAL_STATE->device_model_str);
		nvs_config_set_string(NVS_CONFIG_DEVICE_MODEL, "DC04");
		vTaskDelay(pdMS_TO_TICKS(1000));
		restart_with_reason("Unknown device model, reset to DC04");
		return ret;
	}

    return ret;
}

esp_err_t read_hash_board_temperature(GlobalState *GLOBAL_STATE)
{
    esp_err_t ret = ESP_OK;

    GLOBAL_STATE->HEALTH_MODULE.board_temperature[0] = TMP75_read_temperature(0);
    if(DEVICE_BC08 == GLOBAL_STATE->device_model || DEVICE_BC06 == GLOBAL_STATE->device_model)
        GLOBAL_STATE->HEALTH_MODULE.board_temperature[1] = TMP75_read_temperature(1);
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
