#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "device.h"
#include "pwm_fan.h"

static const char *TAG = "pwm_fan";
static pcnt_unit_handle_t pcnt_units[2];

/* Warning:
 * For ESP32, ESP32S2, ESP32S3, ESP32C3, ESP32C2, ESP32C6, ESP32H2, ESP32P4 targets,
 * when LEDC_DUTY_RES selects the maximum duty resolution (i.e. value equal to SOC_LEDC_TIMER_BIT_WIDTH),
 * 100% duty cycle is not reachable (duty cannot be set to (2 ** SOC_LEDC_TIMER_BIT_WIDTH)).
 */
esp_err_t ledc_pwm_init(ledc_channel_t pwm_channel)
{
    esp_err_t ret = ESP_FAIL;
    
    // Prepare and then apply the LEDC PWM timer configuration
    if(LEDC_CHANNEL_0 == pwm_channel){
        ledc_timer_config_t ledc_timer = {
            .speed_mode       = LEDC_MODE,
            .duty_resolution  = LEDC_DUTY_RES,
            .timer_num        = LEDC_TIMER,
            .freq_hz          = LEDC_FREQUENCY,  // Set output frequency at 8 kHz
            .clk_cfg          = LEDC_AUTO_CLK
        };
        ret = ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_timer_config(&ledc_timer));

        // Prepare and then apply the LEDC PWM channel configuration
        ledc_channel_config_t ledc_channel = {
            .speed_mode     = LEDC_MODE,
            .channel        = LEDC_CHANNEL_0,
            .timer_sel      = LEDC_TIMER,
            .intr_type      = LEDC_INTR_DISABLE,
            .gpio_num       = LEDC_PWM_OUTPUT_IO0,
            .duty           = 0, // Set duty to 0%
            .hpoint         = 0
        };
        ret = ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_channel_config(&ledc_channel));
    }else if(LEDC_CHANNEL_1 == pwm_channel){
        // Prepare and then apply the LEDC PWM timer configuration
        ledc_timer_config_t ledc_timer2 = {
            .speed_mode       = LEDC_MODE,
            .duty_resolution  = LEDC_DUTY_RES,
            .timer_num        = LEDC_TIMER_1,
            .freq_hz          = LEDC_FREQUENCY,  // Set output frequency at 4 kHz
            .clk_cfg          = LEDC_AUTO_CLK
        };
        ret = ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_timer_config(&ledc_timer2));

        // Prepare and then apply the LEDC PWM channel configuration
        ledc_channel_config_t ledc_channel2 = {
            .speed_mode     = LEDC_MODE,
            .channel        = LEDC_CHANNEL_1,
            .timer_sel      = LEDC_TIMER_1,
            .intr_type      = LEDC_INTR_DISABLE,
            .gpio_num       = LEDC_PWM_OUTPUT_IO1,
            .duty           = 0, // Set duty to 0%
            .hpoint         = 0
        };
        ret = ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_channel_config(&ledc_channel2));
    }

    return ret;
}

esp_err_t ledc_set_pwm(ledc_channel_t pwm_channel, int pwm_percent)
{
    int duty = 0;
    esp_err_t ret = ESP_FAIL;

    if(pwm_percent > 100 || pwm_percent < 0){
        return ret;
    }

#if LEDC_DUTY_RES == LEDC_TIMER_13_BIT
    duty = pwm_percent*8192/100;
#else
    assert(false);
#endif

    // Set duty to pwm_channel
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, pwm_channel, duty));
    // Update duty to apply the new value
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, pwm_channel));

    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGD(TAG, "Channel %d: Set fan pwm to %d, duty %d", pwm_channel, pwm_percent, duty);

    return ret = ESP_OK;
}

void prepare_fan_pcnt(pcnt_unit_handle_t *pcnt_unit, int edge_gpio_num)
{
    ESP_LOGD(TAG, "install pcnt unit");
    pcnt_unit_config_t unit_config = {
        .high_limit = MAX_FAN_SPEED,
        .low_limit = -MAX_FAN_SPEED,
        .flags = {
            .accum_count = true,
        }
    };

    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, pcnt_unit));

    ESP_LOGD(TAG, "set glitch filter");
    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(*pcnt_unit, &filter_config));

    ESP_LOGD(TAG, "install pcnt channels");
    pcnt_chan_config_t chan_config = {
        .edge_gpio_num = edge_gpio_num,
        .level_gpio_num = -1,
        .flags = {
            .virt_level_io_level = 0,
        }
    };
    pcnt_channel_handle_t pcnt_chan= NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(*pcnt_unit, &chan_config, &pcnt_chan));

    ESP_LOGD(TAG, "set edge and level actions for pcnt channels");
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_KEEP));
}

void restart_pcnt(pcnt_unit_handle_t *pcnt_unit)
{
    ESP_LOGD(TAG, "enable pcnt unit");
    ESP_ERROR_CHECK(pcnt_unit_enable(*pcnt_unit));
    ESP_LOGD(TAG, "clear pcnt unit");
    ESP_ERROR_CHECK(pcnt_unit_clear_count(*pcnt_unit));
    ESP_LOGD(TAG, "start pcnt unit");
    ESP_ERROR_CHECK(pcnt_unit_start(*pcnt_unit));
}

esp_err_t fan_pcnts_init(ledc_channel_t pwm_channel)
{
    esp_err_t ret = ESP_OK;

    if(LEDC_CHANNEL_0 == pwm_channel){
        prepare_fan_pcnt(pcnt_units, PULSE_COUNTER0_GPIO_NUM0);
        restart_pcnt(pcnt_units);
    }else if(LEDC_CHANNEL_1 == pwm_channel){
        prepare_fan_pcnt(pcnt_units+1, PULSE_COUNTER1_GPIO_NUM1);
        restart_pcnt(pcnt_units+1);
    }else{
        ret = ESP_FAIL;
    }

    return ret;
}

esp_err_t fan_pcnts_restart(ledc_channel_t pwm_channel)
{
    esp_err_t ret = ESP_OK;

    if(LEDC_CHANNEL_0 == pwm_channel){   
        restart_pcnt(pcnt_units);
    }else if(LEDC_CHANNEL_1 == pwm_channel){
        restart_pcnt(pcnt_units+1);
    }else{
        ret = ESP_FAIL;
    }

    return ret;
}

esp_err_t fan_pcnts_get_counter(ledc_channel_t pwm_channel, int *ret_pcounts)
{
    esp_err_t ret = ESP_OK;

    if(LEDC_CHANNEL_0 == pwm_channel){   
        ret = ESP_ERROR_CHECK_WITHOUT_ABORT(pcnt_unit_get_count(pcnt_units[0], ret_pcounts));
        ret = ESP_ERROR_CHECK_WITHOUT_ABORT(pcnt_unit_clear_count(pcnt_units[0]));
    }else if(LEDC_CHANNEL_1 == pwm_channel){
        ret = ESP_ERROR_CHECK_WITHOUT_ABORT(pcnt_unit_get_count(pcnt_units[1], ret_pcounts));
        ret = ESP_ERROR_CHECK_WITHOUT_ABORT(pcnt_unit_clear_count(pcnt_units[1]));
    }else{
        ret = ESP_FAIL;
    }

    ESP_LOGD(TAG, "Channel %d: pulse counter %d", pwm_channel, *ret_pcounts);

    return ret;
}

esp_err_t fan_pcnts_clear_counter(ledc_channel_t pwm_channel)
{
    esp_err_t ret = ESP_OK;

    ret = ESP_ERROR_CHECK_WITHOUT_ABORT(pcnt_unit_clear_count(pcnt_units[pwm_channel]));

    return ret;
}

esp_err_t fan_pcnts_get_rpm(ledc_channel_t pwm_channel, int *ret_pcounts, uint32_t mseconds)
{
    esp_err_t ret = ESP_OK;

    ret = fan_pcnts_get_counter(pwm_channel, ret_pcounts);
    *ret_pcounts = (uint32_t)(*ret_pcounts)*1000*60/mseconds/2;

    return ret;
}

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
        ledc_set_pwm(LEDC_CHANNEL_0, fan_info->pwm_config[0]);
        ledc_set_pwm(LEDC_CHANNEL_1, fan_info->pwm_config[1]);
        executed = true;
        fan_info->b_max_fan_pwm = true;
        fan_info->b_max_fan_pwm_quit = false;

		ESP_LOGD(TAG, "ERRORMSG Temp: %d, Max Fan,  back up PWM %d.", temp_highest, bak_last_pwm);
    }else if((temp_highest <= MAX_FAN_TEMP_QUIT) && (fan_info->pwm_config[0] == MAX_PWM_PERCENT)){ 		
        fan_info->pwm_config[0] = fan_info->pwm_config[1] = (bak_last_pwm + STEP_FAN_TEMP);
		ledc_set_pwm(LEDC_CHANNEL_0, fan_info->pwm_config[0]);
        ledc_set_pwm(LEDC_CHANNEL_1, fan_info->pwm_config[1]);
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
        ledc_set_pwm(LEDC_CHANNEL_0, fan_info->pwm_config[0]);
        ledc_set_pwm(LEDC_CHANNEL_1, fan_info->pwm_config[1]);

        fan_info->b_min_fan_pwm = true;
        fan_info->b_min_fan_pwm_quit = false;

        executed = true;
    }else if(temp_highest >= MIN_FAN_TEMP_QUIT && fan_info->pwm_config[1] == MIN_PWM_PERCENT){
		fan_info->pwm_config[0] = fan_info->pwm_config[1] = enter_50_degree_pwm - 20;/*40 degree.*/
        ledc_set_pwm(LEDC_CHANNEL_0, fan_info->pwm_config[0]);
        ledc_set_pwm(LEDC_CHANNEL_1, fan_info->pwm_config[1]);

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
		
		ledc_set_pwm(LEDC_CHANNEL_0, fan_info->pwm_config[0]);
        ledc_set_pwm(LEDC_CHANNEL_1, fan_info->pwm_config[1]);
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
				ledc_set_pwm(LEDC_CHANNEL_0, fan_info->pwm_config[0]);
				ledc_set_pwm(LEDC_CHANNEL_1, fan_info->pwm_config[1]);                
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
			ledc_set_pwm(LEDC_CHANNEL_0, fan_info->pwm_config[0]);
			ledc_set_pwm(LEDC_CHANNEL_1, fan_info->pwm_config[1]);   
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


/*
 * Where the automatic thermal controller below writes its duty cycle.
 *
 * The controller used to call ledc_set_pwm() directly, which is correct only
 * on boards whose fan hangs off LEDC. A BC04 drives its fans through an
 * EMC2302 and never initialises LEDC, so the first temperature reading after
 * the ASICs came up aborted the miner inside ledc_set_pwm's ESP_ERROR_CHECK
 * and it rebooted into the same failure every time.
 *
 * The output is a hook rather than a model test because this is the hardware
 * layer: it has no business knowing which board it is on. device.c registers
 * the right sink once it has identified the board.
 */
static esp_err_t fan_output_ledc(int pwm_percent)
{
    return ledc_set_pwm(LEDC_CHANNEL_0, pwm_percent);
}

static fan_output_fn fan_output = fan_output_ledc;

void pwm_fan_set_output(fan_output_fn fn)
{
    fan_output = (NULL != fn) ? fn : fan_output_ledc;
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
        fan_output(fan_info->pwm_config[0]);

        executed = true;
        fan_info->b_max_fan_pwm = true;
        fan_info->b_max_fan_pwm_quit = false;

        ESP_LOGI(TAG, "Temp: %d, Max Fan,  back up PWM %d.", temp_highest, fan_info->pwm_config[0]);
    } else if ((temp_highest <= MAX_FAN_TEMP_QUIT) && (fan_info->pwm_config[0] == MAX_PWM_PERCENT)) {
        fan_info->pwm_config[0] = (bak_last_pwm + STEP_FAN_TEMP);
        fan_output(fan_info->pwm_config[0]);

        fan_info->b_max_fan_pwm = false;
        fan_info->b_max_fan_pwm_quit = true;
        executed = true;
        ESP_LOGI(TAG, "Temp: %d, Quit Max Fan,  set PWM %d.", temp_highest, fan_info->pwm_config[0]);
    }

    /*The low temperaturen protection and quit mechanism of hash board.*/
    if (temp_highest <= MIN_FAN_TEMP) {
        fan_info->pwm_config[0] = MIN_PWM_PERCENT;
        fan_output(fan_info->pwm_config[0]);

        fan_info->b_min_fan_pwm = true;
        fan_info->b_min_fan_pwm_quit = false;
        executed = true;


    } else if (temp_highest >= MIN_FAN_TEMP_QUIT && fan_info->pwm_config[0] == MIN_PWM_PERCENT) {
        fan_info->pwm_config[0] = enter_50_degree_pwm - 40;  /*40 degree.*/
        fan_output(fan_info->pwm_config[0]);

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
            fan_output(fan_info->pwm_config[0]);
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
                fan_output(fan_info->pwm_config[0]);
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
                fan_output(fan_info->pwm_config[0]);
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
            fan_output(fan_info->pwm_config[0]);
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
