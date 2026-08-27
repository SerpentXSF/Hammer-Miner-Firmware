#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "device.h"
#include "pwm_fan.h"

static const char *TAG = "pwm_fan";

/*used only by mini, not used by lotto.*/
bool check_fan_ok(
        uint16_t *pwm_config,
        uint16_t *fan_rpm,
        int fan_num,
        int max_fan_speed,
        int fan_check_param
    )
{
    bool isOK = true;

    for(int i = 0; i < fan_num; i++)
    {
        if(fan_rpm[i] < (pwm_config[i]*max_fan_speed/fan_check_param))
        {
            ESP_LOGW(TAG, "fan %d low speed %d, pwm %"PRIu16"", i, fan_rpm[i], pwm_config[i]);
            isOK = false;
            goto exit;
        }
    
        if(100 == pwm_config[i] && fan_rpm[i] < max_fan_speed*75/100)
        {
            ESP_LOGW(TAG, "fan %d low speed %d, max_fan_speed %i", i, fan_rpm[i], max_fan_speed);
            isOK = false;
            goto exit;
        }
    }

exit:
    return isOK;
}

bool max_fan_control(FanInputInfo *fan_info)
{
    bool executed = false;
    int temp_highest = fan_info->highest_board_temperature;
    static int bak_last_pwm = 30;

    /*The high temperaturen protection and quit mechanism of hash board.*/
	if((temp_highest >= MAX_FAN_TEMP) && (fan_info->pwm_config[0] != MAX_PWM_PERCENT))
    {
		bak_last_pwm = fan_info->pwm_config[0];
        fan_info->pwm_config[0] = fan_info->pwm_config[1] = MAX_PWM_PERCENT;
        EMC2302_set_fan_speed(fan_info->pwm_config[0]);
        executed = true;
        fan_info->b_max_fan_pwm = true;
        fan_info->b_max_fan_pwm_quit = false;

		ESP_LOGD(TAG, "ERRORMSG Temp: %d, Max Fan,  back up PWM %d.", temp_highest, bak_last_pwm);
    }else if((temp_highest <= MAX_FAN_TEMP_QUIT) && (fan_info->pwm_config[0] == MAX_PWM_PERCENT)){ 		
        fan_info->pwm_config[0] = fan_info->pwm_config[1] = (bak_last_pwm + STEP_FAN_TEMP);
		EMC2302_set_fan_speed(fan_info->pwm_config[0]);
        executed = true;
        fan_info->b_min_fan_pwm = false;
        fan_info->b_min_fan_pwm_quit = true;

		ESP_LOGD(TAG, "ERRORMSG Temp: %d, Quit Max Fan,  set PWM %d.", temp_highest, fan_info->pwm_config[0]);
	}

    return executed;
}

bool min_fan_control(FanInputInfo *fan_info)
{
    bool executed = false;
    int temp_highest = fan_info->highest_board_temperature;
    int enter_50_degree_pwm = 60;

    /*The low temperaturen protection and quit mechanism of hash board.*/
    if(temp_highest <= MIN_FAN_TEMP)
    {
        fan_info->pwm_config[0] = fan_info->pwm_config[1] = MIN_PWM_PERCENT;
        EMC2302_set_fan_speed(fan_info->pwm_config[0]);

        fan_info->b_min_fan_pwm = true;
        fan_info->b_min_fan_pwm_quit = false;

        executed = true;
    }else if(temp_highest >= MIN_FAN_TEMP_QUIT && fan_info->pwm_config[1] == MIN_PWM_PERCENT){
		fan_info->pwm_config[0] = fan_info->pwm_config[1] = enter_50_degree_pwm - 20;/*40 degree.*/
        EMC2302_set_fan_speed(fan_info->pwm_config[0]);

        fan_info->b_min_fan_pwm = false;
        fan_info->b_min_fan_pwm_quit = true;

        executed = true;
	}

    return executed;
}

bool increase_board_temperature_fan_control(FanInputInfo *fan_info)
{
    bool executed = false;
    static bool increase_temperature_done = false;
    int temp_highest = fan_info->highest_board_temperature;
    int enter_50_degree_pwm = 60;

    /*Increase the temperature of hash board.*/
	if(!increase_temperature_done){
        fan_info->b_in_increase_temperature_flow = true;

		if(temp_highest<=40){	
			fan_info->pwm_config[0] = fan_info->pwm_config[1] = enter_50_degree_pwm - 30;
		}else if(temp_highest>40 && temp_highest<=45){	
			fan_info->pwm_config[0] = fan_info->pwm_config[1] = enter_50_degree_pwm - 20;
		}else if(temp_highest>45 && temp_highest<=50){	
			fan_info->pwm_config[0] = fan_info->pwm_config[1] = enter_50_degree_pwm - 10;
		}else if(temp_highest>50 && temp_highest<=60){	
			fan_info->pwm_config[0] = fan_info->pwm_config[1] = enter_50_degree_pwm;
            increase_temperature_done = true;
            fan_info->b_in_increase_temperature_flow = false;
            ESP_LOGI(TAG, "Increase-temperature 4 is done.");
		}else if(temp_highest>60){
			fan_info->pwm_config[0] = fan_info->pwm_config[1] = 100;
            increase_temperature_done = true;
            fan_info->b_in_increase_temperature_flow = false;
            ESP_LOGI(TAG, "Increase-temperature 5 is done.");
		}
		
		EMC2302_set_fan_speed(fan_info->pwm_config[0]);
	} 

    return executed;
}

bool temperature_range_fan_control(FanInputInfo *fan_info)
{
    bool executed = false;
    static int last_pwm_percent = 30;
	static int fan_pwm_low_limit = 0, fan_pwm_high_limit = 100;
	static int enter_low_limit_pwm = 0, enter_high_limit_pwm = 0;
	static int32_t continuous_need_to_decrease_pwm = 0, continuous_need_to_increase_pwm = 0;
	static int start_temperature_of_continuous_need_to_decrease_pwm = 0;

	int target_degree_upper_limit = MAX_FAN_TEMP - 4, 
        target_degree_lower_limit = MAX_FAN_TEMP - 8; /*[60, 64]*/
    int temp_highest = fan_info->highest_board_temperature;

    if(fan_info->b_max_fan_pwm || fan_info->b_min_fan_pwm || fan_info->b_in_increase_temperature_flow){
        continuous_need_to_decrease_pwm = continuous_need_to_increase_pwm = 0;
        /*clear the fan_pwm_limit, maybe the environmental temperature changed.*/
		fan_pwm_low_limit = 0, fan_pwm_high_limit = 100;
        goto exit;
    }else if(fan_info->b_max_fan_pwm_quit || fan_info->b_min_fan_pwm_quit){
        fan_info->b_max_fan_pwm_quit = fan_info->b_min_fan_pwm_quit = false;
        continuous_need_to_decrease_pwm = continuous_need_to_increase_pwm = 0;
        /*clear the fan_pwm_limit, maybe the environmental temperature changed.*/
		fan_pwm_low_limit = 0, fan_pwm_high_limit = 100;
        goto exit;
    }else{
        executed = true;
        /*update the pwm.*/
        last_pwm_percent = fan_info->pwm_config[0];
    }

    /*keep the temperature stable*/
	if(temp_highest < target_degree_lower_limit){
		continuous_need_to_increase_pwm = 0;
		continuous_need_to_decrease_pwm++;

		if(continuous_need_to_decrease_pwm == 1){
			start_temperature_of_continuous_need_to_decrease_pwm = temp_highest;
		}

		if(continuous_need_to_decrease_pwm == 20){
			if(temp_highest <= start_temperature_of_continuous_need_to_decrease_pwm){
				last_pwm_percent = last_pwm_percent - STEP_FAN_TEMP;
				if(last_pwm_percent < MIN_PWM_PERCENT)
					last_pwm_percent = MIN_PWM_PERCENT;
				if(last_pwm_percent < fan_pwm_low_limit)
					last_pwm_percent = fan_pwm_low_limit;
				
				fan_info->pwm_config[0] = fan_info->pwm_config[1] = last_pwm_percent;
				EMC2302_set_fan_speed(fan_info->pwm_config[0]);               
				ESP_LOGD(TAG, "ERRORMSG decrease pwm %d, %d, temp %d <= %d", 
					fan_info->pwm_config[0], fan_info->pwm_config[1], temp_highest,
					start_temperature_of_continuous_need_to_decrease_pwm
				);
			}

			continuous_need_to_decrease_pwm = 0;
		}
	}else if(temp_highest > target_degree_upper_limit){
		continuous_need_to_decrease_pwm = 0;
		continuous_need_to_increase_pwm++;

		/*if(continuous_need_to_increase_pwm == 1){
			start_temperature_of_continuous_need_to_increase_pwm = temp_highest;
		}*/

		if(continuous_need_to_increase_pwm == 5){
			//if(temp_highest > start_temperature_of_continuous_need_to_increase_pwm){
			last_pwm_percent = last_pwm_percent + STEP_FAN_TEMP;
			if(last_pwm_percent > MAX_PWM_PERCENT)
				last_pwm_percent = MAX_PWM_PERCENT;
			if(last_pwm_percent > fan_pwm_high_limit)
				last_pwm_percent = fan_pwm_high_limit;
			
			fan_info->pwm_config[0] = fan_info->pwm_config[1] = last_pwm_percent;
			EMC2302_set_fan_speed(fan_info->pwm_config[0]);   
			ESP_LOGD(TAG, "ERRORMSG increase pwm %d, %d, temp %d", 
				fan_info->pwm_config[0], fan_info->pwm_config[1], temp_highest
			);
			//}

			continuous_need_to_increase_pwm = 0;
		}
	}else{
		/*59 -> 60 case. */
		if((temp_highest == target_degree_lower_limit) && (continuous_need_to_decrease_pwm != 0)){
			enter_low_limit_pwm = last_pwm_percent;
			ESP_LOGD(TAG, "ERRORMSG enter %d pwm %d.", target_degree_lower_limit, last_pwm_percent);
			
			if(enter_high_limit_pwm != 0){
				fan_pwm_high_limit = enter_high_limit_pwm - STEP_FAN_TEMP;
			}
		}

		/*65 -> 64 case. */
		if((temp_highest == target_degree_upper_limit) && (continuous_need_to_increase_pwm != 0)){
			enter_high_limit_pwm = last_pwm_percent;
			ESP_LOGD(TAG, "ERRORMSG enter %d pwm %d.", target_degree_upper_limit, last_pwm_percent);

			if(enter_low_limit_pwm != 0){
				fan_pwm_low_limit = enter_low_limit_pwm + STEP_FAN_TEMP;
			}
		}

		//do nothing, keep the pwm and clear the counter.
		continuous_need_to_increase_pwm = continuous_need_to_decrease_pwm = 0;
	}

exit:
    return executed;
}

/*TODO: a new feature.*/
bool lowest_power_fan_control(FanInputInfo *fan_info)
{
    bool executed = false;

    return executed; 
}

void mini_set_pwm_according_to_temperature(
        FanInputInfo *fan_info)
{

    if(max_fan_control(fan_info)){
        //do nothing but break
    }else if(min_fan_control(fan_info)){
        //do nothing but break
    }else if(increase_board_temperature_fan_control(fan_info)){
        //do nothing but break
    }
    
    if(temperature_range_fan_control(fan_info)){
        //do nothing but break;
    }
}


void lotto_set_pwm_according_to_temperature(FanInputInfo *fan_info)
{
    bool executed = false;
    int temp_highest = fan_info->highest_board_temperature;
    static int bak_last_pwm = 80;
    int enter_50_degree_pwm = 70;
    static bool increase_temperature_done = false;

    static int last_pwm_percent = 30;
    static int fan_pwm_low_limit = 0, fan_pwm_high_limit = 100;
    static int enter_low_limit_pwm = 0, enter_high_limit_pwm = 0;
    static uint32_t continuous_need_to_decrease_pwm = 0, continuous_need_to_increase_pwm = 0;
    static int start_temperature_of_continuous_need_to_decrease_pwm = 0;

    int target_degree_upper_limit = MAX_FAN_TEMP - 4;  // 64
    int target_degree_lower_limit = MAX_FAN_TEMP - 8; /*[60, 64]*/


    /*The high temperaturen protection and quit mechanism of hash board.*/
    if ((temp_highest >= MAX_FAN_TEMP) && (fan_info->pwm_config[0] != MAX_PWM_PERCENT)) {
        bak_last_pwm = fan_info->pwm_config[0];
        fan_info->pwm_config[0] = MAX_PWM_PERCENT;
        EMC2302_set_fan_speed(fan_info->pwm_config[0]);

        executed = true;
        fan_info->b_max_fan_pwm = true;
        fan_info->b_max_fan_pwm_quit = false;

        ESP_LOGI(TAG, "Temp: %d, Max Fan,  back up PWM %d.", temp_highest, fan_info->pwm_config[0]);
    } else if ((temp_highest <= MAX_FAN_TEMP_QUIT) && (fan_info->pwm_config[0] == MAX_PWM_PERCENT)) {
        fan_info->pwm_config[0] = (bak_last_pwm + STEP_FAN_TEMP);
        EMC2302_set_fan_speed(fan_info->pwm_config[0]);

        fan_info->b_max_fan_pwm = false;
        fan_info->b_max_fan_pwm_quit = true;
        executed = true;
        ESP_LOGI(TAG, "Temp: %d, Quit Max Fan,  set PWM %d.", temp_highest, fan_info->pwm_config[0]);
    }

    /*The low temperaturen protection and quit mechanism of hash board.*/
    if (temp_highest <= MIN_FAN_TEMP) {
        fan_info->pwm_config[0] = MIN_PWM_PERCENT;
        EMC2302_set_fan_speed(fan_info->pwm_config[0]);

        fan_info->b_min_fan_pwm = true;
        fan_info->b_min_fan_pwm_quit = false;
        executed = true;


    } else if (temp_highest >= MIN_FAN_TEMP_QUIT && fan_info->pwm_config[0] == MIN_PWM_PERCENT) {
        fan_info->pwm_config[0] = enter_50_degree_pwm - 40;  /*40 degree.*/
        EMC2302_set_fan_speed(fan_info->pwm_config[0]);

        fan_info->b_min_fan_pwm = false;
        fan_info->b_min_fan_pwm_quit = true;
        executed = true;
    }

    if (!executed) {
        if (increase_temperature_done) {
        } else {
            fan_info->b_in_increase_temperature_flow = true;
            if (temp_highest <= 40) {
                fan_info->pwm_config[0] = enter_50_degree_pwm - 30;
            } else if (temp_highest > 40 && temp_highest <= 45) {
                fan_info->pwm_config[0] = enter_50_degree_pwm - 20;
            } else if (temp_highest > 45 && temp_highest <= 50) {
                fan_info->pwm_config[0] = enter_50_degree_pwm - 10;
            } else if (temp_highest > 50 && temp_highest <= 60) {
                fan_info->pwm_config[0] = enter_50_degree_pwm;
                increase_temperature_done = true;
                fan_info->b_in_increase_temperature_flow = false;
                ESP_LOGD(TAG, "Increase-temperature 4 is done.");
            } else if (temp_highest > 60) {
                fan_info->pwm_config[0] = 100;
                increase_temperature_done = true;
                fan_info->b_in_increase_temperature_flow = false;
                ESP_LOGD(TAG, "Increase-temperature 5 is done.");
            }
            EMC2302_set_fan_speed(fan_info->pwm_config[0]);
        }
    }

    if (((temp_highest >= 55 && fan_info->pwm_config[0] < 50) || (temp_highest < 40 && fan_info->pwm_config[0] > 60)) && increase_temperature_done) {
        increase_temperature_done = false;
        fan_info->b_in_increase_temperature_flow = true;
    }

    /*Increase the temperature of hash board.*/

    if (fan_info->b_max_fan_pwm || fan_info->b_min_fan_pwm || fan_info->b_in_increase_temperature_flow) {
        continuous_need_to_decrease_pwm = continuous_need_to_increase_pwm = 0;
        /*clear the fan_pwm_limit, maybe the environmental temperature changed.*/
        fan_pwm_low_limit = 0;
        fan_pwm_high_limit = 100;

        goto exit;
    } else if (fan_info->b_max_fan_pwm_quit || fan_info->b_min_fan_pwm_quit) {
        fan_info->b_max_fan_pwm_quit = fan_info->b_min_fan_pwm_quit = false;
        continuous_need_to_decrease_pwm = continuous_need_to_increase_pwm = 0;
        /*clear the fan_pwm_limit, maybe the environmental temperature changed.*/
        fan_pwm_low_limit = 0;
        fan_pwm_high_limit = 100;

        goto exit;
    } else {
        /*update the pwm.*/
        last_pwm_percent = fan_info->pwm_config[0];
    }

    /*keep the temperature stable*/
    if (temp_highest < target_degree_lower_limit) {

        continuous_need_to_increase_pwm = 0;
        continuous_need_to_decrease_pwm++;

        if (continuous_need_to_decrease_pwm == 1) {
            start_temperature_of_continuous_need_to_decrease_pwm = temp_highest;
        }

        if (continuous_need_to_decrease_pwm > 10) {
            if (temp_highest < start_temperature_of_continuous_need_to_decrease_pwm) {
                last_pwm_percent = last_pwm_percent - STEP_FAN_TEMP;
                if (last_pwm_percent < MIN_PWM_PERCENT)
                    last_pwm_percent = MIN_PWM_PERCENT;
                if (last_pwm_percent < fan_pwm_low_limit)
                    last_pwm_percent = fan_pwm_low_limit;

                fan_info->pwm_config[0] = last_pwm_percent;
                EMC2302_set_fan_speed(fan_info->pwm_config[0]);
                ESP_LOGD(TAG, "decrease pwm %d, temp %d <= %d",
                    fan_info->pwm_config[0], temp_highest,
                    start_temperature_of_continuous_need_to_decrease_pwm);
            } else if (temp_highest > start_temperature_of_continuous_need_to_decrease_pwm) {
                last_pwm_percent = last_pwm_percent + STEP_FAN_TEMP;
                if (last_pwm_percent > MAX_PWM_PERCENT) {
                    last_pwm_percent = MAX_PWM_PERCENT;
                }
                if (last_pwm_percent < fan_pwm_high_limit) {
                    last_pwm_percent = fan_pwm_high_limit;
                }

                fan_info->pwm_config[0] = last_pwm_percent;
                EMC2302_set_fan_speed(fan_info->pwm_config[0]);
                ESP_LOGD(TAG, "low increase pwm %d, temp %d <= %d",
                    fan_info->pwm_config[0], temp_highest,
                    start_temperature_of_continuous_need_to_decrease_pwm);
            }

            continuous_need_to_decrease_pwm = 0;
        }
    } else if (temp_highest > target_degree_upper_limit) {
        continuous_need_to_decrease_pwm = 0;
        continuous_need_to_increase_pwm++;

		/*if (continuous_need_to_increase_pwm == 1) {
			start_temperature_of_continuous_need_to_increase_pwm = temp_highest;
		}*/

        if (continuous_need_to_increase_pwm == 5) {
            // if (temp_highest > start_temperature_of_continuous_need_to_increase_pwm) {
            last_pwm_percent = last_pwm_percent + STEP_FAN_TEMP;
            if (last_pwm_percent > MAX_PWM_PERCENT) {
                last_pwm_percent = MAX_PWM_PERCENT;
            }
            if (last_pwm_percent > fan_pwm_high_limit) {
                last_pwm_percent = fan_pwm_high_limit;
            }

            fan_info->pwm_config[0] = last_pwm_percent;
            EMC2302_set_fan_speed(fan_info->pwm_config[0]);
            ESP_LOGD(TAG, "increase pwm %d, temp %d",
                fan_info->pwm_config[0], temp_highest);

            continuous_need_to_increase_pwm = 0;
        }
    } else {
        /*59 -> 60 case. */
        if ((temp_highest == target_degree_lower_limit) && (continuous_need_to_decrease_pwm != 0)) {
            enter_low_limit_pwm = last_pwm_percent;
            if (enter_high_limit_pwm != 0) {
                fan_pwm_high_limit = enter_high_limit_pwm - STEP_FAN_TEMP;
            }
        }

        /*65 -> 64 case. */
        if ((temp_highest == target_degree_upper_limit) && (continuous_need_to_increase_pwm != 0)) {
            enter_high_limit_pwm = last_pwm_percent;
            if (enter_low_limit_pwm != 0) {
                fan_pwm_low_limit = enter_low_limit_pwm + STEP_FAN_TEMP;
            }
        }

        // do nothing, keep the pwm and clear the counter.
        continuous_need_to_increase_pwm = continuous_need_to_decrease_pwm = 0;
    }

exit:
    return;
}
