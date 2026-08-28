#ifndef TMP75_H
#define TMP75_H

#include <esp_err.h>

/*
temperature sensor: TMP75 {0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F}
*/
#define TMP75_TEMP_REG 0x00         ///< Temperature register
#define TMP75_CONFIG_REG 0x01       ///< Configuration register
#define TMP75_LOW_LIMIT 0x02        ///< Low limit register
#define TMP75_HIGH_LIMIT 0x03       ///< High limit register
#define TMP75_DEVICE_ID 0x0F        ///< Device ID register

#define TEMPERATURE_SENSOR_MAX_NUM  	8

#define TMP75_I2CADDR_DC04       	(0x48+4)
#define TMP75_I2CADDR_DC06       	(0x48+7)

#define TMP75_I2CADDR_BC04       	(0x48+0)
#define TMP75_I2CADDR_BC08_1       	(0x48+6)
#define TMP75_I2CADDR_BC08_2       	(0x48+2)

/* Measured on a BC01 V02: the sensor answers at 0x49, not the 0x48 the
 * vendor header claimed. 0x48 is the BC04 address; on a BC01 nothing
 * acknowledges there, which sent model detection into a reboot loop.
 * Confirmed with bc_i2c_scan(). */
#define TMP75_I2CADDR_BC01       	(0x48+1)
#define TMP75_I2CADDR_BC01_Pro      (0x48+5)


esp_err_t TMP75_init(uint8_t slave_addr, int temperature_sensor_index);
esp_err_t TMP75_installed(int temperature_sensor_index);
int8_t    TMP75_read_temperature(int temperature_sensor_index);

#endif 
