#include "unity.h"
#include "esp_log.h"

#include "pwm_fan.h"

static const char *TAG = "fan_test";

extern int uart_command_param[4];

/*
check_fan_config 0  0
check_fan_config 20 1500
check_fan_config 50 2000
check_fan_config 70 3000
check_fan_config 100 4000

max pulse counter is about 1500.
*/
TEST_CASE("check_fan_config", "fan_test")
{
    esp_err_t ret = ESP_FAIL;
    int pulse_counter[2] = {0, 0};
    int pwm_config = uart_command_param[0];
    int expected_fan_rpm = uart_command_param[1];
    int ret_rpm0 = 0, ret_rpm1 = 0;

    /*set the pwm*/
    ret = ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_set_pwm(LEDC_CHANNEL_0, pwm_config));
    ret = ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_set_pwm(LEDC_CHANNEL_1, pwm_config));
    ret = ESP_ERROR_CHECK_WITHOUT_ABORT(fan_pcnts_clear_counter(LEDC_CHANNEL_0));
    ret = ESP_ERROR_CHECK_WITHOUT_ABORT(fan_pcnts_clear_counter(LEDC_CHANNEL_1));

    for(int i = 0; i < 1; i++){
        /*sleep 5s*/
        vTaskDelay(pdMS_TO_TICKS(8000));
        /*check the fan rpm. */
        ret = ESP_ERROR_CHECK_WITHOUT_ABORT(fan_pcnts_get_counter(LEDC_CHANNEL_0, pulse_counter));
        ret = ESP_ERROR_CHECK_WITHOUT_ABORT(fan_pcnts_get_counter(LEDC_CHANNEL_1, pulse_counter+1));
        ret_rpm0 =  pulse_counter[0]*60/8/2;
        ret_rpm1 = pulse_counter[1]*60/8/2;
        ESP_LOGI(TAG, "Fan_speed is %d %d.", ret_rpm0, ret_rpm1);
        TEST_ASSERT_GREATER_OR_EQUAL(expected_fan_rpm, ret_rpm0);
        TEST_ASSERT_GREATER_OR_EQUAL(expected_fan_rpm, ret_rpm1);
    }
    /*
    vTaskDelay(pdMS_TO_TICKS(1000));
    ret = ESP_ERROR_CHECK_WITHOUT_ABORT(fan_pcnts_get_counter(pulse_counter));
    ESP_LOGI(TAG, "Fan_speed is %d %d.", pulse_counter[0]*12/2, pulse_counter[1]*12/2);

    vTaskDelay(pdMS_TO_TICKS(1000));
    ret = ESP_ERROR_CHECK_WITHOUT_ABORT(fan_pcnts_get_counter(pulse_counter));   
    ESP_LOGI(TAG, "Fan_speed is %d %d.", pulse_counter[0]*12/2, pulse_counter[1]*12/2);

    vTaskDelay(pdMS_TO_TICKS(1000));
    ret = ESP_ERROR_CHECK_WITHOUT_ABORT(fan_pcnts_get_counter(pulse_counter));
    ESP_LOGI(TAG, "Fan_speed is %d %d.", pulse_counter[0]*12/2, pulse_counter[1]*12/2);

    vTaskDelay(pdMS_TO_TICKS(1000));
    ret = ESP_ERROR_CHECK_WITHOUT_ABORT(fan_pcnts_get_counter(pulse_counter));
    ESP_LOGI(TAG, "Fan_speed is %d %d.", pulse_counter[0]*12/2, pulse_counter[1]*12/2);
    */
}

TEST_CASE("long_time_check_fan_config", "fan_test")
{
    esp_err_t ret = ESP_FAIL;
    int pulse_counter[2] = {0, 0};
    int pwm_config = uart_command_param[0];
    int expected_fan_rpm = uart_command_param[1];
    int ret_rpm0 = 0, ret_rpm1 = 0;

    /*set the pwm*/
    ret = ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_set_pwm(LEDC_CHANNEL_0, pwm_config));
    ret = ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_set_pwm(LEDC_CHANNEL_1, pwm_config));
    ret = ESP_ERROR_CHECK_WITHOUT_ABORT(fan_pcnts_clear_counter(LEDC_CHANNEL_0));
    ret = ESP_ERROR_CHECK_WITHOUT_ABORT(fan_pcnts_clear_counter(LEDC_CHANNEL_1));

    for(int i = 0; i < 20; i++){
        /*sleep 5s*/
        vTaskDelay(pdMS_TO_TICKS(8000));
        /*check the fan rpm. */
        ret = ESP_ERROR_CHECK_WITHOUT_ABORT(fan_pcnts_get_counter(LEDC_CHANNEL_0, pulse_counter));
        ret = ESP_ERROR_CHECK_WITHOUT_ABORT(fan_pcnts_get_counter(LEDC_CHANNEL_1, pulse_counter+1));
        ret_rpm0 =  pulse_counter[0]*60/8/2;
        ret_rpm1 = pulse_counter[1]*60/8/2;
        ESP_LOGI(TAG, "Fan_speed is %d %d.", ret_rpm0, ret_rpm1);
        TEST_ASSERT_GREATER_OR_EQUAL(expected_fan_rpm, ret_rpm0);
        TEST_ASSERT_GREATER_OR_EQUAL(expected_fan_rpm, ret_rpm1);
    }
    /*
    vTaskDelay(pdMS_TO_TICKS(1000));
    ret = ESP_ERROR_CHECK_WITHOUT_ABORT(fan_pcnts_get_counter(pulse_counter));
    ESP_LOGI(TAG, "Fan_speed is %d %d.", pulse_counter[0]*12/2, pulse_counter[1]*12/2);

    vTaskDelay(pdMS_TO_TICKS(1000));
    ret = ESP_ERROR_CHECK_WITHOUT_ABORT(fan_pcnts_get_counter(pulse_counter));   
    ESP_LOGI(TAG, "Fan_speed is %d %d.", pulse_counter[0]*12/2, pulse_counter[1]*12/2);

    vTaskDelay(pdMS_TO_TICKS(1000));
    ret = ESP_ERROR_CHECK_WITHOUT_ABORT(fan_pcnts_get_counter(pulse_counter));
    ESP_LOGI(TAG, "Fan_speed is %d %d.", pulse_counter[0]*12/2, pulse_counter[1]*12/2);

    vTaskDelay(pdMS_TO_TICKS(1000));
    ret = ESP_ERROR_CHECK_WITHOUT_ABORT(fan_pcnts_get_counter(pulse_counter));
    ESP_LOGI(TAG, "Fan_speed is %d %d.", pulse_counter[0]*12/2, pulse_counter[1]*12/2);
    */
}