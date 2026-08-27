#ifndef SERIAL_H_
#define SERIAL_H_

#include <stdbool.h>
#include <esp_err.h>

#include "driver/uart.h"

#define MAX_UART_NUM    2

#define CHUNK_SIZE 1024
#define UART_BUF_SIZE (1024)

typedef enum
{
    JOB_PACKET = 0,
    CMD_PACKET = 1,
} packet_type_t;

int SERIAL_send(uint32_t, uint8_t *, int, bool);
esp_err_t SERIAL_init(uint32_t);
void SERIAL_debug_rx(uint32_t);
int16_t SERIAL_rx(uint32_t, uint8_t *, uint16_t, uint16_t, bool);
void SERIAL_clear_buffer(uint32_t);
esp_err_t SERIAL_set_baud(uint32_t, int);

#endif /* SERIAL_H_ */