#ifndef _PWM_FAN_H_
#define _PWM_FAN_H_

#include "esp_err.h"

#include "miner.h"

#define MAX_FAN_SPEED           8000

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


bool check_fan_ok(uint16_t *pwm_config, uint16_t *fan_rpm, int fan_num, int max_fan_speed, int fan_check_param);
void lotto_set_pwm_according_to_temperature(FanInputInfo *fan_info);
#endif