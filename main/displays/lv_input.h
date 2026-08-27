#ifndef _LV_INPUT_H
#define _LV_INPUT_H

#include "esp_err.h"

#define LONG_PRESS_DURATION_MS 2000

// GPIO for buttons
#define PIN_BUTTON_NEXT (gpio_num_t) 14 // Button 1 GPIO pin
#define PIN_BUTTON_DISPLAY_ON (gpio_num_t) 0  // Button 2 GPIO pin

esp_err_t input_init();
esp_err_t input_init_simple();

#endif /* _LV_INPUT_H*/