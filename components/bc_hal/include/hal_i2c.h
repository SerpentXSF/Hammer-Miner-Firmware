#ifndef HAL_I2C_H
#define HAL_I2C_H

#include "esp_err.h"
#include "driver/i2c_master.h"

#define GPIO_I2C_SDA_0 CONFIG_GPIO_I2C_SDA_0
#define GPIO_I2C_SCL_0 CONFIG_GPIO_I2C_SCL_0

#define GPIO_I2C_SDA_1 CONFIG_GPIO_I2C_SDA_1
#define GPIO_I2C_SCL_1 CONFIG_GPIO_I2C_SCL_1

/*!< I2C master clock frequency */
#define I2C_MASTER_FREQ_HZ 100000   
/*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_NUM 0   
#define I2C_MASTER_TIMEOUT_MS 1000
#define I2C_DEFAULT_TIMEOUT -1  //-1 means wait forever
#define I2C_BUS_SPEED_HZ 100000   /*!< I2C master clock frequency */
#define MAX_DEVICES 8 // Adjust as needed

typedef struct
{
    i2c_master_dev_handle_t handle;
    uint16_t device_address;
    char device_tag[32];
} i2c_dev_map_entry_t;

esp_err_t bc_i2c_init(void);

/* Log every address that acknowledges on the bus. Runs once at init. */
void bc_i2c_scan(void);

/* Report which GPIOs carry an external pull-up. Inputs only; drives
 * nothing. Used to locate a second I2C bus. */
void hammer_gpio_pullup_survey(void);
esp_err_t bc_i2c_add_device(uint8_t device_address, i2c_master_dev_handle_t * dev_handle, const char *device_tag);
esp_err_t bc_i2c_get_bus_handle(i2c_master_bus_handle_t * dev_handle);
esp_err_t bc_i2c_register_read(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t * read_buf, size_t len);
esp_err_t bc_i2c_register_write_addr(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr);
esp_err_t bc_i2c_register_write_byte(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t data);
esp_err_t bc_i2c_register_write_word(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint16_t data);
esp_err_t bc_i2c_register_write_bytes(i2c_master_dev_handle_t dev_handle, uint8_t * data, uint8_t len);

#endif
