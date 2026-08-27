#include <stdbool.h>

#include "esp_log.h"
#include "esp_check.h"

#include "miner.h"
#include "hal_i2c.h"
#include "TMP75.h"

static const char *TAG = "TMP75";

static i2c_master_dev_handle_t tmp75_dev_handles[TEMPERATURE_SENSOR_MAX_NUM];


esp_err_t TMP75_init(uint8_t slave_addr, int temperature_sensor_index) 
{
    return hammer_i2c_add_device(slave_addr, tmp75_dev_handles+temperature_sensor_index, TAG);
}

esp_err_t TMP75_installed(int temperature_sensor_index)
{
    uint8_t data[2] = {0, 0};
    esp_err_t result = ESP_FAIL;

    // read the configuration register
    ESP_LOGI(TAG, "Reading configuration register");
    result = hammer_i2c_register_read(tmp75_dev_handles[temperature_sensor_index], TMP75_CONFIG_REG, data, 2);
    ESP_LOGI(TAG, "result %X Configuration[%d] = %02X %02X", result, temperature_sensor_index, data[0], data[1]);

    return result;
}

int8_t TMP75_read_temperature(int temperature_sensor_index)
{
    uint8_t data[2] = {0, 0};
    int8_t temperature;

    if(hammer_i2c_register_read(tmp75_dev_handles[temperature_sensor_index], TMP75_TEMP_REG, data, 2))
    {
        ESP_LOGW(TAG, "Read Temperature fail");
        return -60;
    }
    ESP_LOGD(TAG, "Raw Temperature = %02X %02X", data[0], data[1]);
    if(data[0] & 0x80){
        temperature = (int8_t)data[0];
    }else{
        temperature = data[0] & 0x7f;
    }
    ESP_LOGD(TAG, "Temperature[%d] = %"PRId8"", temperature_sensor_index, temperature);

    return temperature;
}

