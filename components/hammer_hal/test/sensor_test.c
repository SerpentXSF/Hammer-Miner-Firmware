#include "unity.h"
#include "esp_log.h"

#include "external_sensor.h"
#include "gpio_input_output.h"

static const char *TAG = "sensor_test";

extern int uart_command_param[4];

/*
tmp75 on cotrol board : 0, "mini_test_temperature_sensor_on_control_board"
*/
TEST_CASE("mini_test_temperature_sensor_on_control_board", "sensor_test")
{
    TMP75_installed(MINI_CONTROLBOARD_TEMPERATURE_SENSOR_INDEX);
    
    ESP_LOGI(TAG, "read control board temperature %hhd", 
        TMP75_read_temperature(MINI_CONTROLBOARD_TEMPERATURE_SENSOR_INDEX)
    );
}

/*
tmp75 on cotrol board : 0, 
tmp75 on hashboard 0: 1, "mini_test_temperature_sensor_on_hash_board 1"
tmp75 on hashboard 1: 2, "mini_test_temperature_sensor_on_hash_board 2"
*/
TEST_CASE("mini_test_temperature_sensor_on_hash_board", "sensor_test")
{
    gpio_output_init(39);
    gpio_output(39, 0);
    volc_delay(100);

    TMP75_installed(uart_command_param[0]);
    ESP_LOGI(TAG, "read hashboard %d temperature %hhd",
        uart_command_param[0], TMP75_read_temperature(uart_command_param[0])
    );
}

/*
ina230 on hashboard 0: "mini_test_voltage_sensor_on_hash_board 0 480 50"
ina230 on hashboard 1: "mini_test_voltage_sensor_on_hash_board 1 480 50"
*/
TEST_CASE("mini_test_voltage_sensor_on_hash_board", "sensor_test")
{
    gpio_output(GPIO_HASHBOARD_3V3_CTRL, 0);
    volc_delay(100);
    
    INA230_installed(uart_command_param[0]);
    float ret_vol = INA230_read_voltage(uart_command_param[0]);

    ESP_LOGI(TAG, "read hashboard %d voltage %f,  voltage calibration %d", 
        uart_command_param[0],
        ret_vol, uart_command_param[2]
    );
    TEST_ASSERT_GREATER_OR_EQUAL(uart_command_param[1] - 10 - uart_command_param[2], (int)(100*ret_vol));
    //TEST_ASSERT_LESS_OR_EQUAL(uart_command_param[1] + 10 - uart_command_param[2], (int)(100 * ret_vol));

    ESP_LOGI(TAG, "read hashboard %d current %f",
        uart_command_param[0], 
        INA230_read_current(uart_command_param[0])
    );
}
