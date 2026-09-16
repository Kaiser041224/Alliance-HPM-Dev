/*
 * Main Entry Point
 *
 * Copyright (c) 2024 HPMicro
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "board.h"

extern void hpm_uart_driver_register(void);

extern void app_init(void);
extern void app_run(void);

int main(void)
{
    board_init();

    hpm_uart_driver_register();

    app_init();

    while (1) {
        app_run();
    }

    return 0;
}
