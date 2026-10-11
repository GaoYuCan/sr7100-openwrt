#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/usb_serial_jtag.h"
#include "esp_err.h"

#define BRIDGE_UART UART_NUM_1
#define BRIDGE_TX GPIO_NUM_4
#define BRIDGE_RX GPIO_NUM_5
#define BRIDGE_BAUD 115200

/* Only the USB side receives the startup status. No commands, banners, line
 * conversion, escape sequences or local echo are sent to the target UART. */
static void usb_write_all(const void *data, size_t len)
{
    const uint8_t *p = data;
    while (len) {
        int n = usb_serial_jtag_write_bytes(p, len, pdMS_TO_TICKS(20));
        if (n > 0) { p += n; len -= n; }
        else vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void usb_to_uart(void *arg)
{
    uint8_t buf[256];
    for (;;) {
        int n = usb_serial_jtag_read_bytes(buf, sizeof(buf), pdMS_TO_TICKS(20));
        for (int sent = 0; sent < n;) {
            int k = uart_write_bytes(BRIDGE_UART, buf + sent, n - sent);
            if (k > 0) sent += k;
            else vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}

static void uart_to_usb(void *arg)
{
    uint8_t buf[512];
    for (;;) {
        int n = uart_read_bytes(BRIDGE_UART, buf, sizeof(buf), pdMS_TO_TICKS(10));
        if (n > 0) usb_write_all(buf, n);
    }
}

static bool uart_selftest(void)
{
    uint8_t tx[256], rx[256];
    for (unsigned i = 0; i < sizeof(tx); ++i) tx[i] = i;
    /* Run before assigning external GPIOs; loopback is internal to UART1. */
    ESP_ERROR_CHECK(uart_set_loop_back(BRIDGE_UART, true));
    int written = uart_write_bytes(BRIDGE_UART, tx, sizeof(tx));
    int received = 0;
    while (received < sizeof(rx)) {
        int n = uart_read_bytes(BRIDGE_UART, rx + received,
                               sizeof(rx) - received, pdMS_TO_TICKS(100));
        if (n <= 0) break;
        received += n;
    }
    ESP_ERROR_CHECK(uart_wait_tx_done(BRIDGE_UART, pdMS_TO_TICKS(100)));
    ESP_ERROR_CHECK(uart_set_loop_back(BRIDGE_UART, false));
    ESP_ERROR_CHECK(uart_flush_input(BRIDGE_UART));
    return written == sizeof(tx) && received == sizeof(rx)
           && memcmp(tx, rx, sizeof(tx)) == 0;
}

void app_main(void)
{
    ESP_ERROR_CHECK(gpio_reset_pin(BRIDGE_TX));
    ESP_ERROR_CHECK(gpio_reset_pin(BRIDGE_RX));
    uart_config_t cfg = {
        .baud_rate = BRIDGE_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(BRIDGE_UART, &cfg));
    ESP_ERROR_CHECK(uart_driver_install(BRIDGE_UART, 32768, 4096, 0, NULL, 0));
    usb_serial_jtag_driver_config_t usb = {.tx_buffer_size = 16384, .rx_buffer_size = 4096};
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb));
    if (!uart_selftest()) {
        const char failed[] = "\r\n[C3 bridge: UART self-test FAILED; target pins inactive]\r\n";
        usb_write_all(failed, sizeof(failed) - 1);
        for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_ERROR_CHECK(uart_set_pin(BRIDGE_UART, BRIDGE_TX, BRIDGE_RX,
                                UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(gpio_pullup_en(BRIDGE_RX));
    const char ready[] = "\r\n[C3 bridge READY: 115200 8N1; TX=GPIO4 RX=GPIO5; internal UART self-test PASS]\r\n";
    usb_write_all(ready, sizeof(ready) - 1);
    configASSERT(xTaskCreate(usb_to_uart, "usb_to_uart", 3072, NULL, 10, NULL) == pdPASS);
    configASSERT(xTaskCreate(uart_to_usb, "uart_to_usb", 3072, NULL, 10, NULL) == pdPASS);
}
