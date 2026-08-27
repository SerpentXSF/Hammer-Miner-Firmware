#include "unity.h"
#include "esp_log.h"
#include "esp_err.h"
#include "string.h"

#include "eeprom.h"
#include "gpio_input_output.h"

static const char *TAG = "eeprom_test";

extern int uart_command_param[4];

uint8_t factory_sn[24];
uint8_t eeprom_index;

TEST_CASE("eeprom_read", "eeprom_test")
{   
    ESP_LOGI(TAG, "eeprom_read start.");

    /*read the page, and print the page.*/
    uint8_t buf[64];
    TEST_ASSERT_EQUAL_INT32(ESP_OK, eeprom_page_read(uart_command_param[0], buf));
    ESP_LOG_BUFFER_HEX(TAG, buf, 64);
    /*parse the page.*/

}

TEST_CASE("eeprom_write", "eeprom_test")
{
    /*construct the eeprom page.*/
    /*write the page.*/
    uint8_t buf[64];
    memset(buf, 0xa5, sizeof(buf));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, eeprom_page_write(uart_command_param[0], buf));
}

TEST_CASE("eeprom_write_factory_info", "eeprom_test")
{
    TEST_ASSERT_EQUAL_INT32(0, testWriteEpromData(uart_command_param[0]));
}

/*
version:0,algorithm:0,chip_bin:42,pcb_version:1,sn:0,voltage:490,freq:1800
*/
TEST_CASE("eeprom_read_data", "eeprom_test")
{
    TEST_ASSERT_EQUAL_INT32(0, eeprom_read_data(uart_command_param[0]));
}

/*
factory_write_sn 0 B66A250901672
factory_write_sn 1 B66A250901764
*/
TEST_CASE("factory_write_sn", "eeprom_test")
{
    ESP_LOGI(TAG, "eeprom %d, write sn %s", eeprom_index, (char *)factory_sn);
    TEST_ASSERT_EQUAL_INT32(0, update_sn_to_eeprom(eeprom_index, factory_sn));
    
    memset(factory_sn, 0, sizeof(factory_sn));
    get_sn_from_eeprom(eeprom_index, factory_sn);
    ESP_LOGI(TAG, "Read back sn ");
    esp_log_buffer_char(TAG, factory_sn, sizeof(factory_sn));
}

/*
factory_set_bin_type 11
factory_set_bin_type 23
*/
TEST_CASE("factory_set_bin_type", "eeprom_test")
{
    TEST_ASSERT_LESS_OR_EQUAL_INT32(100, uart_command_param[0]);
    TEST_ASSERT_GREATER_THAN_INT32(0, uart_command_param[0]);
    factory_set_bin_type(uart_command_param[0]);
}

/*
TODO: test eeprom_write_data
*/