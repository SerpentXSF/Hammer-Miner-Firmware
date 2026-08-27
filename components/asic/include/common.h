#ifndef COMMON_H_
#define COMMON_H_

#include <stdint.h>
#include <stddef.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

typedef enum
{
    REGISTER_INVALID = 0,
    REGISTER_ERROR_COUNT,
    REGISTER_DOMAIN_0_COUNT,
    REGISTER_DOMAIN_1_COUNT,
    REGISTER_DOMAIN_2_COUNT,
    REGISTER_DOMAIN_3_COUNT,
    REGISTER_TOTAL_COUNT,
} register_type_t;

typedef struct __attribute__((__packed__))
{
    uint8_t job_id;
    uint8_t chip_id;
    uint8_t core_id;
    uint32_t nonce;
    uint32_t rolled_version;
    // ---- register response
    register_type_t register_type;
    uint8_t asic_nr;
    uint32_t value;
} task_result;

#define CHECK_ESP_ERR(err) check_esp_err(err, __func__, __FILE__, __LINE__)

void check_esp_err(esp_err_t err, const char *func, const char *file, int line);

unsigned char _reverse_bits(unsigned char num);
int _largest_power_of_two(int num);
void volc_delay(int ms);
void enc32be(void *dst, uint32_t val);

int count_asic_chips(uint16_t asic_count, uint16_t chip_id, int chip_id_response_length);
esp_err_t receive_work_bm(uint8_t * buffer, int buffer_size);
esp_err_t receive_work(uint8_t * buffer, int buffer_size, uint32_t chain_num);
esp_err_t receive_work_with_large_buffer(uint8_t * buffer, int buffer_size, uint32_t chain_num);
void get_difficulty_mask(uint16_t difficulty, uint8_t *job_difficulty_mask);

static inline uint32_t swab32(uint32_t val) {
    uint32_t ret = 0;
    ret |= (val & 0xFF) << 24;
    ret |= (val & 0xFF00) << 8;
    ret |= (val & 0xFF0000) >> 8;
    ret |= (val & 0xFF000000) >> 24;
    return ret;
}

static inline void flip12(void *dest_p, const void *src_p) {
    uint32_t *dest = dest_p;
    const uint32_t *src = src_p;
    int i;

    for (i = 0; i < 3; i++)
        dest[i] = swab32(src[i]);
}

static inline void flip16(void *dest_p, const void *src_p) {
    uint32_t *dest = dest_p;
    const uint32_t *src = src_p;
    int i;

    for (i = 0; i < 4; i++)
        dest[i] = swab32(src[i]);
}

static inline void flip96(void *dest_p, const void *src_p) {
    uint32_t *dest = dest_p;
    const uint32_t *src = src_p;
    int i;

    for (i = 0; i < 24; i++)
        dest[i] = swab32(src[i]);
}

static inline void rev(unsigned char *s, size_t l) {
    size_t i, j;
    unsigned char t;

    for (i = 0, j = l - 1; i < j; i++, j--) {
        t = s[i];
        s[i] = s[j];
        s[j] = t;
    }
}

static inline void busy_wait_ms(uint32_t ms) {
    TickType_t start_ticks = xTaskGetTickCount();
    TickType_t wait_ticks = pdMS_TO_TICKS(ms);
    
    while ((xTaskGetTickCount() - start_ticks) < wait_ticks) {
        // 空循环
    }
}

#endif