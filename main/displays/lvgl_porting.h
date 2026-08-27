#ifndef _LVGL_PORTING_H
#define _LVGL_PORTING_H

#include "global_state.h"

#define EXAMPLE_LVGL_TICK_PERIOD_MS    30
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 500
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 1000 / CONFIG_FREERTOS_HZ
#define EXAMPLE_LVGL_TASK_STACK_SIZE   (5 * 1024)
#define EXAMPLE_LVGL_TASK_PRIORITY     18

#define EXAMPLE_LVGL_PROCESS_EVENT_INTERVAL     200
#define EXAMPLE_LVGL_REFRESH_SCREEN_INTERVAL    2000

#define EXAMPLE_LVGL_PARTIAL_BUF_SIZE       320*8*sizeof(lv_color_t)

#define EXAMPLE_LVGL_DRAW_BUF_FULL_LINES    170

#undef ENABLE_LVGL_DRAW_DIRECT_MODE
#define ENABLE_LVGL_TICK_TIMER_PORTING

#define EXAMPLE_DMA_BURST_SIZE         64 // 16, 32, 64. Higher burst size can improve the performance when the DMA buffer comes from PSRAM

#define TDISPLAYS3_LCD_PIXEL_CLOCK_HZ (10 * 1000 * 1000)                             // Pixel clock for LCD in Hz (60 FPS, 170 x 320 pixels)
// Alignment settings for PSRAM and SRAM transfers
#define LCD_PSRAM_TRANS_ALIGN 64 // Alignment for PSRAM transfers
#define LCD_SRAM_TRANS_ALIGN 4   // Alignment for SRAM transfers


void display_s3_init(bool invertScreen);

void refresh_sensor_data_from_system(GlobalState *GLOBAL_STATE);
void refresh_network_from_system(char *stratum_url, uint16_t port);
void refresh_portal_data_from_system();
void refresh_hash_data_from_system(GlobalState *GLOBAL_STATE);
void refresh_coin_data_from_system_test();

void inform_wifi_status(bool b_connected);
void inform_mining_status();

void refresh_ip_from_system(void);

void logMessage(const char *message);
void showFoundBlockScreen(void);
void hideFoundBlockScreen(void);
void showErrorScreen(const char *error_message, uint32_t error_code);
void hideErrorScreen(void);
void showQrScreen(void);
void hideQrScreen(void);
void displayFlashPro(void);
void displayFlashSet(int on);
#endif
