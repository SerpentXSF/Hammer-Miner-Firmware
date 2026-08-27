#include "unity.h"
#include "esp_log.h"

#include "power.h"
#include "miner.h"
#include "volc_i2c.h"

static const char *TAG = "power_test";

extern int uart_command_param[4];
/*
all_power_test 480
*/
TEST_CASE("all_power_test", "power_test")
{
    uint8_t i2c_master_index = I2C_MASTER_INDEX_OF_POWER;

    TEST_ASSERT_EQUAL(ESP_OK, power_detect_type(i2c_master_index));
    TEST_ASSERT_EQUAL(ESP_OK, power_on());
    TEST_ASSERT_EQUAL(ESP_OK, power_set_voltage(uart_command_param[1]));
    volc_delay(3000);

    ESP_LOGI(TAG, "output voltage %f", power_get_output_voltage());
    ESP_LOGI(TAG, "output current %f", power_get_output_current());
    ESP_LOGI(TAG, "output power %f", power_get_output_power());

}


