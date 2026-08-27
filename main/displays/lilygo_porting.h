#ifndef _LILYGO_PORTING
#define _LILYGO_PORTING

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your LCD spec //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ     2000000
#define CONFIG_EXAMPLE_LCD_I80_BUS_WIDTH    8

#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL  1
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL
#define EXAMPLE_PIN_NUM_DATA0          39
#define EXAMPLE_PIN_NUM_DATA1          40
#define EXAMPLE_PIN_NUM_DATA2          41
#define EXAMPLE_PIN_NUM_DATA3          42
#define EXAMPLE_PIN_NUM_DATA4          45
#define EXAMPLE_PIN_NUM_DATA5          46
#define EXAMPLE_PIN_NUM_DATA6          47
#define EXAMPLE_PIN_NUM_DATA7          48
#if CONFIG_EXAMPLE_LCD_I80_BUS_WIDTH > 8
#define EXAMPLE_PIN_NUM_DATA8          CONFIG_EXAMPLE_PIN_NUM_DATA8
#define EXAMPLE_PIN_NUM_DATA9          CONFIG_EXAMPLE_PIN_NUM_DATA9
#define EXAMPLE_PIN_NUM_DATA10         CONFIG_EXAMPLE_PIN_NUM_DATA10
#define EXAMPLE_PIN_NUM_DATA11         CONFIG_EXAMPLE_PIN_NUM_DATA11
#define EXAMPLE_PIN_NUM_DATA12         CONFIG_EXAMPLE_PIN_NUM_DATA12
#define EXAMPLE_PIN_NUM_DATA13         CONFIG_EXAMPLE_PIN_NUM_DATA13
#define EXAMPLE_PIN_NUM_DATA14         CONFIG_EXAMPLE_PIN_NUM_DATA14
#define EXAMPLE_PIN_NUM_DATA15         CONFIG_EXAMPLE_PIN_NUM_DATA15
#endif
#define EXAMPLE_PIN_NUM_PCLK           8
#define EXAMPLE_PIN_NUM_CS             6
#define EXAMPLE_PIN_NUM_DC             7
#define EXAMPLE_PIN_NUM_RST            5
#define EXAMPLE_PIN_NUM_BK_LIGHT       38
#define EXAMPLE_PIN_NUM_PWR            15
#define EXAMPLE_PIN_NUM_RD             9

// The pixel number in horizontal and vertical
#define EXAMPLE_LCD_H_RES              170 //240
#define EXAMPLE_LCD_V_RES              320 //280

// Bit number used to represent command and parameter
#define EXAMPLE_LCD_CMD_BITS           8
#define EXAMPLE_LCD_PARAM_BITS         8


#undef LVGL_PORTING_DEBUG
#endif