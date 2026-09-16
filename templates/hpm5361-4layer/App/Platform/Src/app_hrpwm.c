/*
 * HRPWM Platform Implementation
 *
 * PWM0: ch0/ch1 (pair 0), ch2/ch3 (pair 1)
 * PWM1: ch4/ch5 (pair 2), ch6/ch7 (pair 3)
 *
 * Copyright (c) 2026 HPMicro
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "app_hrpwm.h"

#include "hpm_common.h"
#include "intf_hrpwm.h"

#include <stdbool.h>

ATTR_PLACE_AT_FAST_RAM_INIT static const intf_hrpwm_ch_t pair_to_ch[HRPWM_PAIR_COUNT] = {0, 2, 4, 6};

extern void hpm_hrpwm_driver_register(void);

ATTR_RAMFUNC
static bool hrpwm_pair_is_valid(hrpwm_pair_t pair) { return pair < HRPWM_PAIR_COUNT; }

static bool hrpwm_inst_is_valid(hrpwm_inst_t inst) { return inst < HRPWM_INST_COUNT; }

ATTR_RAMFUNC
static intf_hrpwm_ch_t hrpwm_pair_channel(hrpwm_pair_t pair) { return pair_to_ch[pair]; }

void app_hrpwm_init(void) {
    hpm_hrpwm_driver_register();

    intf_hrpwm_pair_cfg_t cfg[HRPWM_PAIR_COUNT] = {
        [HRPWM_PAIR_A] =
            {.frequency_hz = APP_HRPWM_DEFAULT_FREQ_HZ,
                           .duty = 0.0f,
                           .deadtime_ns = 25,
                           .jitter_cmp = 4,
                           .align = INTF_HRPWM_ALIGN_CENTER,
                           .invert_high_side = false,
                           .invert_low_side = false},
        /* HRPWM_PAIR_B: 输出反相（按实际物理连线调整） */
        [HRPWM_PAIR_B] =
            {.frequency_hz = APP_HRPWM_DEFAULT_FREQ_HZ,
                           .duty = 0.0f,
                           .deadtime_ns = 25,
                           .jitter_cmp = 4,
                           .align = INTF_HRPWM_ALIGN_CENTER,
                           .invert_high_side = true,
                           .invert_low_side = true },
        [HRPWM_PAIR_C] =
            {.frequency_hz = APP_HRPWM_DEFAULT_FREQ_HZ,
                           .duty = 0.0f,
                           .deadtime_ns = 15,
                           .jitter_cmp = 4,
                           .align = INTF_HRPWM_ALIGN_CENTER,
                           .invert_high_side = false,
                           .invert_low_side = false},
        [HRPWM_PAIR_D] =
            {.frequency_hz = APP_HRPWM_DEFAULT_FREQ_HZ,
                           .duty = 0.0f,
                           .deadtime_ns = 15,
                           .jitter_cmp = 4,
                           .align = INTF_HRPWM_ALIGN_CENTER,
                           .invert_high_side = false,
                           .invert_low_side = false},
    };
    for (hrpwm_pair_t pair = HRPWM_PAIR_A; pair < HRPWM_PAIR_COUNT; pair++) {
        (void)intf_hrpwm_init_pair(hrpwm_pair_channel(pair), &cfg[pair]);
        /* PWM configured but NOT started — ADC calibrates in quiet environment first */
    }
    app_hrpwm_set_phase(HRPWM_INST_0, HRPWM_PAIR_A, HRPWM_PAIR_B, 180.0f);
}

ATTR_RAMFUNC
void app_hrpwm_set_duty(hrpwm_pair_t pair, float duty) {
    if (!hrpwm_pair_is_valid(pair))
        return;

    (void)intf_hrpwm_set_duty(hrpwm_pair_channel(pair), duty);
}

ATTR_RAMFUNC
void app_hrpwm_set_duty_direct(hrpwm_pair_t pair, float duty) {
    if (!hrpwm_pair_is_valid(pair))
        return;

    (void)intf_hrpwm_set_duty_direct(hrpwm_pair_channel(pair), duty);
}

ATTR_RAMFUNC
void app_hrpwm_set_duty_direct_dual(
    hrpwm_pair_t pair_a, float duty_a, hrpwm_pair_t pair_b, float duty_b) {
    if (!hrpwm_pair_is_valid(pair_a) || !hrpwm_pair_is_valid(pair_b))
        return;

    (void)intf_hrpwm_set_duty_direct_dual(
        hrpwm_pair_channel(pair_a), duty_a, hrpwm_pair_channel(pair_b), duty_b);
}

void app_hrpwm_set_frequency(hrpwm_inst_t inst, uint32_t freq_hz) {
    if (!hrpwm_inst_is_valid(inst))
        return;

    (void)intf_hrpwm_set_frequency(inst, freq_hz);
}

void app_hrpwm_set_jitter(hrpwm_pair_t pair, uint8_t jitter_cmp) {
    if (!hrpwm_pair_is_valid(pair))
        return;

    (void)intf_hrpwm_set_jitter(hrpwm_pair_channel(pair), jitter_cmp);
}

void app_hrpwm_start(hrpwm_pair_t pair) {
    if (!hrpwm_pair_is_valid(pair))
        return;

    (void)intf_hrpwm_start(hrpwm_pair_channel(pair));
}

void app_hrpwm_stop(hrpwm_pair_t pair) {
    if (!hrpwm_pair_is_valid(pair))
        return;

    (void)intf_hrpwm_stop(hrpwm_pair_channel(pair));
    (void)intf_hrpwm_stop((intf_hrpwm_ch_t)(hrpwm_pair_channel(pair) + 1U));
}

void app_hrpwm_stop_all(void) {
    for (hrpwm_pair_t pair = HRPWM_PAIR_A; pair < HRPWM_PAIR_COUNT; pair++) {
        app_hrpwm_stop(pair);
    }
}

void app_hrpwm_start_all(void) {
    for (hrpwm_pair_t pair = HRPWM_PAIR_A; pair < HRPWM_PAIR_COUNT; pair++) {
        app_hrpwm_start(pair);
    }
}

void app_hrpwm_start_counter_only(hrpwm_inst_t inst) {
    if (!hrpwm_inst_is_valid(inst))
        return;

    (void)intf_hrpwm_start_counter_only((intf_hrpwm_inst_t)inst);
}

ATTR_RAMFUNC
void app_hrpwm_force_low(hrpwm_pair_t pair) {
    if (!hrpwm_pair_is_valid(pair))
        return;

    (void)intf_hrpwm_force_low(hrpwm_pair_channel(pair));
    (void)intf_hrpwm_force_low((intf_hrpwm_ch_t)(hrpwm_pair_channel(pair) + 1U));
}

ATTR_RAMFUNC
void app_hrpwm_force_release(hrpwm_pair_t pair) {
    if (!hrpwm_pair_is_valid(pair))
        return;

    (void)intf_hrpwm_force_release(hrpwm_pair_channel(pair));
    (void)intf_hrpwm_force_release((intf_hrpwm_ch_t)(hrpwm_pair_channel(pair) + 1U));
}

ATTR_RAMFUNC
void app_hrpwm_emergency_stop(void) {
    for (hrpwm_pair_t pair = HRPWM_PAIR_A; pair < HRPWM_PAIR_COUNT; pair++) {
        app_hrpwm_force_low(pair);
    }
}

void app_hrpwm_resume(void) {
    for (hrpwm_pair_t pair = HRPWM_PAIR_A; pair < HRPWM_PAIR_COUNT; pair++) {
        app_hrpwm_force_release(pair);
    }
}

void app_hrpwm_config_fault(void) {
    intf_hrpwm_fault_cfg_t fault_cfg = {
        .source = INTF_HRPWM_FAULT_SRC_EXTERNAL_0,
        .mode = INTF_HRPWM_FAULT_MODE_FORCE_LOW,
        .recovery = INTF_HRPWM_FAULT_RECOVERY_ON_FAULT_CLEAR,
        .active_low = true,
    };

    (void)intf_hrpwm_config_fault((intf_hrpwm_inst_t)HRPWM_INST_0, &fault_cfg);
    (void)intf_hrpwm_config_fault((intf_hrpwm_inst_t)HRPWM_INST_1, &fault_cfg);
}

void app_hrpwm_clear_fault(void) {
    (void)intf_hrpwm_clear_fault((intf_hrpwm_inst_t)HRPWM_INST_0);
    (void)intf_hrpwm_clear_fault((intf_hrpwm_inst_t)HRPWM_INST_1);
}

ATTR_RAMFUNC
void app_hrpwm_set_phase(
    hrpwm_inst_t inst, uint8_t ref_pair, uint8_t target_pair, float phase_deg) {
    intf_hrpwm_phase_cfg_t cfg = {
        .inst = inst,
        .ref_pair = ref_pair,
        .target_pair = target_pair,
        .phase_deg = phase_deg,
    };
    (void)intf_hrpwm_set_phase(&cfg);
}

void app_hrpwm_config_phase_limit(
    hrpwm_inst_t inst, float max_phase_deg, float max_duty_ref, float max_duty_target) {
    (void)inst;
    (void)max_phase_deg;
    (void)max_duty_ref;
    (void)max_duty_target;
}
