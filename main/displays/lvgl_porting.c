#include <stdio.h>
#include <sys/lock.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_spiffs.h"
#include "driver/gpio.h"
#include "lvgl.h"

#include "lvgl_screen.h"
#include "lv_input.h"
#include "lilygo_porting.h"
#include "lvgl_porting.h"

static const char *TAG = "lvgl_porting";
_lock_t lvgl_api_lock;
static lv_timer_t *refresh_timer = NULL, *process_msg_timer = NULL;

typedef struct {
    uint32_t addr;
    uint8_t param[20];
    uint32_t len;
} lcd_cmd_t;

lcd_cmd_t lcd_st7789v[] = {
    {0x11, {0}, 0 | 0x80},
    {0x3A, {0X05}, 1},
    {0xB2, {0X0B, 0X0B, 0X00, 0X33, 0X33}, 5},
    {0xB7, {0X75}, 1},
    {0xBB, {0X28}, 1},
    {0xC0, {0X2C}, 1},
    {0xC2, {0X01}, 1},
    {0xC3, {0X1F}, 1},
    {0xC6, {0X13}, 1},
    {0xD0, {0XA7}, 1},
    {0xD0, {0XA4, 0XA1}, 2},
    {0xD6, {0XA1}, 1},
    {0xE0, {0XF0, 0X05, 0X0A, 0X06, 0X06, 0X03, 0X2B, 0X32, 0X43, 0X36, 0X11, 0X10, 0X2B, 0X32}, 14},
    {0xE1, {0XF0, 0X08, 0X0C, 0X0B, 0X09, 0X24, 0X2B, 0X22, 0X43, 0X38, 0X15, 0X16, 0X2F, 0X37}, 14},
};

static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_display_t *display = (lv_display_t *)user_ctx;
    lv_display_flush_ready(display);  
    return false;
}

void display_push_colors(esp_lcd_panel_handle_t panel_handle, uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint16_t *data)
{
    esp_lcd_panel_draw_bitmap(panel_handle, x_start, y_start, x_end, y_end, data);
}

static void ST7789_lvgl_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(display);

    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;

    // because LCD is big-endian, we need to swap the RGB bytes order
    lv_draw_sw_rgb565_swap(color_map, (offsetx2 + 1 - offsetx1) * (offsety2 + 1 - offsety1));

    display_push_colors(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, (uint16_t *)color_map);
}

static void example_increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);

#ifdef LVGL_PORTING_DEBUG
    static int counter = 0;
    if(500 == counter++ ){
        counter = 0;
        printf("tick 500\n");
    }
#endif
    /*process the update event.*/
    /*_lock_acquire(&lvgl_api_lock);
    lv_update_screen_cb();
    _lock_release(&lvgl_api_lock);*/
}

// 定时器回调：标记整个屏幕为需要重绘
void timer_full_refresh_cb(lv_timer_t * timer) {
    // 获取当前活动屏幕
    //lv_obj_t * screen = lv_scr_act();
    
    // 标记整个屏幕为无效（需要重绘）
    //lv_obj_invalidate(screen);
    
    // 或者更直接的方式：标记所有对象
    // lv_obj_invalidate(NULL); // 传入NULL表示所有对象
    refresh_current_screen();
}

void lv_process_msg_cb(lv_timer_t *timer) {
    lv_update_screen_cb();
}


static void example_lvgl_port_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");
    uint32_t time_till_next_ms = 0;
    while (1) {
        _lock_acquire(&lvgl_api_lock);
        time_till_next_ms = lv_timer_handler();
        _lock_release(&lvgl_api_lock);

        // in case of triggering a task watch dog time out
        time_till_next_ms = MAX(time_till_next_ms, EXAMPLE_LVGL_TASK_MIN_DELAY_MS);
        // in case of lvgl display not ready yet
        time_till_next_ms = MIN(time_till_next_ms, EXAMPLE_LVGL_TASK_MAX_DELAY_MS);
        vTaskDelay(pdMS_TO_TICKS(time_till_next_ms));
    }
}

#if CONFIG_EXAMPLE_LCD_IMAGE_FROM_FILE_SYSTEM
void example_init_filesystem(void)
{
    ESP_LOGI(TAG, "Initializing filesystem");
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true
    };

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name(ret));
        esp_spiffs_format(conf.partition_label);
        return;
    } else {
        ESP_LOGI(TAG, "Partition size: total: %zu, used: %zu", total, used);
    }
}
#endif // CONFIG_EXAMPLE_LCD_IMAGE_FROM_FILE_SYSTEM

void example_init_i80_bus(esp_lcd_panel_io_handle_t *io_handle)
{
    ESP_LOGI(TAG, "Initialize lcd i80 bus");
    esp_lcd_i80_bus_handle_t i80_bus = NULL;
    esp_lcd_i80_bus_config_t bus_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .dc_gpio_num = EXAMPLE_PIN_NUM_DC,
        .wr_gpio_num = EXAMPLE_PIN_NUM_PCLK,
        .data_gpio_nums = {
            EXAMPLE_PIN_NUM_DATA0,
            EXAMPLE_PIN_NUM_DATA1,
            EXAMPLE_PIN_NUM_DATA2,
            EXAMPLE_PIN_NUM_DATA3,
            EXAMPLE_PIN_NUM_DATA4,
            EXAMPLE_PIN_NUM_DATA5,
            EXAMPLE_PIN_NUM_DATA6,
            EXAMPLE_PIN_NUM_DATA7,
#if CONFIG_EXAMPLE_LCD_I80_BUS_WIDTH > 8
            EXAMPLE_PIN_NUM_DATA8,
            EXAMPLE_PIN_NUM_DATA9,
            EXAMPLE_PIN_NUM_DATA10,
            EXAMPLE_PIN_NUM_DATA11,
            EXAMPLE_PIN_NUM_DATA12,
            EXAMPLE_PIN_NUM_DATA13,
            EXAMPLE_PIN_NUM_DATA14,
            EXAMPLE_PIN_NUM_DATA15,
#endif
        },
        .bus_width = CONFIG_EXAMPLE_LCD_I80_BUS_WIDTH,
        #ifdef ENABLE_LVGL_DRAW_DIRECT_MODE
        .max_transfer_bytes = EXAMPLE_LCD_V_RES * EXAMPLE_LVGL_DRAW_BUF_FULL_LINES * sizeof(uint16_t),
        #else
        .max_transfer_bytes = EXAMPLE_LVGL_PARTIAL_BUF_SIZE,
        #endif
        .dma_burst_size = EXAMPLE_DMA_BURST_SIZE,
        //.psram_trans_align = LCD_PSRAM_TRANS_ALIGN,
        //.sram_trans_align = LCD_SRAM_TRANS_ALIGN
    };
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));

    esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = EXAMPLE_PIN_NUM_CS,
        .pclk_hz = TDISPLAYS3_LCD_PIXEL_CLOCK_HZ, //EXAMPLE_LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth = 10,
        .dc_levels = {
            .dc_idle_level = 0,
            .dc_cmd_level = 0,
            .dc_dummy_level = 0,
            .dc_data_level = 1,
        },
        .lcd_cmd_bits = EXAMPLE_LCD_CMD_BITS,
        .lcd_param_bits = EXAMPLE_LCD_PARAM_BITS,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, io_handle));
}

void example_init_lcd_panel(bool invertScreen, esp_lcd_panel_io_handle_t io_handle, esp_lcd_panel_handle_t *panel)
{
    esp_lcd_panel_handle_t panel_handle = NULL;

    ESP_LOGI(TAG, "Install LCD driver: %d", invertScreen);
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_RST,
        .color_space = ESP_LCD_COLOR_SPACE_RGB,
        .bits_per_pixel = 16,
        .vendor_config = NULL
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    esp_lcd_panel_reset(panel_handle);
    vTaskDelay(100 / portTICK_PERIOD_MS);  // 100 ms delay after reset
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_invert_color(panel_handle, true);
    esp_lcd_panel_swap_xy(panel_handle, true); 
    //The screen faces you, and the USB is on the left
    if(invertScreen){
        esp_lcd_panel_mirror(panel_handle, true, false);
    } else {
        esp_lcd_panel_mirror(panel_handle, false, true);
    } 
    esp_lcd_panel_set_gap(panel_handle, 0, 35);

    for (uint8_t i = 0; i < (sizeof(lcd_st7789v) / sizeof(lcd_cmd_t)); i++) {
        esp_lcd_panel_io_tx_param(io_handle, lcd_st7789v[i].addr, lcd_st7789v[i].param, lcd_st7789v[i].len & 0x7f);
        if (lcd_st7789v[i].len & 0x80)
            vTaskDelay(pdMS_TO_TICKS(120));
    }
    ESP_LOGD(TAG, "panel_handle is @%p", panel_handle);

    *panel = panel_handle;
}

static lv_display_t * display_lvgl_init(bool invertScreen)
{
    esp_lcd_panel_io_handle_t io_handle = NULL;
    example_init_i80_bus(&io_handle);
    esp_lcd_panel_handle_t panel_handle = NULL;
    example_init_lcd_panel(invertScreen, io_handle, &panel_handle);
    // Stub: user can flush pre-defined pattern to the screen before we turn on the screen or backlight
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    // create a lvgl display
    lv_display_t *display = lv_display_create(EXAMPLE_LCD_V_RES, EXAMPLE_LCD_H_RES); //320 x 170

    // it's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized
    // 320 x 170
    #ifdef ENABLE_LVGL_DRAW_DIRECT_MODE
    size_t draw_buffer_sz = EXAMPLE_LCD_V_RES * EXAMPLE_LVGL_DRAW_BUF_FULL_LINES * sizeof(lv_color16_t);
    #else
    size_t draw_buffer_sz = EXAMPLE_LVGL_PARTIAL_BUF_SIZE;
    #endif
    // alloc draw buffers used by LVGL
    //uint32_t draw_buf_alloc_caps = 0;
#if CONFIG_EXAMPLE_LCD_I80_COLOR_IN_PSRAM
    draw_buf_alloc_caps |= MALLOC_CAP_SPIRAM;
#endif
    //void *buf1 = esp_lcd_i80_alloc_draw_buffer(io_handle, draw_buffer_sz, draw_buf_alloc_caps);
    //void *buf2 = esp_lcd_i80_alloc_draw_buffer(io_handle, draw_buffer_sz, draw_buf_alloc_caps);
    void *buf1 = heap_caps_aligned_calloc(64, 1, draw_buffer_sz, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    void *buf2 = heap_caps_aligned_calloc(64, 1, draw_buffer_sz, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    ESP_LOGI(TAG, "buf1@%p, buf2@%p", buf1, buf2);
    assert(buf1);
    assert(buf2);

    // initialize LVGL draw buffers
    #ifdef ENABLE_LVGL_DRAW_DIRECT_MODE
    lv_display_set_buffers(display, buf1, buf2, draw_buffer_sz, LV_DISPLAY_RENDER_MODE_DIRECT);
    #else
    lv_display_set_buffers(display, buf1, buf2, draw_buffer_sz, LV_DISPLAY_RENDER_MODE_PARTIAL);
    #endif
    // associate the mipi panel handle to the display
    lv_display_set_user_data(display, panel_handle);
    // set color depth
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    // set the callback which can copy the rendered image to an area of the display
    lv_display_set_flush_cb(display, ST7789_lvgl_flush_cb);

#ifdef ENABLE_LVGL_TICK_TIMER_PORTING 
    ESP_LOGI(TAG, "Install LVGL tick timer");
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

    ESP_LOGI(TAG, "Register panel io event callback for LVGL");
    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = example_notify_lvgl_flush_ready,
    };
    /* Register done callback */
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, display));

    //create a global time, refresh every 200ms
    refresh_timer = lv_timer_create(timer_full_refresh_cb, EXAMPLE_LVGL_REFRESH_SCREEN_INTERVAL, NULL);
    process_msg_timer = lv_timer_create(lv_process_msg_cb, EXAMPLE_LVGL_PROCESS_EVENT_INTERVAL, NULL);

    ESP_LOGI(TAG, "Create LVGL task");
    xTaskCreate(example_lvgl_port_task, "LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE, NULL, EXAMPLE_LVGL_TASK_PRIORITY, NULL);
#endif

    return display;
}

static esp_err_t display_s3_gpio_init()
{
    esp_err_t ret = ESP_OK;
    
    gpio_config_t pwr_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << EXAMPLE_PIN_NUM_PWR
    };
    ESP_ERROR_CHECK(gpio_config(&pwr_gpio_config));
    gpio_set_level(EXAMPLE_PIN_NUM_PWR, 0);

    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << EXAMPLE_PIN_NUM_BK_LIGHT
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    /*turn on backligt after the lvgl initialization.*/
    gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL);

    gpio_config_t rd_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << EXAMPLE_PIN_NUM_RD
    };
    ESP_ERROR_CHECK(gpio_config(&rd_gpio_config));
    ret = gpio_set_level(EXAMPLE_PIN_NUM_RD, 1);

    return ret;
}

static void display_s3_init_input_and_screen(lv_display_t *display)
{
    ESP_LOGI(TAG, "Display LVGL first screen.");       

#if 1   
    input_init();
#else
    input_init_simple();
#endif
    init_all_screen(display);
    show_first_screen();
    //_lock_release(&lvgl_api_lock);  
}

void display_s3_init(bool invertScreen)
{
    display_s3_gpio_init();

    lv_display_t *display = display_lvgl_init(invertScreen);
    
    display_s3_init_input_and_screen(display);
    display_backlight_on();
    refresh_current_screen();
}
