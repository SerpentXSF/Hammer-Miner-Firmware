#include "unity.h"
#include "esp_log.h"

#include "gpio_input_output.h"

static const char* TAG = "gpio_input_output";

extern int uart_command_param[4];

TEST_CASE("start_led_blink", "gpio_test")
{
    led_blink(GPIO_RED_PIN);
    led_blink(GPIO_GREEN_PIN);
    led_blink(GPIO_RED_PIN);
    led_blink(GPIO_GREEN_PIN);
    led_blink(GPIO_RED_PIN);
    led_blink(GPIO_GREEN_PIN);
    
    ESP_LOGI(TAG, "led start to blink.");
}

TEST_CASE("stop_led_blink", "gpio_test")
{
    led_off(GPIO_RED_PIN);
    led_off(GPIO_GREEN_PIN);

    ESP_LOGI(TAG, "led stop to blink.");
}

TEST_CASE("all_led_test", "gpio_test")
{
    /*
    led_blink(GPIO_RED_PIN);
    led_blink(GPIO_BLUE_PIN);
    led_blink(GPIO_YELLOW_PIN);
    led_blink(GPIO_GREEN_PIN);
    */
    all_led_blink();
    volc_delay(1000);
    all_led_blink();
    ESP_LOGI(TAG, "LED TEST is done.");
}

TEST_CASE("detect_input_pin", "gpio_test")
{
    /*TODO： the lvs library is needed to porting.*/
    ESP_LOGI(TAG, "Detect input pin is empty.");
}

TEST_CASE("reset_pin_output", "gpio_test")
{
    ESP_LOGI(TAG, "reset_pin_output %d", uart_command_param[0]);

    if(1 == uart_command_param[0]){
        reset_pin_high(0);
        reset_pin_high(1);
    }else if(0 == uart_command_param[1]){
        reset_pin_low(0);
        reset_pin_low(1);
    }
}