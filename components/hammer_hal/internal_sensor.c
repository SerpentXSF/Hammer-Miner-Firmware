#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "internal_sensor.h"

static  temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-30, 50);
static  temperature_sensor_handle_t temp_handle = NULL;

static const char *TAG = "internal_sensor";

void start_internal_temperature_sensor()
{
    ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_handle));
    
    ESP_LOGI(TAG, "Enable temperature sensor");
    ESP_ERROR_CHECK(temperature_sensor_enable(temp_handle));

    ESP_LOGI(TAG, "start internal temperature sensor");
}

esp_err_t read_internal_temperature_sensor(float *temperature)
{
    esp_err_t err = ESP_FAIL;
    
    if(NULL != temperature && NULL != temp_handle)
        err = temperature_sensor_get_celsius(temp_handle, temperature);

    if(err == ESP_OK){
        ESP_LOGD(TAG, "Temperature: %.2f °C", *temperature);
    }else{
        ESP_LOGW(TAG, "Failed to read temperature");
    }

    return err;
}

void stop_internal_temperature_sensor()
{
    temperature_sensor_disable(temp_handle);
    temp_handle = NULL;

    ESP_LOGI(TAG, "stop internal temperature sensor");
}