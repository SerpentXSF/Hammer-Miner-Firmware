#ifndef _HEALTH_MAINTENNANCE_H
#define _HEALTH_MAINTENNANCE_H

#include "stdint.h"

#include "miner.h"

typedef struct {
    float  cpu_temperature;
    int8_t control_board_temperature;
    int8_t board_temperature[MAX_CHAIN_NUM];

    bool fan_eft;
    uint16_t fan_percent[MAX_PWM_CHANNEL];
    uint16_t fan_rpm[MAX_PWM_CHANNEL];

    float voltage;
    float input_voltage;
    float out_voltage;
    float out_voltage1;
    float out_current;
    float power;
    float max_power;
    int nominal_input_voltage;
}HealthMaintenceModule;

void health_maintenance_task(void *pvParameters);
void health_maintenance_task_lotto(void *pvParameters);
#endif