#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "soc/uart_struct.h"

#include "serial.h"
#include "utils.h"
#include "miner.h"


static const char *TAG = "serial";

/*IMPORTANT:  chain_num + 1 = uart_num*/
esp_err_t SERIAL_init(uint32_t chain_num)
{
    ESP_LOGI(TAG, "Initializing chain %"PRIu32"", chain_num);
    uint32_t tx_io_num = 0, rx_io_num = 0;
    uint32_t uart_num = chain_num + 1;

    if(0 == chain_num){
        tx_io_num = UART_CHAIN_0_TXD0;
        rx_io_num = UART_CHAIN_0_RXD0;
    }else if(1 == chain_num){
        tx_io_num = UART_CHAIN_1_TXD1;
        rx_io_num = UART_CHAIN_1_RXD1;
    }

    // Configure UART parameters
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
    };
    // Configure UART1 parameters
    ESP_ERROR_CHECK_WITHOUT_ABORT(uart_param_config(uart_num, &uart_config));
    // Set UART1 pins(TX: IO17, RX: I018)
    ESP_ERROR_CHECK_WITHOUT_ABORT(uart_set_pin(uart_num, tx_io_num, rx_io_num, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // Install UART driver (we don't need an event queue here)
    // tx buffer 0 so the tx time doesn't overlap with the job wait time
    //  by returning before the job is written
    return uart_driver_install(uart_num, UART_BUF_SIZE * 2, UART_BUF_SIZE * 2, 0, NULL, 0);
}

esp_err_t SERIAL_set_baud(uint32_t chain_num, int baud)
{
    uint32_t uart_num = chain_num + 1;
    ESP_LOGI(TAG, "Changing UART %" PRIu32 " baud to %i", uart_num, baud);

    // Make sure that we are done writing before setting a new baudrate.
    ESP_ERROR_CHECK_WITHOUT_ABORT(uart_wait_tx_done(uart_num, 1000 / portTICK_PERIOD_MS));
    ESP_ERROR_CHECK_WITHOUT_ABORT(uart_set_baudrate(uart_num, baud));

    return ESP_OK;
}

int SERIAL_send(uint32_t chain_num, uint8_t *data, int len, bool debug)
{
    uint32_t uart_num = chain_num + 1;
    int ret = uart_write_bytes(uart_num, (const char *)data, len);

    /* This tested `false`, so the debug argument every caller passes -- and the
     * BM1370_SERIALTX_DEBUG switch feeding it -- did nothing. Honouring it costs
     * nothing when off, and dumping the frames leaving the ESP32 is the first
     * thing wanted when an ASIC goes quiet. */
    if (debug)
    {
        ESP_LOGI(TAG, "tx: uart %"PRIu32" ret [%d]", uart_num, ret);
        prettyHex((unsigned char *)data, len);
        ESP_LOGI(TAG, "[%d]\n", len);
    }else{
        //vTaskDelay(10 / portTICK_PERIOD_MS); /*delay 10ms.*/
    }
    //uart_flush(uart_num);

    return ret;
}

/// @brief waits for a serial response from the device
/// @param buf buffer to read data into
/// @param buf number of ms to wait before timing out
/// @return number of bytes read, or -1 on error
int16_t SERIAL_rx(uint32_t chain_num, uint8_t *buf, uint16_t size, uint16_t timeout_ms, bool debug)
{
    uint32_t uart_num = chain_num + 1;
    int32_t bytes_read = uart_read_bytes(uart_num, buf, size, timeout_ms / portTICK_PERIOD_MS);

    if(false){
        if (bytes_read > 0) {
            ESP_LOGI(TAG, "rx: uart %" PRIu32 "", uart_num);
            prettyHex((unsigned char*) buf, bytes_read);
            //ESP_LOGI(TAG, " [%" PRIu32 "]\n", bytes_read);
        }
    }

    return bytes_read;
}

void SERIAL_debug_rx(uint32_t chain_num)
{
    int ret;
    uint8_t buf[100];
    uint32_t uart_num = chain_num + 1;

    ret = SERIAL_rx(uart_num, buf, 100, 20, true);
    if (ret < 0)
    {
        fprintf(stderr, "unable to read data\n");
        return;
    }

    memset(buf, 0, 100);
}

void SERIAL_clear_buffer(uint32_t chain_num)
{
    uint32_t uart_num = chain_num + 1;
    uart_flush(uart_num);
}
