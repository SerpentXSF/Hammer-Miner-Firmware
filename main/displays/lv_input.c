#include "esp_log.h"
#include "esp_check.h"
#include "lvgl.h"
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "lv_input.h"
#include "lvgl_screen.h"
#include "lilygo_porting.h"

#define DEBOUNCE_TIME_MS 10

static const char * TAG = "input";

static TimerHandle_t debounce_next_button_timer, debounce_display_button_timer;
int last_next_button_state = 1;
int last_display_button_state = 1;

extern QueueHandle_t screen_msg_queue;

static void debounce_next_button_timer_callback(TimerHandle_t xTimer) 
{
    bool current_state = gpio_get_level(PIN_BUTTON_NEXT);
    
    if(current_state != last_next_button_state) {
        last_next_button_state = current_state;
    }
#ifdef LVGL_PORTING_DEBUG    
    esp_rom_printf("Next button: %s\n", last_next_button_state ? "released" : "pressed");
#endif

    if(!last_next_button_state){
        screen_msg msg = {.msg_type = NEXT_KEY_PREESSED};
        xQueueSendFromISR(screen_msg_queue, &msg, 0);
    }
}

static void IRAM_ATTR buttonNextIsrHandler(void *arg) 
{
    last_next_button_state = gpio_get_level(PIN_BUTTON_NEXT);
#ifdef LVGL_PORTING_DEBUG    
    esp_rom_printf("Next button isr %d.\n", last_next_button_state);
#endif
    xTimerStartFromISR(debounce_next_button_timer, NULL);
}

static void IRAM_ATTR buttonNextIsrHandlerSimple(void *arg)
{
    screen_msg msg = {.msg_type = NEXT_KEY_PREESSED};
    xQueueSendFromISR(screen_msg_queue, &msg, 0);    
}

static void IRAM_ATTR buttonDisplayOnIsrHandlerSimple(void *arg) 
{
    screen_msg msg = {.msg_type = DISPLAY_KEY_PREESSED};
    xQueueSendFromISR(screen_msg_queue, &msg, 0);
}

static void debounce_display_button_timer_callback(TimerHandle_t xTimer) 
{
    bool current_state = gpio_get_level(PIN_BUTTON_DISPLAY_ON);
    
    if(current_state != last_display_button_state) {
        last_display_button_state = current_state;
    }
#ifdef LVGL_PORTING_DEBUG
    esp_rom_printf("Display button: %s\n", last_display_button_state ? "released" : "pressed");
#endif

    if(!last_display_button_state){
        screen_msg msg = {.msg_type = DISPLAY_KEY_PREESSED};
        xQueueSendFromISR(screen_msg_queue, &msg, 0);
    }
}

static void IRAM_ATTR buttonDisplayOnIsrHandler(void *arg) 
{
    last_display_button_state = gpio_get_level(PIN_BUTTON_DISPLAY_ON);
#ifdef LVGL_PORTING_DEBUG       
    esp_rom_printf("Display change isr %d.\n", last_display_button_state);
#endif
    xTimerStartFromISR(debounce_display_button_timer, NULL);
}

void buttons_init(void)
{
    esp_rom_gpio_pad_select_gpio(PIN_BUTTON_NEXT);
    gpio_set_direction(PIN_BUTTON_NEXT, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_BUTTON_NEXT, GPIO_PULLUP_ONLY);
    gpio_set_intr_type(PIN_BUTTON_NEXT, GPIO_INTR_NEGEDGE);
    //gpio_set_intr_type(PIN_BUTTON_NEXT, GPIO_INTR_POSEDGE);
    vTaskDelay(20/portTICK_PERIOD_MS);
#ifdef LVGL_PORTING_DEBUG    
    ESP_LOGI(TAG, "Next Button get %d", gpio_get_level(PIN_BUTTON_NEXT));
#endif
    esp_rom_gpio_pad_select_gpio(PIN_BUTTON_DISPLAY_ON);
    gpio_set_direction(PIN_BUTTON_DISPLAY_ON, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_BUTTON_DISPLAY_ON, GPIO_PULLUP_ONLY);
    gpio_set_intr_type(PIN_BUTTON_DISPLAY_ON, GPIO_INTR_NEGEDGE);
    //gpio_set_intr_type(PIN_BUTTON_DISPLAY_ON, GPIO_INTR_POSEDGE);
    vTaskDelay(20/portTICK_PERIOD_MS);
#ifdef LVGL_PORTING_DEBUG    
    ESP_LOGI(TAG, "Display Button get %d", gpio_get_level(PIN_BUTTON_DISPLAY_ON));    
#endif
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_BUTTON_NEXT, buttonNextIsrHandler, (void*) NULL);
    gpio_isr_handler_add(PIN_BUTTON_DISPLAY_ON, buttonDisplayOnIsrHandler, (void*) NULL);    
}

esp_err_t input_init()
{
    ESP_LOGI(TAG, "Install button driver");
    buttons_init();

    debounce_next_button_timer = xTimerCreate(
        "debounce_next_button_timer",
        pdMS_TO_TICKS(DEBOUNCE_TIME_MS),
        pdFALSE,        // 不自动重载
        NULL,
        debounce_next_button_timer_callback
    );

    debounce_display_button_timer = xTimerCreate(
        "debounce_display_button_timer",
        pdMS_TO_TICKS(DEBOUNCE_TIME_MS),
        pdFALSE,        // 不自动重载
        NULL,
        debounce_display_button_timer_callback
    );

    return ESP_OK;
}

void buttons_init_simple(void)
{
    esp_rom_gpio_pad_select_gpio(PIN_BUTTON_NEXT);
    gpio_set_direction(PIN_BUTTON_NEXT, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_BUTTON_NEXT, GPIO_PULLUP_ONLY);
    gpio_set_intr_type(PIN_BUTTON_NEXT, GPIO_INTR_POSEDGE);
    vTaskDelay(20/portTICK_PERIOD_MS);
#ifdef LVGL_PORTING_DEBUG    
    ESP_LOGI(TAG, "Next Button get %d", gpio_get_level(PIN_BUTTON_NEXT));
#endif
    esp_rom_gpio_pad_select_gpio(PIN_BUTTON_DISPLAY_ON);
    gpio_set_direction(PIN_BUTTON_DISPLAY_ON, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_BUTTON_DISPLAY_ON, GPIO_PULLUP_ONLY);
    gpio_set_intr_type(PIN_BUTTON_DISPLAY_ON, GPIO_INTR_POSEDGE);
    vTaskDelay(20/portTICK_PERIOD_MS);
#ifdef LVGL_PORTING_DEBUG    
    ESP_LOGI(TAG, "Display Button get %d", gpio_get_level(PIN_BUTTON_DISPLAY_ON));    
#endif
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_BUTTON_NEXT, buttonNextIsrHandlerSimple, (void*) NULL);
    gpio_isr_handler_add(PIN_BUTTON_DISPLAY_ON, buttonDisplayOnIsrHandlerSimple, (void*) NULL);   
}

esp_err_t input_init_simple()
{
    ESP_LOGI(TAG, "Install button driver");
    buttons_init_simple();

    return ESP_OK;
}
