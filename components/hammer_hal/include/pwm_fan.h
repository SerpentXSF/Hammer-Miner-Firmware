#ifndef _PWM_FAN_H_
#define _PWM_FAN_H_

#include "esp_err.h"
#include "driver/ledc.h"
#include "driver/pulse_cnt.h"

#include "miner.h"

/*PWM configuration*/
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE

#define LEDC_PWM_OUTPUT_IO0     CONFIG_GPIO_PWM_0   //Define the output GPIO
#define LEDC_PWM_OUTPUT_IO1     CONFIG_GPIO_PWM_1   // Define the output GPIO

#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT // Set duty resolution to 13 bits, 2**13 = 8192
#define LEDC_FREQUENCY          8000 // Frequency in Hertz. Set frequency at 8 kHz
#define MAX_FAN_SPEED           8000
#define MIN_FAN_SPEED           0
/*#define MAX_PWM_CHANNEL         2*/

/*Pulse Counter configuration.*/
#define PULSE_COUNTER0_GPIO_NUM0 CONFIG_GPIO_PULSE_COUNTER_0
#define PULSE_COUNTER1_GPIO_NUM1 CONFIG_GPIO_PULSE_COUNTER_1

#define MINI_FAN_MAX_SPEED          MAX_FAN_SPEED
#define MINI_FAN_CHECK_PARAM        180

#define MINI_PRE_FAN_MAX_SPEED      4200
#define MINI_PRE_FAN_CHECK_PARAM    100

#define MIN_PWM_PERCENT         18
#define MAX_PWM_PERCENT         100

/*Overheat protection.*/
#define MAX_HASHBOARD_TEMP         71 
#define MAX_FAN_TEMP               68    //80//100    // 80
#define MAX_ENV_TEMP               55

/*Fan Control Parameters.*/
#define MAX_FAN_TEMP_QUIT          (MAX_FAN_TEMP - 10)
#define MIN_FAN_TEMP               30
#define MIN_FAN_TEMP_QUIT          (MIN_FAN_TEMP + 10) 
#define MID_FAN_TEMP                40
#define STEP_FAN_TEMP               2

typedef struct{
    int8_t control_board_temperature;
    int8_t highest_board_temperature;
    /*0: front, 1: back*/
    int fan_rpm[MAX_PWM_CHANNEL];
    uint16_t current;

    /*0: front, 1: back*/
    uint16_t pwm_config[MAX_PWM_CHANNEL];

    /*for increase temperature.*/
    bool b_in_increase_temperature_flow;    
    
    /*fan running status*/
    bool b_max_fan_pwm, b_max_fan_pwm_quit;
    bool b_min_fan_pwm, b_min_fan_pwm_quit;
    bool b_pwm_changed;
}FanInputInfo;

esp_err_t ledc_pwm_init(ledc_channel_t pwm_channel);
esp_err_t ledc_set_pwm(ledc_channel_t pwm_channel, int pwm_percent);

esp_err_t fan_pcnts_init(ledc_channel_t pwm_channel);
esp_err_t fan_pcnts_restart(ledc_channel_t pwm_channel);
esp_err_t fan_pcnts_clear_counter(ledc_channel_t pwm_channel);
esp_err_t fan_pcnts_get_counter(ledc_channel_t pwm_channel, int *ret_pcounts);

esp_err_t fan_pcnts_get_rpm(ledc_channel_t pwm_channel, int *ret_pcounts, uint32_t mseconds);

bool check_fan_ok(uint16_t *pwm_config, uint16_t *fan_rpm, int fan_num, int max_fan_speed, int fan_check_param);
void lotto_set_pwm_according_to_temperature(FanInputInfo *fan_info);
#endif
