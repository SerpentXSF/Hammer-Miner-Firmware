#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_check.h"

#include "miner.h"
#include "hal_i2c.h"

static const char * TAG = "hammer-i2c";

static i2c_master_bus_handle_t i2c_bus_handle;
static i2c_dev_map_entry_t i2c_device_map[MAX_DEVICES];
static int i2c_device_count = 0;

static esp_err_t log_on_error(esp_err_t err, i2c_master_dev_handle_t handle) 
{
    if (err == ESP_OK) 
	{
        return ESP_OK;
    }

    for (int i = 0; i < i2c_device_count; i++) 
	{
        if (i2c_device_map[i].handle == handle) 
		{
            ESP_LOGE(TAG, "Device %s (0x%02x)", i2c_device_map[i].device_tag, i2c_device_map[i].device_address);
            return err;
        }
    }
    
    ESP_LOGE(TAG, "Unknown device");
    return err;
}

esp_err_t hammer_i2c_init(void)
{   
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_MASTER_NUM,
        .scl_io_num = GPIO_I2C_SCL_0,
        .sda_io_num = GPIO_I2C_SDA_0,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &i2c_bus_handle));
    
    //wait for I2C to init
    vTaskDelay(100 / portTICK_PERIOD_MS);

    hammer_i2c_scan();

    return ESP_OK;
}

/*
 * Log every address that acknowledges on the bus.
 *
 * Sensor addresses differ between BC models and the vendor released the
 * headers for only some of them, so a board whose sensor is not where the
 * table expects looks identical to a board with no sensor at all. This
 * turns that into an answer rather than a guess, and it costs one pass at
 * boot.
 */
void hammer_i2c_scan(void)
{
    int found = 0;

    ESP_LOGI(TAG, "Scanning I2C bus (SDA=%d SCL=%d)", GPIO_I2C_SDA_0, GPIO_I2C_SCL_0);

    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        if (i2c_master_probe(i2c_bus_handle, addr, 50) == ESP_OK) {
            ESP_LOGW(TAG, "  device responding at 0x%02x", addr);
            found++;
        }
    }

    if (found == 0) {
        ESP_LOGE(TAG, "  no devices found -- check hashboard power and wiring");
    } else {
        ESP_LOGI(TAG, "  %d device(s) found", found);
    }
}

esp_err_t hammer_i2c_add_device(uint8_t device_address, 
        i2c_master_dev_handle_t * dev_handle, const char *device_tag)
{
    if (i2c_device_count >= MAX_DEVICES) 
	{
        ESP_LOGE(TAG, "Device map full, cannot add more devices");
        return ESP_FAIL;
    }

    i2c_device_config_t dev_cfg = 
	{
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = device_address,
        .scl_speed_hz = I2C_BUS_SPEED_HZ,
    };

    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, dev_handle), TAG, "Device 0x%02x", device_address);

    i2c_device_map[i2c_device_count].handle = *dev_handle;
    i2c_device_map[i2c_device_count].device_address = device_address;
    strncpy(i2c_device_map[i2c_device_count].device_tag, device_tag, sizeof(i2c_device_map[i2c_device_count].device_tag) - 1);
    i2c_device_map[i2c_device_count].device_tag[sizeof(i2c_device_map[i2c_device_count].device_tag) - 1] = '\0';
    i2c_device_count++;

    return ESP_OK;
}

esp_err_t hammer_i2c_get_bus_handle(i2c_master_bus_handle_t * dev_handle)
{
    *dev_handle = i2c_bus_handle;
    return ESP_OK;
}

esp_err_t hammer_i2c_register_read(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t * read_buf, size_t len)
{
    return log_on_error(i2c_master_transmit_receive(dev_handle, &reg_addr, 1, read_buf, len, I2C_DEFAULT_TIMEOUT), dev_handle);
}

esp_err_t hammer_i2c_register_write_addr(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr)
{
    return log_on_error(i2c_master_transmit(dev_handle, &reg_addr, 1, I2C_DEFAULT_TIMEOUT), dev_handle);
}

esp_err_t hammer_i2c_register_write_byte(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data};

    return log_on_error(i2c_master_transmit(dev_handle, write_buf, 2, I2C_DEFAULT_TIMEOUT), dev_handle);
}

esp_err_t hammer_i2c_register_write_word(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint16_t data)
{
    uint8_t write_buf[3] = {reg_addr, (uint8_t)(data & 0x00FF), (uint8_t)((data & 0xFF00) >> 8)};

    return log_on_error(i2c_master_transmit(dev_handle, write_buf, 3, I2C_DEFAULT_TIMEOUT), dev_handle);
}

esp_err_t hammer_i2c_register_write_bytes(i2c_master_dev_handle_t dev_handle, uint8_t * data, uint8_t len)
{
    return log_on_error(i2c_master_transmit(dev_handle, data, len, I2C_DEFAULT_TIMEOUT), dev_handle);
}
