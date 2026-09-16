/*
 * HRPWM Platform API
 *
 * Copyright (c) 2026 HPMicro
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef APP_HRPWM_H
#define APP_HRPWM_H

#include <stdint.h>

#define APP_HRPWM_DEFAULT_FREQ_HZ (200000U)

typedef enum {
    HRPWM_PAIR_A = 0, /* PWM0 pair 0 */
    HRPWM_PAIR_B,     /* PWM0 pair 1 */
    HRPWM_PAIR_C,     /* PWM1 pair 0 */
    HRPWM_PAIR_D,     /* PWM1 pair 1 */
    HRPWM_PAIR_COUNT,
} hrpwm_pair_t;

typedef enum {
    HRPWM_INST_0 = 0, /* PWM0 */
    HRPWM_INST_1,     /* PWM1 */
    HRPWM_INST_COUNT,
} hrpwm_inst_t;

void app_hrpwm_init(void);
void app_hrpwm_set_duty(hrpwm_pair_t pair, float duty);
void app_hrpwm_set_duty_direct(hrpwm_pair_t pair, float duty);
void app_hrpwm_set_duty_direct_dual(hrpwm_pair_t pair_a, float duty_a,
                                     hrpwm_pair_t pair_b, float duty_b);
void app_hrpwm_set_frequency(hrpwm_inst_t inst, uint32_t freq_hz);
void app_hrpwm_set_jitter(hrpwm_pair_t pair, uint8_t jitter_cmp);
void app_hrpwm_start(hrpwm_pair_t pair);
void app_hrpwm_stop(hrpwm_pair_t pair);
void app_hrpwm_stop_all(void);
void app_hrpwm_start_all(void);

/*
 * 仅启动指定实例的 PWM 计数器 (CEN)，不使能物理输出。
 * 用于软启动前预热 ADC PMT 触发链：CMP 事件依赖计数器运行，
 * 待占空比计算完成后再调用 app_hrpwm_start() 使能实际输出。
 */
void app_hrpwm_start_counter_only(hrpwm_inst_t inst);
void app_hrpwm_force_low(hrpwm_pair_t pair);
void app_hrpwm_force_release(hrpwm_pair_t pair);
void app_hrpwm_emergency_stop(void);
void app_hrpwm_resume(void);
void app_hrpwm_config_fault(void);
void app_hrpwm_clear_fault(void);

void app_hrpwm_set_phase(hrpwm_inst_t inst, uint8_t ref_pair, uint8_t target_pair, float phase_deg);
void app_hrpwm_config_phase_limit(hrpwm_inst_t inst,
                                  float max_phase_deg,
                                  float max_duty_ref,
                                  float max_duty_target);

#endif /* APP_HRPWM_H */
