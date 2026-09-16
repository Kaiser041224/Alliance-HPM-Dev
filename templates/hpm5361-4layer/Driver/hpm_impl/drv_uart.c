/*
 * UART Driver Implementation (HPM SDK)
 *
 * Copyright (c) 2024 HPMicro
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "intf_uart.h"

/* HPM SDK headers */
/* #include "hpm_uart_drv.h" */  /* TODO: Uncomment when implementing */
#include "hpm_clock_drv.h"

/* ============================================================================
 * HPM-specific Implementation
 * ============================================================================ */

static int hpm_uart_init(intf_uart_port_t port, const intf_uart_cfg_t *cfg)
{
    /* TODO: Implement using hpm_uart_drv */
    (void)port;
    (void)cfg;
    return 0;
}

static int hpm_uart_transmit(intf_uart_port_t port, const uint8_t *data, size_t len, uint32_t timeout_ms)
{
    /* TODO: Implement */
    (void)port;
    (void)data;
    (void)len;
    (void)timeout_ms;
    return 0;
}

static int hpm_uart_receive(intf_uart_port_t port, uint8_t *data, size_t len, uint32_t timeout_ms)
{
    /* TODO: Implement */
    (void)port;
    (void)data;
    (void)len;
    (void)timeout_ms;
    return 0;
}

static int hpm_uart_register_rx_callback(intf_uart_port_t port, intf_uart_rx_cb_t cb)
{
    /* TODO: Implement */
    (void)port;
    (void)cb;
    return 0;
}

static void hpm_uart_deinit(intf_uart_port_t port)
{
    /* TODO: Implement */
    (void)port;
}

/* ============================================================================
 * Operations Structure
 * ============================================================================ */

static const intf_uart_ops_t hpm_uart_ops = {
    .init               = hpm_uart_init,
    .transmit           = hpm_uart_transmit,
    .receive            = hpm_uart_receive,
    .register_rx_callback = hpm_uart_register_rx_callback,
    .deinit             = hpm_uart_deinit,
};

/* ============================================================================
 * Registration
 * ============================================================================ */

void hpm_uart_driver_register(void)
{
    intf_uart_register(&hpm_uart_ops);
}
