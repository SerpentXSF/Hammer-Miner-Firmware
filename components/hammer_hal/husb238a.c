#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "hal_i2c.h"
#include "husb238a.h"

static const char *TAG = "HUSB238A";

/* The stock firmware uses 100 ms on every transaction with this part. */
#define HUSB238A_TIMEOUT_MS 100

static i2c_master_dev_handle_t dev_handle;
static bool initialised;

esp_err_t husb238a_init(void)
{
    if (initialised) {
        return ESP_OK;
    }

    esp_err_t ret = hammer_i2c_add_device(HUSB238A_I2CADDR, &dev_handle, TAG);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add HUSB238A at 0x%02x", HUSB238A_I2CADDR);
        return ret;
    }

    /* Confirm it actually answers before anything relies on it. */
    uint8_t status = 0;
    ret = husb238a_read(HUSB238A_REG_CONNECT_STATUS, &status);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "No response at 0x%02x; this board may not be USB-PD powered",
                 HUSB238A_I2CADDR);
        return ret;
    }

    initialised = true;
    ESP_LOGI(TAG, "USB-PD sink controller present at 0x%02x", HUSB238A_I2CADDR);
    return ESP_OK;
}

bool husb238a_present(void)
{
    return initialised;
}

esp_err_t husb238a_read(uint8_t reg, uint8_t *value)
{
    if (dev_handle == NULL || value == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return i2c_master_transmit_receive(dev_handle, &reg, 1, value, 1,
                                       HUSB238A_TIMEOUT_MS);
}

esp_err_t husb238a_write(uint8_t reg, uint8_t value)
{
    if (dev_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t buf[2] = { reg, value };
    return i2c_master_transmit(dev_handle, buf, sizeof(buf), HUSB238A_TIMEOUT_MS);
}

void husb238a_dump(void)
{
    uint8_t value = 0;

    ESP_LOGW(TAG, "--- USB-PD controller state (read-only) ---");

    if (husb238a_read(HUSB238A_REG_CONNECT_STATUS, &value) == ESP_OK) {
        ESP_LOGW(TAG, "  0x%02x connect status    = 0x%02x", HUSB238A_REG_CONNECT_STATUS, value);
    }
    if (husb238a_read(HUSB238A_REG_NEGOTIATED_V, &value) == ESP_OK) {
        ESP_LOGW(TAG, "  0x%02x negotiated voltage= 0x%02x", HUSB238A_REG_NEGOTIATED_V, value);
    }
    if (husb238a_read(HUSB238A_REG_GATE, &value) == ESP_OK) {
        ESP_LOGW(TAG, "  0x%02x gate control      = 0x%02x  (VBUS %s: disable bit 0x%02x is %s)",
                 HUSB238A_REG_GATE, value,
                 (value & HUSB238A_GATE_DISABLE_BIT) ? "OFF" : "ON",
                 HUSB238A_GATE_DISABLE_BIT,
                 (value & HUSB238A_GATE_DISABLE_BIT) ? "set" : "clear");
    }

    /*
     * Sweep the low control block and the status/capability block.
     *
     * Clearing the gate bit alone did not bring the hashboard up, because
     * the controller reports nothing attached and no capabilities: it has
     * not begun negotiating. The stock driver has a husb_soft_enable step
     * that sets bit 0x08 in a register whose number could not be recovered
     * from the binary, so read the plausible ranges and let the device say
     * what state it is actually in.
     */
    ESP_LOGW(TAG, "  registers 0x00-0x0f:");
    for (uint8_t reg = 0x00; reg <= 0x0F; reg++) {
        if (husb238a_read(reg, &value) == ESP_OK) {
            ESP_LOGW(TAG, "    0x%02x = 0x%02x", reg, value);
        }
    }

    ESP_LOGW(TAG, "  registers 0x60-0x70:");
    for (uint8_t reg = 0x60; reg <= 0x70; reg++) {
        if (husb238a_read(reg, &value) == ESP_OK) {
            ESP_LOGW(TAG, "    0x%02x = 0x%02x", reg, value);
        }
    }

    ESP_LOGW(TAG, "-------------------------------------------");
}

/* Read, change one bit, write back -- the shape the stock driver uses. */
static esp_err_t gate_set_disable_bit(bool disable)
{
    uint8_t value = 0;
    esp_err_t ret = husb238a_read(HUSB238A_REG_GATE, &value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not read gate register 0x%02x", HUSB238A_REG_GATE);
        return ret;
    }

    uint8_t updated = disable ? (value | HUSB238A_GATE_DISABLE_BIT)
                              : (value & (uint8_t)~HUSB238A_GATE_DISABLE_BIT);

    if (updated == value) {
        ESP_LOGI(TAG, "VBUS gate already %s (0x%02x)", disable ? "closed" : "open", value);
        return ESP_OK;
    }

    ret = husb238a_write(HUSB238A_REG_GATE, updated);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not write gate register 0x%02x", HUSB238A_REG_GATE);
        return ret;
    }

    ESP_LOGW(TAG, "VBUS output %s (0x%02x -> 0x%02x)",
             disable ? "disabled" : "enabled", value, updated);

    /* Let the rail settle before anything downstream is probed. */
    vTaskDelay(pdMS_TO_TICKS(100));
    return ESP_OK;
}

esp_err_t husb238a_gate_open(void)
{
    return gate_set_disable_bit(false);
}

esp_err_t husb238a_gate_close(void)
{
    return gate_set_disable_bit(true);
}
