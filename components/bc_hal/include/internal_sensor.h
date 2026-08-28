#ifndef INTERNAL_SENSOR_H
#define INTERNAL_SENSOR_H

#include "driver/temperature_sensor.h"

void start_internal_temperature_sensor();
esp_err_t read_internal_temperature_sensor(float *temperature);
void stop_internal_temperature_sensor();



#endif