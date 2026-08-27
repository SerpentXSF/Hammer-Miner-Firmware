#ifndef GPIO_INPUT_OUTPUT_H
#define GPIO_INPUT_OUTPUT_H

#include "esp_system.h"
#include "miner.h"
#include "driver/gpio.h"

#define GPIO_RED_PIN    CONFIG_GPIO_RED_LED
#define GPIO_GREEN_PIN  CONFIG_GPIO_GREEN_LED

#define GPIO_BLUE_PIN   CONFIG_GPIO_BLUE_LED
#define GPIO_YELLOW_PIN CONFIG_GPIO_YELLOW_LED

#define GPIO_CHAIN_0_PLUG_PIN   CONFIG_GPIO_CHAIN_0_PLUG
#define GPIO_CHAIN_1_PLUG_PIN   CONFIG_GPIO_CHAIN_1_PLUG

#define GPIO_HASHBOARD0_ENABLE  39
#define GPIO_HASHBOARD1_ENABLE  

extern SemaphoreHandle_t led_mutex;

typedef enum
{
    GREEN_RED_LED_OFF,
    GREEN_LED_ON,
    RED_LED_ON,
    GREEN_RED_LED_ON,
    GREEN_LED_BLINK,
    RED_LED_BLINK,
    GREEN_RED_LED_BLINK,
    GREEN_RED_LED_SYNC_BLINK
}LedOperation;

typedef enum
{
    BLUE_YELLOW_LED_OFF = 0,
    BLUE_LED_ON,
    YELLOW_LED_ON,
    BLUE_YELLOW_LED_ON,
    BLUE_LED_BLINK,
    YELLOW_LED_BLINK,
    BLUE_YELLOW_LED_BLINK,
    BLUE_YELLOW_LED_SYNC_BLINK
}AuxziliaryLedOperation;

typedef enum
{
    MSG_NORMAL_BOOTING = 0,
    MSG_NORMAL_MINING,
    MSG_BOOT_UP_ERROR,
    MSG_FAN_ERROR,
    MSG_EEPROM_ERROR,
    MSG_NETWORK_ERROR,
    MSG_HASHBOARD_ERROR,
    MSG_CONTROLBOARD_ERROR,
    MSG_HIGH_ENVIROMENT_TEMPERATURE,
    MSG_LOCATION,
    MSG_LOCATION_DONE,
    MSG_WIFI_CONNECTING,
    MSG_WIFI_CONNECTED,
    MSG_STATUS_UNKNOWN    
}LedMessage;

esp_err_t led_init(int led_pin);
esp_err_t led_on(int led_pin);
esp_err_t led_off(int led_pin);
esp_err_t led_blink(int led_pin);
esp_err_t all_led_blink();

esp_err_t reset_pin_init(uint32_t chain_num);
esp_err_t reset_pin_low(uint32_t chain_num);
esp_err_t reset_pin_high(uint32_t chain_num);

esp_err_t hash_board_3v3_ctrl_init();
esp_err_t hash_board_3v3_ctrl_enable();

void inform_led(LedMessage led_msg);
void show_led();

esp_err_t gpio_output_init(uint8_t gpio_pin);
esp_err_t gpio_output(uint8_t gpio_pin, uint8_t level);

void debounce_ip_reporter_timer_callback(TimerHandle_t xTimer);
void IRAM_ATTR buttonIpReporterIsrHandler(void *arg);
esp_err_t gpio_input_init(uint8_t gpio_pin, gpio_isr_t isr_handler, 
    TimerCallbackFunction_t callback);

esp_err_t gpio_input_init_simple(uint8_t gpio_pin);
esp_err_t plug_pin_init(uint8_t chain_num);
bool check_plug_in(uint8_t chain_num);

#endif