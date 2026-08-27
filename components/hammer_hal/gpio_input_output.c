#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "gpio_input_output.h"
#include "protocol_task.h"

static const char *TAG = "gpio_input_output";

esp_err_t led_init(int led_pin)
{
    esp_err_t ret = ESP_FAIL;
    /*
    ret = gpio_reset_pin(led_pin);
    ret = gpio_set_direction(led_pin, GPIO_MODE_OUTPUT);
    */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << led_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    ret = gpio_config(&io_conf);

    return ret;
}

esp_err_t led_on(int led_pin)
{
    return gpio_set_level(led_pin, 1);
}

esp_err_t led_off(int led_pin)
{
    return gpio_set_level(led_pin, 0);
}

esp_err_t led_blink(int led_pin)
{
    esp_err_t ret = ESP_FAIL;
    
    ret = led_on(led_pin);
    vTaskDelay(1000/portTICK_PERIOD_MS);
    ret = led_off(led_pin);

    return ret;
}

esp_err_t all_led_blink()
{
    esp_err_t ret = ESP_FAIL;

    ret = led_off(GPIO_RED_PIN);
    ret = led_off(GPIO_GREEN_PIN);
    ret = led_off(GPIO_BLUE_PIN);
    ret = led_off(GPIO_YELLOW_PIN);

    vTaskDelay(1000/portTICK_PERIOD_MS);
    
    ret = led_on(GPIO_RED_PIN);
    ret = led_on(GPIO_GREEN_PIN);
    ret = led_on(GPIO_BLUE_PIN);
    ret = led_on(GPIO_YELLOW_PIN);

    return ret;    
}

esp_err_t reset_pin_init(uint32_t chain_num)
{
    esp_err_t ret = ESP_FAIL;
    int gpio_pin = 0;

    if(0 == chain_num){
        gpio_pin = GPIO_RESET_CHAIN_0;
    }else if(1 == chain_num){
        gpio_pin = GPIO_RESET_CHAIN_1;
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    ret = gpio_config(&io_conf);

    return ret;
}

esp_err_t reset_pin_low(uint32_t chain_num)
{
    int gpio_pin = 0;

    if(0 == chain_num){
        gpio_pin = GPIO_RESET_CHAIN_0;
    }else if(1 == chain_num){
        gpio_pin = GPIO_RESET_CHAIN_1;
    }

    return gpio_set_level(gpio_pin, 0);    
}

esp_err_t reset_pin_high(uint32_t chain_num)
{
    int gpio_pin = 0;

    if(0 == chain_num){
        gpio_pin = GPIO_RESET_CHAIN_0;
    }else if(1 == chain_num){
        gpio_pin = GPIO_RESET_CHAIN_1;
    }

    return gpio_set_level(gpio_pin, 1);    
}

esp_err_t gpio_output_init(uint8_t gpio_pin)
{
    esp_err_t ret = ESP_FAIL;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    ret = gpio_config(&io_conf);

    return ret;
}

esp_err_t gpio_output(uint8_t gpio_pin, uint8_t level)
{
    return gpio_set_level(gpio_pin, level);
}

esp_err_t hash_board_3v3_ctrl_init()
{
    #if GPIO_HASHBOARD_3V3_CTRL == 255
    return ESP_FAIL;
    #else
    return gpio_output_init(GPIO_HASHBOARD_3V3_CTRL);
    #endif
}

esp_err_t hash_board_3v3_ctrl_enable()
{
    #if GPIO_HASHBOARD_3V3_CTRL == 255
    return ESP_FAIL;
    #else
    return gpio_output(GPIO_HASHBOARD_3V3_CTRL, 0);
    #endif
}

static LedOperation g_led_status = GREEN_RED_LED_OFF;
static AuxziliaryLedOperation g_aux_led_status = BLUE_YELLOW_LED_OFF;
SemaphoreHandle_t led_mutex;

void inform_led(LedMessage led_msg)
{  
    if(!xSemaphoreTake(led_mutex, pdMS_TO_TICKS(2000))){
        ESP_LOGW(TAG, "Fail to Change led status.");
        return;
    }

    switch (led_msg)
    {
        case MSG_STATUS_UNKNOWN:
            g_led_status = GREEN_RED_LED_OFF;
            break;
        case MSG_NORMAL_BOOTING:
            g_led_status = GREEN_LED_ON;
            break;
        case MSG_NORMAL_MINING:
            g_led_status = GREEN_LED_BLINK;
            break;
        case MSG_EEPROM_ERROR:
            g_led_status = RED_LED_BLINK;
            break;        
        case MSG_BOOT_UP_ERROR:
        case MSG_FAN_ERROR:
        case MSG_HASHBOARD_ERROR:
        case MSG_CONTROLBOARD_ERROR:
            g_led_status = RED_LED_ON;
            break;
        case MSG_NETWORK_ERROR:
        case MSG_HIGH_ENVIROMENT_TEMPERATURE:
            g_led_status = GREEN_RED_LED_BLINK;
            break;
        case MSG_LOCATION:
            g_aux_led_status = BLUE_YELLOW_LED_SYNC_BLINK;
            break;
        case MSG_LOCATION_DONE:
            g_aux_led_status = BLUE_YELLOW_LED_OFF;
            break;
        case MSG_WIFI_CONNECTING:
            g_aux_led_status = YELLOW_LED_BLINK;
            break;
        case MSG_WIFI_CONNECTED:
            g_aux_led_status = BLUE_YELLOW_LED_OFF;
            break;

        default:
            break;
    }
    xSemaphoreGive(led_mutex);
}

void show_led()
{
    if(!xSemaphoreTake(led_mutex, pdMS_TO_TICKS(100))){
        ESP_LOGW(TAG, "Fail to Change led status.");
        return;        
    }

    switch (g_led_status)
    {
        case GREEN_LED_ON:
        case GREEN_LED_BLINK:
        case GREEN_RED_LED_BLINK:
            led_on(GPIO_GREEN_PIN);
            break;

        case RED_LED_ON:
        case RED_LED_BLINK:
            led_on(GPIO_RED_PIN);
            break;

        case GREEN_RED_LED_ON:
        case GREEN_RED_LED_SYNC_BLINK:
            led_on(GPIO_GREEN_PIN);
            led_on(GPIO_RED_PIN);
            break;
        case GREEN_RED_LED_OFF:
            led_off(GPIO_GREEN_PIN);
            led_off(GPIO_RED_PIN);
            break;
        default:
            ESP_LOGW(TAG, "unknow led status 1.");
            break;
    }

    switch (g_aux_led_status)
    {
        case BLUE_YELLOW_LED_OFF:
            led_off(GPIO_BLUE_PIN);
            led_off(GPIO_YELLOW_PIN);
            break;
        case YELLOW_LED_BLINK:
            led_on(GPIO_YELLOW_PIN);
            break;
        case BLUE_YELLOW_LED_ON:
        case BLUE_YELLOW_LED_SYNC_BLINK:
            led_on(GPIO_BLUE_PIN);
            led_on(GPIO_YELLOW_PIN);
            break;
        default:
            ESP_LOGW(TAG, "unknow auxiliary led status 1");
            break;
    }

    vTaskDelay(pdMS_TO_TICKS(500));
    
    switch (g_led_status)
    {
        case GREEN_LED_BLINK:
        case RED_LED_BLINK:
            led_off(GPIO_GREEN_PIN);
            led_off(GPIO_RED_PIN);            
            break;
        
        case GREEN_RED_LED_BLINK:
            led_off(GPIO_GREEN_PIN);
            led_on(GPIO_RED_PIN);   
            break;
        
        case GREEN_RED_LED_OFF:
        case GREEN_RED_LED_SYNC_BLINK:
            led_off(GPIO_GREEN_PIN);
            led_off(GPIO_RED_PIN);
            break;
        default:
            break;
    }

    switch(g_aux_led_status)
    {      
        case BLUE_YELLOW_LED_SYNC_BLINK:
            led_off(GPIO_BLUE_PIN);
            led_off(GPIO_YELLOW_PIN);
            break;
        case YELLOW_LED_BLINK:
            led_off(GPIO_YELLOW_PIN);
            break;
        default:
            /*do nothing.*/
            break;
    }

    vTaskDelay(pdMS_TO_TICKS(500));

    switch (g_led_status)
    {
        case GREEN_RED_LED_BLINK:
            led_off(GPIO_RED_PIN);
            break;
        
        default:
            break;
    }

    xSemaphoreGive(led_mutex);
}

extern void ip_reporter_task(void *pvParameters);

#define DEBOUNCE_TIME_MS 10
int last_ip_reporter_state = 1;
extern SemaphoreHandle_t xIpReporterSemaphore;

void debounce_ip_reporter_timer_callback(TimerHandle_t xTimer) 
{
    bool current_state = gpio_get_level(GPIO_IP_REPORTER);
    
    if(current_state != last_ip_reporter_state) {
        last_ip_reporter_state = current_state;
    }
    //esp_rom_printf("IP reporter button: %s\n", last_ip_reporter_state ? "released" : "pressed");

    if(!last_ip_reporter_state){
        if (pdTRUE == xSemaphoreGive(xIpReporterSemaphore)){
            esp_rom_printf("Inform the ip reporter task.\n");
        }else{
            esp_rom_printf("Fail to inform the ip reporter task.\n");
        }
    }
}

void IRAM_ATTR buttonIpReporterIsrHandler(void *arg) 
{
    last_ip_reporter_state = gpio_get_level(GPIO_IP_REPORTER);
    //esp_rom_printf("IP Reporter isr %d.\n", last_ip_reporter_state);
    xTimerStartFromISR((TimerHandle_t)arg, NULL);
}

esp_err_t gpio_input_init(uint8_t gpio_pin, gpio_isr_t isr_handler, 
                            TimerCallbackFunction_t callback)
{
    esp_err_t ret = ESP_FAIL;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
        .pull_down_en = 1,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    ret = gpio_config(&io_conf);
    
    TimerHandle_t timer;
    timer = xTimerCreate(
        "debouce_button_timer",
        pdMS_TO_TICKS(DEBOUNCE_TIME_MS),
        pdFALSE,        // 不自动重载
        NULL,
        callback
    );

    gpio_install_isr_service(0);
    gpio_isr_handler_add(gpio_pin, isr_handler, (void*)timer);

    return ret;
}

esp_err_t gpio_input_init_simple(uint8_t gpio_pin)
{
    esp_err_t ret = ESP_FAIL;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    ret = gpio_config(&io_conf);

    return ret;
}

esp_err_t plug_pin_init(uint8_t chain_num)
{
    uint8_t gpio_pin = 255;
    
    if(0 == chain_num){
        gpio_pin = GPIO_CHAIN_0_PLUG_PIN;
    }else if(1 == chain_num){
        gpio_pin = GPIO_CHAIN_1_PLUG_PIN;
    }
    
    return gpio_input_init_simple(gpio_pin);
}

bool check_plug_in(uint8_t chain_num)
{
    uint8_t gpio_pin = 255;
    
    if(0 == chain_num){
        gpio_pin = GPIO_CHAIN_0_PLUG_PIN;
    }else if(1 == chain_num){
        gpio_pin = GPIO_CHAIN_1_PLUG_PIN;
    }

    if(1 == gpio_get_level(gpio_pin)){
        return true;
    }else{
        return false;
    }
}