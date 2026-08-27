#include "common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "serial.h"
#include "asic_abstration.h"

#include "crc.h"

#define PREAMBLE 0xAA55

static const char *TAG = "asic_common";

unsigned char _reverse_bits(unsigned char num)
{
    unsigned char reversed = 0;
    int i;

    for (i = 0; i < 8; i++) {
        reversed <<= 1;      // Left shift the reversed variable by 1
        reversed |= num & 1; // Use bitwise OR to set the rightmost bit of reversed to the current bit of num
        num >>= 1;           // Right shift num by 1 to get the next bit
    }

    return reversed;
}

int _largest_power_of_two(int num)
{
    int power = 0;

    while (num > 1) {
        num = num >> 1;
        power++;
    }

    return 1 << power;
}


int count_asic_chips(uint16_t asic_count, uint16_t chip_id, int chip_id_response_length)
{
    uint8_t buffer[11] = {0};

    int chip_counter = 0;
    while (true) {
        int received = SERIAL_rx(0, buffer, chip_id_response_length, 1000, true);
        if (received == 0) break;

        if (received == -1) {
            ESP_LOGE(TAG, "Error reading CHIP_ID");
            break;
        }

        if (received != chip_id_response_length) {
            ESP_LOGE(TAG, "Invalid CHIP_ID response length: expected %d, got %d", chip_id_response_length, received);
            ESP_LOG_BUFFER_HEX(TAG, buffer, received);
            break;
        }

        uint16_t received_preamble = (buffer[0] << 8) | buffer[1];
        if (received_preamble != PREAMBLE) {
            ESP_LOGW(TAG, "Preamble mismatch: expected 0x%04x, got 0x%04x", PREAMBLE, received_preamble);
            ESP_LOG_BUFFER_HEX(TAG, buffer, received);
            continue;
        }

        uint16_t received_chip_id = (buffer[2] << 8) | buffer[3];
        if (received_chip_id != chip_id) {
            ESP_LOGW(TAG, "CHIP_ID response mismatch: expected 0x%04x, got 0x%04x", chip_id, received_chip_id);
            ESP_LOG_BUFFER_HEX(TAG, buffer, received);
            continue;
        }

        if (crc5(buffer + 2, received - 2) != 0) {
            ESP_LOGW(TAG, "Checksum failed on CHIP_ID response");
            ESP_LOG_BUFFER_HEX(TAG, buffer, received);
            continue;
        }

        ESP_LOGI(TAG, "Chip %d detected: CORE_NUM: 0x%02x ADDR: 0x%02x", chip_counter, buffer[4], buffer[5]);

        chip_counter++;
    }    
    
    if (chip_counter != asic_count) {
        ESP_LOGW(TAG, "%i chip(s) detected on the chain, expected %i", chip_counter, asic_count);
    }

    return chip_counter;
}

void volc_delay(int ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void enc32be(void *dst, uint32_t val)
{
	((unsigned char *)dst)[0] = (val >> 24);
	((unsigned char *)dst)[1] = (val >> 16);
	((unsigned char *)dst)[2] = (val >> 8);
	((unsigned char *)dst)[3] = val;
}

// 检查 esp_err_t 返回值，如果不为 ESP_OK，则记录错误并挂起系统
void check_esp_err(esp_err_t err, const char *func, const char *file, int line)
{
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error occurred in %s at %s:%d, err: %s", func, file, line, esp_err_to_name(err));
        while (1) {
            vTaskDelay(portMAX_DELAY); // 挂起系统，无限期等待
        }
    }
}

esp_err_t receive_work(uint8_t * buffer, int buffer_size, uint32_t chain_num)
{
    int received = SERIAL_rx(chain_num, buffer, buffer_size, 10000, true);

    if (received < 0) {
        ESP_LOGE(TAG, "UART error in serial RX");
        return ESP_FAIL;
    }

    if (received == 0) {
        ESP_LOGD(TAG, "UART timeout in serial RX");
        return ESP_FAIL;
    }

    if (received != buffer_size) {
        ESP_LOGE(TAG, "Invalid response length %i", received);
        ESP_LOG_BUFFER_HEX(TAG, buffer, received);
        SERIAL_clear_buffer(chain_num);
        return ESP_FAIL;
    }

    uint16_t received_preamble = *(uint16_t *)buffer;
    if (received_preamble != MS_SYNC_TAG) {
        ESP_LOGE(TAG, "Preamble mismatch: got 0x%04x, expected 0x%04x", received_preamble, MS_SYNC_TAG);
        ESP_LOG_BUFFER_HEX(TAG, buffer, received);
        SERIAL_clear_buffer(chain_num);
        return ESP_FAIL;
    }

    if (crc5_bits(buffer + 2, 64) != buffer[buffer_size - 1]) {
        ESP_LOGE(TAG, "Checksum failed on response");        
        ESP_LOG_BUFFER_HEX(TAG, buffer, received);
        SERIAL_clear_buffer(chain_num);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t receive_work_bm(uint8_t * buffer, int buffer_size)
{
    int received = SERIAL_rx(0, buffer, buffer_size, 10000, true);

    if (received < 0) {
        ESP_LOGE(TAG, "UART error in serial RX");
        return ESP_FAIL;
    }

    if (received == 0) {
        ESP_LOGD(TAG, "UART timeout in serial RX");
        return ESP_FAIL;
    }

    if (received != buffer_size) {
        ESP_LOGE(TAG, "Invalid response length %i", received);
        ESP_LOG_BUFFER_HEX(TAG, buffer, received);
        SERIAL_clear_buffer(0);
        return ESP_FAIL;
    }

    uint16_t received_preamble = (buffer[0] << 8) | buffer[1];
    if (received_preamble != PREAMBLE) {
        ESP_LOGE(TAG, "Preamble mismatch: got 0x%04x, expected 0x%04x", received_preamble, PREAMBLE);
        ESP_LOG_BUFFER_HEX(TAG, buffer, received);
        SERIAL_clear_buffer(0);
        return ESP_FAIL;
    }

    if (crc5(buffer + 2, buffer_size - 2) != 0) {
        ESP_LOGE(TAG, "Checksum failed on response");        
        ESP_LOG_BUFFER_HEX(TAG, buffer, received);
        SERIAL_clear_buffer(0);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t receive_work_with_large_buffer(uint8_t * buffer, int buffer_size, uint32_t chain_num)
{
    uint8_t large_buffer[600];
    int received = SERIAL_rx(chain_num, large_buffer, sizeof(large_buffer)/sizeof(large_buffer[0]), 10000, true);

    if (received < 0) {
        ESP_LOGE(TAG, "UART error in serial RX");
        return ESP_FAIL;
    }

    if (received == 0) {
        ESP_LOGD(TAG, "UART timeout in serial RX");
        return ESP_FAIL;
    }

    if (received != buffer_size) {
        ESP_LOGE(TAG, "Invalid response length %i", received);
        //ESP_LOG_BUFFER_HEX(TAG, large_buffer, received);
        SERIAL_clear_buffer(chain_num);
        return ESP_FAIL;
    }else{
        memcpy(buffer, large_buffer, buffer_size);
    }

    uint16_t received_preamble = *(uint16_t *)buffer;
    if (received_preamble != MS_SYNC_TAG) {
        ESP_LOGE(TAG, "Preamble mismatch: got 0x%04x, expected 0x%04x", received_preamble, MS_SYNC_TAG);
        ESP_LOG_BUFFER_HEX(TAG, buffer, received);
        SERIAL_clear_buffer(chain_num);
        return ESP_FAIL;
    }

    if (crc5_bits(buffer + 2, 64) != buffer[buffer_size - 1]) {
        ESP_LOGE(TAG, "Checksum failed on response");        
        ESP_LOG_BUFFER_HEX(TAG, buffer, received);
        SERIAL_clear_buffer(chain_num);
        return ESP_FAIL;
    }

    return ESP_OK;
}

void get_difficulty_mask(uint16_t difficulty, uint8_t *job_difficulty_mask)
{
    // The mask must be a power of 2 so there are no holes
    // Correct:   {0b00000000, 0b00000000, 0b11111111, 0b11111111}
    // Incorrect: {0b00000000, 0b00000000, 0b11100111, 0b11111111}
    difficulty = _largest_power_of_two(difficulty) - 1;

    job_difficulty_mask[0] = 0x00;
    job_difficulty_mask[1] = 0x14; // TICKET_MASK

    // convert difficulty into char array
    // Ex: 256 = {0b00000000, 0b00000000, 0b00000000, 0b11111111}, {0x00, 0x00, 0x00, 0xff}
    // Ex: 512 = {0b00000000, 0b00000000, 0b00000001, 0b11111111}, {0x00, 0x00, 0x01, 0xff}
    job_difficulty_mask[2] = _reverse_bits((difficulty >> 24) & 0xFF);
    job_difficulty_mask[3] = _reverse_bits((difficulty >> 16) & 0xFF);
    job_difficulty_mask[4] = _reverse_bits((difficulty >>  8) & 0xFF);
    job_difficulty_mask[5] = _reverse_bits( difficulty        & 0xFF);
}
