/*
 * ADC App Layer
 *
 * 示例映射（仅供参考，新工程请按实际硬件修改下方通道表）:
 *   ADC0: PB14/ch6 → V_IN    PB13/ch5 → I_AUX   PB12/ch4 → I_OUT
 *   ADC1: PB08/ch11 → I_IN   PB10/ch2 → I_L     PB11/ch3 → V_OUT
 *
 * Copyright (c) 2026 HPMicro
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef APP_ADC_H
#define APP_ADC_H

#include "intf_adc.h"
#include <stdint.h>

/* ============================================================================
 * Channel Definitions
 * ============================================================================ */

typedef enum {
    ADC_CH_V_IN = 0,  /* 输入电压 */
    ADC_CH_I_IN = 1,  /* 输入电流 */
    ADC_CH_I_L = 2,   /* 电感/主电流 */
    ADC_CH_V_OUT = 3, /* 输出电压 */
    ADC_CH_I_OUT = 4, /* 输出电流 */
    ADC_CH_I_AUX = 5, /* 辅助电流 */
    ADC_CH_COUNT,
} adc_channel_t;

typedef enum {
    APP_ADC_INST_0 = 0,
    APP_ADC_INST_1 = 1,
    APP_ADC_INST_COUNT,
} app_adc_inst_t;

/* ============================================================================
 * PMT Default Configuration
 * ============================================================================ */

#define APP_ADC_PMT_TRIGGER_CMP_INDEX_PWM0 (8U)
#define APP_ADC_PMT_TRIGGER_CMP_INDEX_PWM1 (10U)
#define APP_ADC_PMT_POSITION_RATIO_ADC0 (0.5f)
#define APP_ADC_PMT_POSITION_RATIO_ADC1 (0.5f)
#define APP_ADC_PMT_ADC0_TRIG_CH        (0U)
#define APP_ADC_PMT_ADC1_TRIG_CH        (3U)
#define APP_ADC_PMT_ADC0_CH_COUNT       (4U)
#define APP_ADC_PMT_ADC1_CH_COUNT       (4U)
#define APP_ADC_PMT_DMA_BUFF_LEN        (48U)

/* ============================================================================
 * Calibration
 * ============================================================================ */

typedef struct {
    float sense_gain;
    float sense_offset_mv;
    float physical_gain;
    float physical_offset;
} app_adc_calibration_t;

/* ============================================================================
 * API
 * ============================================================================ */

typedef void (*app_adc_pmt_callback_t)(void);

void app_adc_init(void);

int app_adc_register_pmt_callback(app_adc_inst_t inst, app_adc_pmt_callback_t cb);

uint16_t app_adc_read_raw(adc_channel_t ch);
void app_adc_read_all(uint16_t values[ADC_CH_COUNT]);
int app_adc_read_adc_voltage_mv(adc_channel_t ch, float* voltage_mv);
int app_adc_read_sense_voltage_mv(adc_channel_t ch, float* voltage_mv);
int app_adc_read_physical(adc_channel_t ch, float* value);

void app_adc_set_calibration(adc_channel_t ch, const app_adc_calibration_t* cal);
int app_adc_get_calibration(adc_channel_t ch, app_adc_calibration_t* cal);
void app_adc_set_vref_inst(app_adc_inst_t inst, float mv);
void app_adc_set_vref_all(float mv);
void app_adc_calibrate(void);

void app_adc_pmt_start_inst(app_adc_inst_t inst);
void app_adc_pmt_stop_inst(app_adc_inst_t inst);
int app_adc_get_pmt_raw(adc_channel_t ch, uint16_t* raw);
int app_adc_set_pmt_trigger_position(app_adc_inst_t inst, float position_ratio);

void app_adc_wdog_init(
    adc_channel_t ch, uint16_t thshd_high, uint16_t thshd_low, intf_adc_wdog_cb_t cb,
    void* user_data);
void app_adc_wdog_reenable(adc_channel_t ch);

#endif /* APP_ADC_H */
