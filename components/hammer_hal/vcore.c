#include <stdio.h>
#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "esp_check.h"
#include "driver/gpio.h"

#include "global_state.h"
#include "vcore.h"
#include "device.h"

static const char *TAG = "vcore";

#define CONFIG_GPIO_PLUG_SENSE      11
#define GPIO_PLUG_SENSE  CONFIG_GPIO_PLUG_SENSE

// nominal voltage settings
#define NOMINAL_VOLTAGE_5 5 //volts
#define NOMINAL_VOLTAGE_12 12//volts

static TPS546_CONFIG TPS546_CONFIG_LOTTO = {
    /* vin voltage */
    .TPS546_INIT_VIN_ON = 11.0,
    .TPS546_INIT_VIN_OFF = 10.5,
    .TPS546_INIT_VIN_UV_WARN_LIMIT = 11.0, //Set to 0 to ignore. TI Bug in this register
    .TPS546_INIT_VIN_OV_FAULT_LIMIT = 14.0,
    /* vout voltage */
    .TPS546_INIT_SCALE_LOOP = 0.5,
    .TPS546_INIT_VOUT_MIN = 1,
    .TPS546_INIT_VOUT_MAX = 1.3,
    .TPS546_INIT_VOUT_COMMAND = 1.0,
    /* iout current */
    .TPS546_INIT_IOUT_OC_WARN_LIMIT = 50.00, /* A */
    .TPS546_INIT_IOUT_OC_FAULT_LIMIT = 55.00 /* A */  
};

esp_err_t VCORE_init(GlobalState * GLOBAL_STATE) {
    esp_err_t ret = ESP_OK;

    // configure plug sense, if present
    // Configure plug sense pin as input(barrel jack) 1 is plugged in
    gpio_config_t barrel_jack_conf = {
        .pin_bit_mask = (1ULL << GPIO_PLUG_SENSE),
        .mode = GPIO_MODE_INPUT,
    };
    gpio_config(&barrel_jack_conf);
    int barrel_jack_plugged_in = gpio_get_level(GPIO_PLUG_SENSE);
    ESP_LOGI(TAG, "TPS546 power good %d", barrel_jack_plugged_in);

    TPS546_CONFIG_LOTTO.TPS546_INIT_VOUT_MIN = (float)GLOBAL_STATE->asic_vol_min/100;
    TPS546_CONFIG_LOTTO.TPS546_INIT_VOUT_MAX = (float)GLOBAL_STATE->asic_vol_max/100;

    /*
     * VOUT_SCALE_LOOP has to bracket the requested ceiling, or the
     * regulator cannot represent the voltage at all:
     *
     *     scale 1      0.25 - 0.75 V
     *     scale 0.5    0.25 - 1.5  V
     *     scale 0.25   0.25 - 3    V
     *     scale 0.125  0.25 - 6    V
     *
     * The struct initialises to 0.5, which covers a single-ASIC domain at
     * 1.19 V, so the ceilings at or below 1.5 V need no adjustment here.
     */
    if(TPS546_CONFIG_LOTTO.TPS546_INIT_VOUT_MAX > 1.5)
    {
        /*
        VOUT_SCALE_LOOP = 1     0.25    0.75
        VOUT_SCALE_LOOP = 0.5   0.25    1.5
        VOUT_SCALE_LOOP = 0.25  0.25    3
        VOUT_SCALE_LOOP = 0.125 0.25    6
        */
        if(TPS546_CONFIG_LOTTO.TPS546_INIT_VOUT_MAX > 3.0)
        {
            TPS546_CONFIG_LOTTO.TPS546_INIT_SCALE_LOOP = 0.125;
        }
        else
        {
            TPS546_CONFIG_LOTTO.TPS546_INIT_SCALE_LOOP = 0.25;
        }
    }

    ESP_RETURN_ON_ERROR(TPS546_init(TPS546_CONFIG_LOTTO), TAG, "TPS546 init failed!");

    /*update the parameter*/
    VCORE_get_nominal_voltage(GLOBAL_STATE);
    VCORE_get_max_power(GLOBAL_STATE);

    return ret;
}

esp_err_t VCORE_set_voltage(float core_voltage)
{
    ESP_LOGI(TAG, "Set ASIC voltage = %.2fV", core_voltage);
    ESP_RETURN_ON_ERROR(TPS546_set_vout(core_voltage), TAG, "TPS546 set voltage failed!");

    // TODO: configure plug sense, if present

    return ESP_OK;
}

int16_t VCORE_get_voltage_mv(void) 
{
    return TPS546_get_vout() * 1000;
}

int VCORE_get_temp(void) 
{
    return TPS546_get_temperature();
}

esp_err_t VCORE_check_fault(GlobalState * GLOBAL_STATE) 
{
    ESP_RETURN_ON_ERROR(TPS546_check_status(GLOBAL_STATE), TAG, "TPS546 check status failed!");

    return ESP_OK;
}

const char* VCORE_get_fault_string(GlobalState * GLOBAL_STATE) 
{
    return TPS546_get_error_message();
}

esp_err_t VCORE_update_power(GlobalState * GLOBAL_STATE)
{
    GLOBAL_STATE->HEALTH_MODULE.out_current = TPS546_get_iout()*1000.0;
    GLOBAL_STATE->HEALTH_MODULE.input_voltage = TPS546_get_vin();
    GLOBAL_STATE->HEALTH_MODULE.out_voltage = TPS546_get_vout();
    GLOBAL_STATE->HEALTH_MODULE.power = TPS546_get_vout() * GLOBAL_STATE->HEALTH_MODULE.out_current / 1000.0;

    ESP_LOGD(TAG, "TPS546 vin %f vout %f iout %f, power %f",
            GLOBAL_STATE->HEALTH_MODULE.input_voltage,
            GLOBAL_STATE->HEALTH_MODULE.out_voltage,
            GLOBAL_STATE->HEALTH_MODULE.out_current,
            GLOBAL_STATE->HEALTH_MODULE.power
            );

    return ESP_OK;
}

esp_err_t VCORE_get_nominal_voltage(GlobalState * GLOBAL_STATE)
{
    GLOBAL_STATE->HEALTH_MODULE.nominal_input_voltage = NOMINAL_VOLTAGE_12;

    return ESP_OK;
}

esp_err_t VCORE_get_max_power(GlobalState * GLOBAL_STATE)
{
    GLOBAL_STATE->HEALTH_MODULE.max_power = 1.2*40;

    return ESP_OK;
}
