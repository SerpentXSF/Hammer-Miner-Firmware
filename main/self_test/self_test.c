#include <stdio.h>
#include <inttypes.h>

#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#include "self_test.h"
#include "nvs_config.h"
#include "lvgl_porting.h"
#include "vcore.h"
#include "asic.h"
#include "device.h"
#include "ip_reporter.h"
#include "network.h"
#include "main.h"
#include "esp_app_desc.h"

static const char *TAG = "self_test";
static GlobalState * GLOBAL_STATE = NULL;

static void tests_done(GlobalState * GLOBAL_STATE, bool test_result) 
{
    GLOBAL_STATE->SELF_TEST_MODULE.result = test_result;
    GLOBAL_STATE->SELF_TEST_MODULE.finished = true;

    if (test_result != true) {
        ESP_LOGW(TAG, "SELF TESTS FAIL !!!");
    }else{
        ESP_LOGI(TAG, "SELF_TEST OK !!!");
        nvs_config_set_u16(NVS_CONFIG_SELF_TEST,1);
    }
}

esp_err_t test_temperature_sensor(float *temperature)
{
    esp_err_t ret = ESP_FAIL;

    start_internal_temperature_sensor();
    ret = read_internal_temperature_sensor(temperature);
    if(ret == ESP_OK){
        ESP_LOGI(TAG, "Temperature: %.2f °C", *temperature);
    }else{
        ESP_LOGW(TAG, "Failed to read temperature");
    }
    stop_internal_temperature_sensor();

    return ret;
}


#define CONFIG_GPIO_PLUG_SENSE      11
#define GPIO_PLUG_SENSE  CONFIG_GPIO_PLUG_SENSE
#include "TPS546.h"
static TPS546_CONFIG TPS546_CONFIG_LOTTO = {
    /* vin voltage */
    .TPS546_INIT_VIN_ON = 4.8,
    .TPS546_INIT_VIN_OFF = 4.5,
    .TPS546_INIT_VIN_UV_WARN_LIMIT = 0, //Set to 0 to ignore. TI Bug in this register
    .TPS546_INIT_VIN_OV_FAULT_LIMIT = 6.5,
    /* vout voltage */
    .TPS546_INIT_SCALE_LOOP = 0.125,
    .TPS546_INIT_VOUT_MIN = 4,
    .TPS546_INIT_VOUT_MAX = 5,
    .TPS546_INIT_VOUT_COMMAND = 1.2,
    /* iout current */
    .TPS546_INIT_IOUT_OC_WARN_LIMIT = 25.00, /* A */
    .TPS546_INIT_IOUT_OC_FAULT_LIMIT = 30.00 /* A */  
};

esp_err_t test_power(int i2c_master_index)
{
    esp_err_t ret = ESP_FAIL;

    // configure plug sense, if present
    // Configure plug sense pin as input(barrel jack) 1 is plugged in
    gpio_config_t barrel_jack_conf = {
        .pin_bit_mask = (1ULL << GPIO_PLUG_SENSE),
        .mode = GPIO_MODE_INPUT,
    };
    gpio_config(&barrel_jack_conf);
    int barrel_jack_plugged_in = gpio_get_level(GPIO_PLUG_SENSE);
    ESP_LOGI(TAG, "TPS546 power good %d", barrel_jack_plugged_in);

    if(DEVICE_DC04 == GLOBAL_STATE->device_model)
    {
        TPS546_CONFIG_LOTTO.TPS546_INIT_VOUT_MAX = 3.0;
        TPS546_CONFIG_LOTTO.TPS546_INIT_SCALE_LOOP = 0.25;
    }
    else if(DEVICE_DC06 == GLOBAL_STATE->device_model)
    {
        TPS546_CONFIG_LOTTO.TPS546_INIT_VOUT_MAX = 4.5;
        TPS546_CONFIG_LOTTO.TPS546_INIT_SCALE_LOOP = 0.125;
    }
    else if(DEVICE_BC04 == GLOBAL_STATE->device_model)
    {
        TPS546_CONFIG_LOTTO.TPS546_INIT_VOUT_MAX = 5;
        TPS546_CONFIG_LOTTO.TPS546_INIT_SCALE_LOOP = 0.125;
        GLOBAL_STATE->HEALTH_MODULE.voltage = 400;
        GLOBAL_STATE->asic_vol_default = 400;
    }
    else if(DEVICE_BC08 == GLOBAL_STATE->device_model)
    {
        TPS546_CONFIG_LOTTO.TPS546_INIT_VOUT_MAX = 5;
        TPS546_CONFIG_LOTTO.TPS546_INIT_SCALE_LOOP = 0.125;
        GLOBAL_STATE->HEALTH_MODULE.voltage = 400;
        GLOBAL_STATE->asic_vol_default = 400;
    }
    else if(DEVICE_BC06 == GLOBAL_STATE->device_model)
    {
        TPS546_CONFIG_LOTTO.TPS546_INIT_VOUT_MAX = 390;
        TPS546_CONFIG_LOTTO.TPS546_INIT_SCALE_LOOP = 0.125;
        GLOBAL_STATE->HEALTH_MODULE.voltage = 300;
        GLOBAL_STATE->asic_vol_default = 300;
    }
    else if(DEVICE_BC01 == GLOBAL_STATE->device_model)
    {
        /*
         * Single BM1370, so the core domain is one ASIC rather than a
         * series string: roughly 1.2 V, not the 3-4 V of the multi-ASIC
         * boards. The vendor's BC01 self-test source was not released, so
         * these figures come from a running BC01 on 2.0.1 reporting
         * coreVoltage 119 (1.19 V nominal) against 1195 mV measured.
         *
         * VOUT_MAX is deliberately held just above that operating point.
         * Do not raise it to the 5 V the multi-ASIC boards use: on a
         * single-ASIC domain that clamp would permit a voltage far outside
         * the chip's safe range.
         */
        TPS546_CONFIG_LOTTO.TPS546_INIT_VOUT_MAX = 1.5;
        TPS546_CONFIG_LOTTO.TPS546_INIT_SCALE_LOOP = 0.125;
        GLOBAL_STATE->HEALTH_MODULE.voltage = 120;
        GLOBAL_STATE->asic_vol_default = 120;
    }
    else if(DEVICE_BC02 == GLOBAL_STATE->device_model)
    {
        /*
         * Not calibrated. BC02 is a two-ASIC board, so neither the BC01
         * single-ASIC limits nor the BC04 four-ASIC limits apply, and no
         * BC02 hardware was available to measure. Refusing is the safe
         * outcome: running the self-test with the wrong VOUT_MAX risks the
         * ASIC. Normal mining is unaffected -- it takes its voltage from
         * NVS through power_on_hashboard(), not from here.
         */
        ESP_LOGE(TAG, "Self-test is not calibrated for BC02; refusing to set "
                      "PMBus voltage limits. Normal operation is unaffected.");
        return ESP_ERR_NOT_SUPPORTED;
    }
    ret = TPS546_init(TPS546_CONFIG_LOTTO);

    ESP_LOGI(TAG, "test_power done.");
    return ret;      
}

esp_err_t test_power_on(float *vin, float *vout)
{
    esp_err_t ret = ESP_OK;

    power_on_hashboard(GLOBAL_STATE);
    volc_delay(3000);
    *vin = TPS546_get_vin();
    *vout = TPS546_get_vout();
    ESP_LOGI(TAG, "Power in %.2fv, out %.2fv",*vin, *vout);
    if(*vin < 11)
    {
        ret = ESP_FAIL;
    }
    if(*vout < (GLOBAL_STATE->HEALTH_MODULE.voltage/100 * 0.6) )
    {
        ret = ESP_FAIL;
    }
    return ret;      
}

esp_err_t test_hashboard(void)
{
    esp_err_t ret = ESP_OK;

    /*reset the hashboard*/
    reset_hash_board(GLOBAL_STATE);
    volc_delay(1000);
    GLOBAL_STATE->interface_initalized = true;
    /* Every BC board starts the detect pass at a low frequency. */
    if(DEVICE_BC01 == GLOBAL_STATE->device_model ||
       DEVICE_BC02 == GLOBAL_STATE->device_model ||
       DEVICE_BC04 == GLOBAL_STATE->device_model ||
       DEVICE_BC06 == GLOBAL_STATE->device_model ||
       DEVICE_BC08 == GLOBAL_STATE->device_model)
    {
        GLOBAL_STATE->asic_freqency = 100;
    }
    ret = ASIC_detect(GLOBAL_STATE);
    return ret;      
}

esp_err_t test_external_temperature_sensor(int8_t *t)
{
    esp_err_t ret = ESP_FAIL;

    /*
     * This duplicated device.c's thermal identify logic, once per model,
     * and had no BC01 or BC02 branch -- so those boards fell through and
     * the model was rewritten, which put the device into a reboot loop
     * flipping BC04 -> BC08 -> BC04 and rewriting NVS on every cycle.
     *
     * It now shares the profile table in device.c. A model whose sensor
     * address does not distinguish it from another model is never
     * reconfigured on a missing sensor, because guessing trades a wrong
     * reading for a boot loop.
     */
    uint8_t primary = 0, secondary = 0;
    const char *fallback = NULL;

    if (device_thermal_addresses(GLOBAL_STATE->device_model, &primary, &secondary, &fallback) != ESP_OK) {
        ESP_LOGE(TAG, "No thermal profile for model %s", GLOBAL_STATE->device_model_str);
        return ESP_ERR_NOT_SUPPORTED;
    }

    ret = TMP75_init(primary, 0);
    ret = TMP75_installed(0);
    if (ESP_OK == ret) {
        *t = TMP75_read_temperature(0);
        ESP_LOGI(TAG, "external_temperature_sensor Temperature: %d C", *t);
    } else if (NULL != fallback) {
        ESP_LOGE(TAG, "DEVICE model %s identify mismatch, reset model to %s",
                 GLOBAL_STATE->device_model_str, fallback);
        nvs_config_set_string(NVS_CONFIG_DEVICE_MODEL, fallback);
        vTaskDelay(pdMS_TO_TICKS(1000));
        restart_with_reason("Device model identify mismatch");
    } else {
        ESP_LOGE(TAG, "DEVICE model %s: no temperature sensor at 0x%02x",
                 GLOBAL_STATE->device_model_str, primary);
    }

    return ret;
}


esp_err_t test_fan(uint16_t * rpm)
{
    esp_err_t ret = ESP_FAIL;
    int pwm_conf[] = {0, 50, 80, 100};

	ret = EMC2302_init();
	if (ret != ESP_OK) {
        return ret;
    }
	ret = EMC2302_installed(false);
    if (ret != ESP_OK) {
        return ret;
    }

    for(int i = 0; i < sizeof(pwm_conf)/sizeof(pwm_conf[0]); i++)
    {
        ret = ESP_ERROR_CHECK_WITHOUT_ABORT(EMC2302_set_fan_speed(pwm_conf[i]));
        vTaskDelay(pdMS_TO_TICKS(5000));
        ret = ESP_ERROR_CHECK_WITHOUT_ABORT(EMC2302_get_fan_speed(rpm));
        ESP_LOGI(TAG, "Fan %d%%,speed is %d rpm", pwm_conf[i],*rpm);
        if(pwm_conf[i] < 10)
        {
            if(*rpm > 1000)
            {
                return ESP_FAIL;
            }
        }
        else if(pwm_conf[i] > 90)
        {
            if(*rpm < 1800)
            {
                return ESP_FAIL;
            }
        }
    }
    return ret;
}

esp_err_t test_eth(char * ip)
{
    network_eth_init_test();
	
    int i = 20;
    while(i--)
    {
        vTaskDelay(pdMS_TO_TICKS(2000));
        if(get_eth_ip(ip))
        {
            if(strcmp(ip,"0.0.0.0"))
            {
                return ESP_OK;
            }
        }
    }
    return ESP_FAIL;
}

/*api function */
bool should_test(void)
{
    bool ret = true;
    uint16_t self_test = nvs_config_get_u16(NVS_CONFIG_SELF_TEST, 0);
    
    if(0 == self_test){
        ret = true;
    }else if(1 == self_test){
        ret = false;
    }else{
        ESP_LOGW(TAG, "NVS ERROR: self_test %"PRIu16"", self_test);
    }

    return ret;
}


void self_test(GlobalState *global_state)
{
    GLOBAL_STATE = global_state;
    esp_err_t ret = ESP_FAIL;
    char test_str[1000] = "Self Tests : ";
    char test_item_str[64];
    strcat(test_str, GLOBAL_STATE->device_model_str);
    strcat(test_str, "  ");
    strcat(test_str, esp_app_get_description()->version);
    strcat(test_str, "\n\n");
    GLOBAL_STATE->SELF_TEST_MODULE.active = true;
    GLOBAL_STATE->SELF_TEST_MODULE.message = test_str;

    ESP_LOGW(TAG, "Running Self Tests");
    logMessage(GLOBAL_STATE->SELF_TEST_MODULE.message);

    float t = 0;
    if((ret = test_temperature_sensor(&t)) != ESP_OK){
        ESP_LOGI(TAG, "Internal Temperature Sensor test failed, %d, %s", ret, esp_err_to_name(ret));
        strcat(GLOBAL_STATE->SELF_TEST_MODULE.message, "interanl temp fail!\n");
        logMessage(GLOBAL_STATE->SELF_TEST_MODULE.message);
        tests_done(GLOBAL_STATE, false);
        return;
    }
    else
    {
        sprintf(test_item_str,"internal temp: %0.2f\n",t);
        strcat(GLOBAL_STATE->SELF_TEST_MODULE.message, test_item_str);
        logMessage(GLOBAL_STATE->SELF_TEST_MODULE.message);
    }
        
    int8_t temp[2] = {0};
    if(ESP_OK != (ret = test_external_temperature_sensor(temp))){
        ESP_LOGE(TAG, "temperature_sensor test failed, %d, %s", ret, esp_err_to_name(ret));
        strcat(GLOBAL_STATE->SELF_TEST_MODULE.message, "temp sensor fail!\n");
        logMessage(GLOBAL_STATE->SELF_TEST_MODULE.message);
        tests_done(GLOBAL_STATE, false);
        return;
    }
    else
    {
        if(DEVICE_BC08 == GLOBAL_STATE->device_model || DEVICE_BC06 == GLOBAL_STATE->device_model)
        {
            sprintf(test_item_str,"temp sensor %d, %d\n",temp[0],temp[1]);
        }
        else
        {
            sprintf(test_item_str,"temp sensor %d\n",temp[0]);
        }
        strcat(GLOBAL_STATE->SELF_TEST_MODULE.message, test_item_str);
        logMessage(GLOBAL_STATE->SELF_TEST_MODULE.message);
    }

    uint16_t rpm = 0;
    if((ret = test_fan(&rpm)) != ESP_OK){
        ESP_LOGI(TAG, "Fan test failed, %d, %s", ret, esp_err_to_name(ret));
        strcat(GLOBAL_STATE->SELF_TEST_MODULE.message, "FAN fail!\n");
        logMessage(GLOBAL_STATE->SELF_TEST_MODULE.message);
        tests_done(GLOBAL_STATE, false);
        return;
    }
    else
    {
        sprintf(test_item_str,"PWM 100%%,  FAN %d rpm\n",rpm);
        strcat(GLOBAL_STATE->SELF_TEST_MODULE.message, test_item_str);
        logMessage(GLOBAL_STATE->SELF_TEST_MODULE.message);
    }

    char ip[24];
    if((ret = test_eth(ip)) != ESP_OK){
        ESP_LOGI(TAG, "ETH test failed, %d, %s", ret, esp_err_to_name(ret));
        strcat(GLOBAL_STATE->SELF_TEST_MODULE.message, "ETH fail!\n");
        logMessage(GLOBAL_STATE->SELF_TEST_MODULE.message);
        tests_done(GLOBAL_STATE, false);
        return;
    }
    else
    {
        sprintf(test_item_str,"ETH IP %s\n",ip);
        strcat(GLOBAL_STATE->SELF_TEST_MODULE.message, test_item_str);
        logMessage(GLOBAL_STATE->SELF_TEST_MODULE.message);
    }

    if(ESP_OK != (ret = test_power(I2C_MASTER_INDEX_OF_POWER))){
        ESP_LOGE(TAG, "power test failed, %d, %s", ret, esp_err_to_name(ret));
        strcat(GLOBAL_STATE->SELF_TEST_MODULE.message, "Power init fail!\n");
        logMessage(GLOBAL_STATE->SELF_TEST_MODULE.message);
        tests_done(GLOBAL_STATE, false);
        return;
    }
    else
    {
        strcat(GLOBAL_STATE->SELF_TEST_MODULE.message, "Power init OK\n");
        logMessage(GLOBAL_STATE->SELF_TEST_MODULE.message);
    }

    float vin,vout;
    if(ESP_OK != (ret = test_power_on(&vin, &vout))){
        ESP_LOGE(TAG, "power on test failed, %d, %s", ret, esp_err_to_name(ret));
        strcat(GLOBAL_STATE->SELF_TEST_MODULE.message, "Power on fail!\n");
        logMessage(GLOBAL_STATE->SELF_TEST_MODULE.message);
        tests_done(GLOBAL_STATE, false);
        return;
    }
    else
    {
        sprintf(test_item_str,"Power vin %0.2fV, vout %0.2fV\n",vin,vout);
        strcat(GLOBAL_STATE->SELF_TEST_MODULE.message, test_item_str);
        logMessage(GLOBAL_STATE->SELF_TEST_MODULE.message);
    }

    if(ESP_OK != (ret = test_hashboard())){
        ESP_LOGE(TAG, "hashboard test failed, %d, %s", ret, esp_err_to_name(ret));
        sprintf(test_item_str,"hashboard test fail! asic=%d\n",GLOBAL_STATE->asic_count[0]);
        strcat(GLOBAL_STATE->SELF_TEST_MODULE.message, test_item_str);
        logMessage(GLOBAL_STATE->SELF_TEST_MODULE.message);
        tests_done(GLOBAL_STATE, false);
        return;
    }
    else
    {
        sprintf(test_item_str,"hashboard OK, asic=%d\n",GLOBAL_STATE->asic_count[0]);
        strcat(GLOBAL_STATE->SELF_TEST_MODULE.message, test_item_str);
        logMessage(GLOBAL_STATE->SELF_TEST_MODULE.message);
    }

    strcat(GLOBAL_STATE->SELF_TEST_MODULE.message, "Tests ok!\n");
    logMessage(GLOBAL_STATE->SELF_TEST_MODULE.message);
    tests_done(GLOBAL_STATE, true);
}
