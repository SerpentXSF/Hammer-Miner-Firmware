/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <math.h>
#include "sdkconfig.h"
#include "lvgl.h"
#include "esp_log.h"

static const char * TAG="LVGL_DEMO_UI";

#ifndef PI
#define PI  (3.14159f)
#endif

#if CONFIG_EXAMPLE_LCD_IMAGE_FROM_EMBEDDED_BINARY
// LVGL image declare
LV_IMG_DECLARE(esp_logo)
LV_IMG_DECLARE(esp_text)
LV_IMG_DECLARE(miningscreen2)
#endif // CONFIG_EXAMPLE_LCD_IMAGE_FROM_EMBEDDED_BINARY

typedef struct {
    lv_obj_t *scr;
    int count_val;
} my_timer_context_t;

static my_timer_context_t my_tim_ctx;
static lv_obj_t *arc[3];
static lv_obj_t *img_logo = NULL;
static lv_obj_t *img_text = NULL;
static lv_color_t arc_color[] = {
    LV_COLOR_MAKE(232, 87, 116),
    LV_COLOR_MAKE(126, 87, 162),
    LV_COLOR_MAKE(90, 202, 228),
};

static void anim_timer_cb(lv_timer_t *timer)
{
    my_timer_context_t *timer_ctx = (my_timer_context_t *)lv_timer_get_user_data(timer);
    int count = timer_ctx->count_val;
    lv_obj_t *scr = timer_ctx->scr;

    // Play arc animation
    if (count < 90) {
        lv_coord_t arc_start = count > 0 ? (1 - cosf(count / 180.0f * PI)) * 270 : 0;
        lv_coord_t arc_len = (sinf(count / 180.0f * PI) + 1) * 135;

        for (size_t i = 0; i < sizeof(arc) / sizeof(arc[0]); i++) {
            lv_arc_set_bg_angles(arc[i], arc_start, arc_len);
            lv_arc_set_rotation(arc[i], (count + 120 * (i + 1)) % 360);
        }
    }

    // Delete arcs when animation finished
    if (count == 90) {
        for (size_t i = 0; i < sizeof(arc) / sizeof(arc[0]); i++) {
            lv_obj_delete(arc[i]);
        }

        // Create new image and make it transparent
        img_text = lv_image_create(scr);
#if CONFIG_EXAMPLE_LCD_IMAGE_FROM_FILE_SYSTEM
        lv_image_set_src(img_text, "S:/spiffs/esp_text.png");
#elif CONFIG_EXAMPLE_LCD_IMAGE_FROM_EMBEDDED_BINARY
        lv_image_set_src(img_text, &esp_text);
#endif
        lv_obj_set_style_image_opa(img_text, 0, 0);
    }

    // Move images when arc animation finished
    if ((count >= 100) && (count <= 180)) {
        lv_coord_t offset = (sinf((count - 140) * 2.25f / 90.0f) + 1) * 20.0f;
        lv_obj_align(img_logo, LV_ALIGN_CENTER, 0, -offset);
        lv_obj_align(img_text, LV_ALIGN_CENTER, 0, 2 * offset);
        lv_obj_set_style_image_opa(img_text, offset / 40.0f * 255, 0);
    }

    // Delete timer when all animation finished
    if ((count += 5) == 220) {
        lv_timer_delete(timer);
    } else {
        timer_ctx->count_val = count;
    }
}

static void start_animation(lv_obj_t *scr)
{
    // Align image
    lv_obj_center(img_logo);

    // Create arcs
    for (size_t i = 0; i < sizeof(arc) / sizeof(arc[0]); i++) {
        arc[i] = lv_arc_create(scr);

        // Set arc caption
        lv_obj_set_size(arc[i], 220 - 30 * i, 220 - 30 * i);
        lv_arc_set_bg_angles(arc[i], 120 * i, 10 + 120 * i);
        lv_arc_set_value(arc[i], 0);

        // Set arc style
        lv_obj_remove_style(arc[i], NULL, LV_PART_KNOB);
        lv_obj_set_style_arc_width(arc[i], 10, 0);
        lv_obj_set_style_arc_color(arc[i], arc_color[i], 0);

        // Make arc center
        lv_obj_center(arc[i]);
    }

    if (img_text) {
        lv_obj_delete(img_text);
        img_text = NULL;
    }

    // Create timer for animation
    my_tim_ctx.count_val = -90;
    my_tim_ctx.scr = scr;
    lv_timer_create(anim_timer_cb, 20, &my_tim_ctx);
}

void example_lvgl_demo_ui(lv_disp_t *disp)
{
    lv_obj_t *scr = lv_display_get_screen_active(disp);

    // Create image
    img_logo = lv_image_create(scr);
#if CONFIG_EXAMPLE_LCD_IMAGE_FROM_FILE_SYSTEM
    lv_image_set_src(img_logo, "S:/spiffs/esp_logo.png");
#elif CONFIG_EXAMPLE_LCD_IMAGE_FROM_EMBEDDED_BINARY
    //lv_image_set_src(img_logo, &esp_logo);
    lv_image_set_src(img_logo, &miningscreen2);
    lv_obj_center(img_logo);
#endif

    //start_animation(scr);
}

void lv_example_get_started_1(lv_disp_t *disp)
{
    //lv_obj_t *scr = lv_display_get_screen_active(disp);
    lv_obj_t *scr = lv_screen_active();

    /*Change the active screen's background color*/
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x003a57), LV_PART_MAIN);

    /*Create a white label, set its text and align it to the center*/
    lv_obj_t * label = lv_label_create(scr);
    lv_label_set_text(label, "DiDi Serena");
    lv_obj_set_style_text_color(scr, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    ESP_LOGI(TAG, "lv_example_get_started_2");
}

void lv_example_style_2(void)
{
    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_radius(&style, 5);

    /*Make a gradient*/
    lv_style_set_bg_opa(&style, LV_OPA_COVER);
    static lv_grad_dsc_t grad;
    grad.dir = LV_GRAD_DIR_VER;
    grad.stops_count = 2;
    grad.stops[0].color = lv_palette_lighten(LV_PALETTE_GREY, 1);
    grad.stops[0].opa = LV_OPA_COVER;
    grad.stops[1].color = lv_palette_main(LV_PALETTE_BLUE);
    grad.stops[1].opa = LV_OPA_COVER;

    /*Shift the gradient to the bottom*/
    grad.stops[0].frac  = 128;
    grad.stops[1].frac  = 192;

    lv_style_set_bg_grad(&style, &grad);

    /*Create an object with the new style*/
    lv_obj_t * obj = lv_obj_create(lv_screen_active());
    lv_obj_add_style(obj, &style, 0);
    lv_obj_center(obj);
}

void lv_example_style_6(void)
{
    static lv_style_t style;
    lv_style_init(&style);

    /*Set a background color and a radius*/
    lv_style_set_radius(&style, 5);
    lv_style_set_bg_opa(&style, LV_OPA_COVER);
    lv_style_set_bg_color(&style, lv_palette_lighten(LV_PALETTE_GREY, 3));
    lv_style_set_border_width(&style, 2);
    lv_style_set_border_color(&style, lv_palette_main(LV_PALETTE_BLUE));

    lv_style_set_image_recolor(&style, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_image_recolor_opa(&style, LV_OPA_50);
    lv_style_set_transform_rotation(&style, 300);

    /*Create an object with the new style*/
    lv_obj_t * obj = lv_image_create(lv_screen_active());
    lv_obj_add_style(obj, &style, 0);

    LV_IMAGE_DECLARE(img_cogwheel_argb);
    lv_image_set_src(obj, &img_cogwheel_argb);

    lv_obj_center(obj);
}