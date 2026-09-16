/*
 * Application Logic - UART Hello Skeleton
 *
 * Copyright (c) 2024 HPMicro
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * NOTE: App 层禁止包含任何 hpm_* 头文件，只能使用 Interface 头与标准 C。
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "intf_uart.h"

#define APP_UART_PORT     (0U)
#define APP_UART_BAUDRATE (115200U)
#define APP_TX_TIMEOUT_MS (100U)

static const char app_banner[] = "HPM5361 template: hello\r\n";

static bool s_uart_ready;

void app_init(void)
{
    intf_uart_cfg_t cfg = {
        .baudrate = APP_UART_BAUDRATE,
        .data_bits = 8,
        .stop_bits = 1,
        .parity = 0,
        .flow_ctrl = false,
    };

    s_uart_ready = (intf_uart_init(APP_UART_PORT, &cfg) == 0);
}

void app_run(void)
{
    if (!s_uart_ready) {
        return;
    }

    (void)intf_uart_transmit(
        APP_UART_PORT, (const uint8_t *)app_banner, strlen(app_banner), APP_TX_TIMEOUT_MS);
}
