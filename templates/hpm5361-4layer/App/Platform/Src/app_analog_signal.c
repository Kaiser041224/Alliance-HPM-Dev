/*
 * Analog Signal Conditioning Module
 *
 * Centralizes ADC calibration defaults and physical measurement conversion.
 */

#include "app_analog_signal.h"

#include "algo_filter.h"
#include "hpm_common.h"

#include <stddef.h>

/* ============================================================================
 * Constants and Internal Types
 * ============================================================================ */

#define APP_ANALOG_SIGNAL_VOLTAGE_DIVIDER_GAIN ((100.0f + 3.3f) / 3.3f)
#define APP_ANALOG_SIGNAL_INA240A2_RSENSE_OHM  (0.001f)
#define APP_ANALOG_SIGNAL_INA240A2_GAIN        (50.0f)
#define APP_ANALOG_SIGNAL_CURRENT_GAIN_A_PER_V \
    (1.0f / (APP_ANALOG_SIGNAL_INA240A2_RSENSE_OHM * APP_ANALOG_SIGNAL_INA240A2_GAIN))
#define APP_ANALOG_SIGNAL_CT_BURDEN_OHM (5.1f)
#define APP_ANALOG_SIGNAL_CT_RATIO      (100.0f)
#define APP_ANALOG_SIGNAL_CT_GAIN_A_PER_V \
    (APP_ANALOG_SIGNAL_CT_RATIO / APP_ANALOG_SIGNAL_CT_BURDEN_OHM)
#define APP_ANALOG_SIGNAL_BIDIR_BIAS_MV (INTF_ADC_DEFAULT_VREF_MV * 0.5f)

#define APP_ANALOG_SIGNAL_FILTER_MA_MAX_WINDOW (8U)
#define APP_ANALOG_SIGNAL_FILTER_FIR_MAX_TAPS  (16U)

typedef enum {
    APP_ANALOG_SIGNAL_FILTER_NONE = 0,
    APP_ANALOG_SIGNAL_FILTER_MA,
    APP_ANALOG_SIGNAL_FILTER_LPF,
    APP_ANALOG_SIGNAL_FILTER_FIR,
    APP_ANALOG_SIGNAL_FILTER_BIQUAD,
} app_analog_signal_filter_type_t;

typedef struct {
    app_analog_signal_filter_type_t type;
    union {
        struct {
            uint16_t window_size;
        } ma;
        struct {
            float cutoff_hz;
            float sample_rate_hz;
        } lpf;
        struct {
            const float* coeffs;
            uint16_t num_taps;
        } fir;
        struct {
            algo_biquad_coeffs_t coeffs;
        } biquad;
    } cfg;
} app_analog_signal_filter_cfg_t;

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

ATTR_PLACE_AT_FAST_RAM_INIT static const adc_channel_t
    s_item_to_channel[APP_ANALOG_SIGNAL_ITEM_COUNT] = {
        [APP_ANALOG_SIGNAL_ITEM_V_IN] = ADC_CH_V_IN,
        [APP_ANALOG_SIGNAL_ITEM_I_IN] = ADC_CH_I_IN,
        [APP_ANALOG_SIGNAL_ITEM_I_L] = ADC_CH_I_L,
        [APP_ANALOG_SIGNAL_ITEM_V_OUT] = ADC_CH_V_OUT,
        [APP_ANALOG_SIGNAL_ITEM_I_OUT] = ADC_CH_I_OUT,
        [APP_ANALOG_SIGNAL_ITEM_I_AUX] = ADC_CH_I_AUX,
    };

ATTR_PLACE_AT_FAST_RAM_INIT const app_analog_signal_item_t s_channel_to_item[ADC_CH_COUNT] = {
    [ADC_CH_V_IN] = APP_ANALOG_SIGNAL_ITEM_V_IN,
    [ADC_CH_I_IN] = APP_ANALOG_SIGNAL_ITEM_I_IN,
    [ADC_CH_I_L] = APP_ANALOG_SIGNAL_ITEM_I_L,
    [ADC_CH_V_OUT] = APP_ANALOG_SIGNAL_ITEM_V_OUT,
    [ADC_CH_I_OUT] = APP_ANALOG_SIGNAL_ITEM_I_OUT,
    [ADC_CH_I_AUX] = APP_ANALOG_SIGNAL_ITEM_I_AUX,
};

static const app_adc_calibration_t s_default_calibration[ADC_CH_COUNT] = {
    [ADC_CH_V_IN] =
        {
                       .sense_gain = 1.0f,
                       .sense_offset_mv = 0.0f,
                       .physical_gain = 0.001f * APP_ANALOG_SIGNAL_VOLTAGE_DIVIDER_GAIN,
                       .physical_offset = 0.0f,
                       },
    [ADC_CH_I_IN] =
        {
                       .sense_gain = 1.0f,
                       .sense_offset_mv = 0.0f,
                       .physical_gain = 0.001f * APP_ANALOG_SIGNAL_CURRENT_GAIN_A_PER_V,
                       .physical_offset =
                       -0.001f * APP_ANALOG_SIGNAL_BIDIR_BIAS_MV * APP_ANALOG_SIGNAL_CURRENT_GAIN_A_PER_V,
                       },
    [ADC_CH_I_L] =
        {
                       .sense_gain = 1.0f,
                       .sense_offset_mv = 0.0f,
                       .physical_gain = 0.001f * APP_ANALOG_SIGNAL_CURRENT_GAIN_A_PER_V,
                       .physical_offset =
                       -0.001f * APP_ANALOG_SIGNAL_BIDIR_BIAS_MV * APP_ANALOG_SIGNAL_CURRENT_GAIN_A_PER_V,
                       },
    [ADC_CH_V_OUT] =
        {
                       .sense_gain = 1.0f,
                       .sense_offset_mv = 0.0f,
                       .physical_gain = 0.001f * APP_ANALOG_SIGNAL_VOLTAGE_DIVIDER_GAIN,
                       .physical_offset = 0.0f,
                       },
    [ADC_CH_I_OUT] =
        {
                       .sense_gain = 1.0f,
                       .sense_offset_mv = 0.0f,
                       .physical_gain = 0.001f * APP_ANALOG_SIGNAL_CT_GAIN_A_PER_V,
                       .physical_offset =
                       -0.001f * APP_ANALOG_SIGNAL_BIDIR_BIAS_MV * APP_ANALOG_SIGNAL_CT_GAIN_A_PER_V,
                       },
    [ADC_CH_I_AUX] =
        {
                       .sense_gain = 1.0f,
                       .sense_offset_mv = 0.0f,
                       .physical_gain = 0.001f * APP_ANALOG_SIGNAL_CT_GAIN_A_PER_V,
                       .physical_offset =
                       -0.001f * APP_ANALOG_SIGNAL_BIDIR_BIAS_MV * APP_ANALOG_SIGNAL_CT_GAIN_A_PER_V,
                       },
};

static const app_analog_signal_filter_cfg_t s_default_filter_cfg[ADC_CH_COUNT] = {
    [ADC_CH_V_IN] =
        {
                       .type = APP_ANALOG_SIGNAL_FILTER_MA,
                       .cfg.ma = {.window_size = 4U},
                       },
    [ADC_CH_I_IN] =
        {
                       .type = APP_ANALOG_SIGNAL_FILTER_LPF,
                       .cfg.lpf = {.cutoff_hz = 20.0f, .sample_rate_hz = 25000.0f},
                       },
    [ADC_CH_I_L] =
        {
                       .type = APP_ANALOG_SIGNAL_FILTER_LPF,
                       .cfg.lpf = {.cutoff_hz = 20000.0f, .sample_rate_hz = 200000.0f},
                       },
    [ADC_CH_V_OUT] =
        {
                       .type = APP_ANALOG_SIGNAL_FILTER_LPF,
                       .cfg.lpf = {.cutoff_hz = 40000.0f, .sample_rate_hz = 200000.0f},
                       },
    [ADC_CH_I_OUT] =
        {
                       .type = APP_ANALOG_SIGNAL_FILTER_LPF,
                       .cfg.lpf = {.cutoff_hz = 10000.0f, .sample_rate_hz = 148000.0f},
                       },
    [ADC_CH_I_AUX] =
        {
                       .type = APP_ANALOG_SIGNAL_FILTER_LPF,
                       .cfg.lpf = {.cutoff_hz = 20000.0f, .sample_rate_hz = 148000.0f},
                       },
};

/* ============================================================================
 * Runtime State
 * ============================================================================ */

ATTR_PLACE_AT_FAST_RAM_BSS algo_ma_t s_ma_filters[APP_ANALOG_SIGNAL_ITEM_COUNT];
ATTR_PLACE_AT_FAST_RAM_BSS algo_lpf_t s_lpf_filters[APP_ANALOG_SIGNAL_ITEM_COUNT];
static algo_fir_t s_fir_filters[APP_ANALOG_SIGNAL_ITEM_COUNT];
static algo_biquad_t s_biquad_filters[APP_ANALOG_SIGNAL_ITEM_COUNT];

ATTR_PLACE_AT_FAST_RAM_BSS static float s_ma_filter_buffers[APP_ANALOG_SIGNAL_ITEM_COUNT]
                                                           [APP_ANALOG_SIGNAL_FILTER_MA_MAX_WINDOW];
static float s_fir_filter_buffers[APP_ANALOG_SIGNAL_ITEM_COUNT]
                                 [APP_ANALOG_SIGNAL_FILTER_FIR_MAX_TAPS];

static uint16_t s_raw_cache[ADC_CH_COUNT];
static bool s_raw_cache_valid[ADC_CH_COUNT];
static float s_physical_cache[ADC_CH_COUNT];
static float s_filtered_cache[ADC_CH_COUNT];
static bool s_filtered_cache_valid[ADC_CH_COUNT];

/* 预计算合并校准系数: physical = raw * cal_gain + cal_offset
 * 放入 DLM (fast_ram.bss)，ISR 中每个周期读取，消除 flash 访问延迟 */
ATTR_PLACE_AT_FAST_RAM_BSS static float s_cal_gain[ADC_CH_COUNT];
ATTR_PLACE_AT_FAST_RAM_BSS static float s_cal_offset[ADC_CH_COUNT];

static bool s_initialized;

app_analog_signal_snapshot_t g_analog_signal_snapshot;

/* ============================================================================
 * Private Helpers
 * ============================================================================ */

/**
 * @brief 判断模拟量枚举是否有效。
 *
 * @param item 目标模拟量项。
 * @return true 有效。
 * @return false 无效。
 */
static bool app_analog_signal_item_is_valid(app_analog_signal_item_t item) {
    return item < APP_ANALOG_SIGNAL_ITEM_COUNT;
}

static void
    app_analog_signal_update_fast_calibration(adc_channel_t ch, const app_adc_calibration_t* cal) {
    float vref_over_range = INTF_ADC_DEFAULT_VREF_MV / 65535.0f;

    s_cal_gain[ch] = vref_over_range * cal->sense_gain * cal->physical_gain;
    s_cal_offset[ch] = cal->sense_offset_mv * cal->physical_gain + cal->physical_offset;
}

/**
 * @brief 获取指定 ADC 通道对应的默认滤波配置。
 *
 * @param ch ADC 通道。
 * @return 对应滤波配置指针。
 */
static const app_analog_signal_filter_cfg_t* app_analog_signal_filter_cfg(adc_channel_t ch) {
    return &s_default_filter_cfg[ch];
}

/**
 * @brief 将缓存中的原始 ADC 结果转换为真实物理量。
 *
 * @param ch    目标 ADC 逻辑通道。
 * @param value 输出真实物理量。
 * @return 0 成功，-1 失败。
 */
static int app_analog_signal_read_item_from_cache(adc_channel_t ch, float* value) {
    if (ch >= ADC_CH_COUNT || value == NULL || !s_raw_cache_valid[ch]) {
        return -1;
    }

    *value = s_physical_cache[ch];
    return 0;
}

/**
 * @brief 初始化单路滑动平均滤波器。
 *
 * @param item 目标模拟量项。
 * @param cfg  滤波配置。
 * @return 0 成功，-1 参数非法。
 */
static int app_analog_signal_filter_init_ma(
    app_analog_signal_item_t item, const app_analog_signal_filter_cfg_t* cfg) {
    algo_ma_cfg_t ma_cfg = {
        .window_size = cfg->cfg.ma.window_size,
        .buffer = s_ma_filter_buffers[item],
    };

    algo_ma_ctor(&s_ma_filters[item]);
    if (ma_cfg.window_size == 0U || ma_cfg.window_size > APP_ANALOG_SIGNAL_FILTER_MA_MAX_WINDOW) {
        return -1;
    }

    return s_ma_filters[item].init(&s_ma_filters[item], &ma_cfg);
}

/**
 * @brief 初始化单路一阶低通滤波器。
 *
 * @param item 目标模拟量项。
 * @param cfg  滤波配置。
 * @return 0 成功，其他为算法库错误码。
 */
static int app_analog_signal_filter_init_lpf(
    app_analog_signal_item_t item, const app_analog_signal_filter_cfg_t* cfg) {
    algo_lpf_cfg_t lpf_cfg = {
        .cutoff_hz = cfg->cfg.lpf.cutoff_hz,
        .sample_rate_hz = cfg->cfg.lpf.sample_rate_hz,
    };

    algo_lpf_ctor(&s_lpf_filters[item]);
    return s_lpf_filters[item].init(&s_lpf_filters[item], &lpf_cfg);
}

/**
 * @brief 初始化单路 FIR 滤波器。
 *
 * @param item 目标模拟量项。
 * @param cfg  滤波配置。
 * @return 0 成功，-1 参数非法。
 */
static int app_analog_signal_filter_init_fir(
    app_analog_signal_item_t item, const app_analog_signal_filter_cfg_t* cfg) {
    algo_fir_cfg_t fir_cfg = {
        .coeffs = cfg->cfg.fir.coeffs,
        .num_taps = cfg->cfg.fir.num_taps,
        .buffer = s_fir_filter_buffers[item],
    };

    algo_fir_ctor(&s_fir_filters[item]);
    if (fir_cfg.coeffs == NULL || fir_cfg.num_taps == 0U
        || fir_cfg.num_taps > APP_ANALOG_SIGNAL_FILTER_FIR_MAX_TAPS) {
        return -1;
    }

    return s_fir_filters[item].init(&s_fir_filters[item], &fir_cfg);
}

/**
 * @brief 初始化单路 Biquad 滤波器。
 *
 * @param item 目标模拟量项。
 * @param cfg  滤波配置。
 * @return 0 成功，其他为算法库错误码。
 */
static int app_analog_signal_filter_init_biquad(
    app_analog_signal_item_t item, const app_analog_signal_filter_cfg_t* cfg) {
    algo_biquad_cfg_t biquad_cfg = {
        .coeffs = cfg->cfg.biquad.coeffs,
    };

    algo_biquad_ctor(&s_biquad_filters[item]);
    return s_biquad_filters[item].init(&s_biquad_filters[item], &biquad_cfg);
}

/**
 * @brief 按配置初始化单路滤波器。
 *
 * @param item 目标模拟量项。
 */
static void app_analog_signal_filter_init_item(app_analog_signal_item_t item) {
    const app_analog_signal_filter_cfg_t* cfg =
        app_analog_signal_filter_cfg(s_item_to_channel[item]);

    switch (cfg->type) {
    case APP_ANALOG_SIGNAL_FILTER_NONE: break;
    case APP_ANALOG_SIGNAL_FILTER_MA: (void)app_analog_signal_filter_init_ma(item, cfg); break;
    case APP_ANALOG_SIGNAL_FILTER_LPF: (void)app_analog_signal_filter_init_lpf(item, cfg); break;
    case APP_ANALOG_SIGNAL_FILTER_FIR: (void)app_analog_signal_filter_init_fir(item, cfg); break;
    case APP_ANALOG_SIGNAL_FILTER_BIQUAD:
        (void)app_analog_signal_filter_init_biquad(item, cfg);
        break;
    default: break;
    }
    /* V_OUT/I_IN 需要二级 MA：主配置为 LPF，此处显式初始化 MA。
     * V_OUT: 电压环反馈平滑。
     * I_IN: 功率环输入电流重滤波，抑制输入母线脉动单点采样。 */
    if (item == APP_ANALOG_SIGNAL_ITEM_V_OUT || item == APP_ANALOG_SIGNAL_ITEM_I_IN) {
        app_analog_signal_filter_cfg_t ma_cfg = {
            .type = APP_ANALOG_SIGNAL_FILTER_MA,
            .cfg.ma = {.window_size = (item == APP_ANALOG_SIGNAL_ITEM_I_IN) ? 8U : 4U},
        };
        (void)app_analog_signal_filter_init_ma(item, &ma_cfg);
    }
}

/**
 * @brief 对单路物理量执行一步滤波。
 *
 * @param item  目标模拟量项。
 * @param value 原始物理量。
 * @return 滤波后的物理量。
 */
static float app_analog_signal_filter_step(app_analog_signal_item_t item, float value) {
    const app_analog_signal_filter_cfg_t* cfg =
        app_analog_signal_filter_cfg(s_item_to_channel[item]);

    switch (cfg->type) {
    case APP_ANALOG_SIGNAL_FILTER_NONE: return value;
    case APP_ANALOG_SIGNAL_FILTER_MA: return s_ma_filters[item].step(&s_ma_filters[item], value);
    case APP_ANALOG_SIGNAL_FILTER_LPF: return s_lpf_filters[item].step(&s_lpf_filters[item], value);
    case APP_ANALOG_SIGNAL_FILTER_FIR: return s_fir_filters[item].step(&s_fir_filters[item], value);
    case APP_ANALOG_SIGNAL_FILTER_BIQUAD:
        return s_biquad_filters[item].step(&s_biquad_filters[item], value);
    default: return value;
    }
}

/**
 * @brief 读取单路未经滤波的真实物理量。
 *
 * @param item  目标模拟量项。
 * @param value 输出物理量。
 * @return 0 成功，-1 失败。
 */
static int app_analog_signal_read_item_raw(app_analog_signal_item_t item, float* value) {
    adc_channel_t ch;

    if (!app_analog_signal_item_is_valid(item) || value == NULL) {
        return -1;
    }

    if (!s_initialized) {
        app_analog_signal_init();
    }

    ch = s_item_to_channel[item];

    if (app_analog_signal_read_item_from_cache(ch, value) == 0) {
        return 0;
    }

    return app_adc_read_physical(ch, value);
}

/**
 * @brief 内部全量读取实现。
 *
 * @param measurements 输出测量结构体。
 * @param mode         读取模式：原始值或滤波值。
 * @return 0 成功，-1 任一路失败。
 */
static int app_analog_signal_read_measurements_internal(
    app_analog_signal_measurements_t* measurements, app_analog_signal_value_mode_t mode) {
    if (measurements == NULL) {
        return -1;
    }

    *measurements = (app_analog_signal_measurements_t){0};

    if (app_analog_signal_read_item(APP_ANALOG_SIGNAL_ITEM_V_IN, mode, &measurements->v_in_v)
        != 0) {
        return -1;
    }
    if (app_analog_signal_read_item(APP_ANALOG_SIGNAL_ITEM_I_IN, mode, &measurements->i_in_a)
        != 0) {
        return -1;
    }
    if (app_analog_signal_read_item(APP_ANALOG_SIGNAL_ITEM_I_L, mode, &measurements->i_l_a) != 0) {
        return -1;
    }
    if (app_analog_signal_read_item(APP_ANALOG_SIGNAL_ITEM_V_OUT, mode, &measurements->v_out_v)
        != 0) {
        return -1;
    }
    if (app_analog_signal_read_item(APP_ANALOG_SIGNAL_ITEM_I_OUT, mode, &measurements->i_out_a)
        != 0) {
        return -1;
    }
    if (app_analog_signal_read_item(APP_ANALOG_SIGNAL_ITEM_I_AUX, mode, &measurements->i_aux_a)
        != 0) {
        return -1;
    }

    return 0;
}

/* ============================================================================
 * Public Interfaces
 * ============================================================================ */

/**
 * @brief 装载默认 ADC 标定参数。
 */
void app_analog_signal_load_default_calibration(void) {
    for (adc_channel_t ch = ADC_CH_V_IN; ch < ADC_CH_COUNT; ch++) {
        app_adc_set_calibration(ch, &s_default_calibration[ch]);
    }
}

static float*
    snapshot_field_ptr(app_analog_signal_measurements_t* m, app_analog_signal_item_t item) {
    switch (item) {
    case APP_ANALOG_SIGNAL_ITEM_V_IN: return &m->v_in_v;
    case APP_ANALOG_SIGNAL_ITEM_I_IN: return &m->i_in_a;
    case APP_ANALOG_SIGNAL_ITEM_I_L: return &m->i_l_a;
    case APP_ANALOG_SIGNAL_ITEM_V_OUT: return &m->v_out_v;
    case APP_ANALOG_SIGNAL_ITEM_I_OUT: return &m->i_out_a;
    case APP_ANALOG_SIGNAL_ITEM_I_AUX: return &m->i_aux_a;
    default: return NULL;
    }
}

void app_analog_signal_update_raw(adc_channel_t ch, uint16_t raw) {
    if (ch >= ADC_CH_COUNT) {
        return;
    }

    s_raw_cache[ch] = raw;
    s_raw_cache_valid[ch] = true;

    float physical = (float)raw * s_cal_gain[ch] + s_cal_offset[ch];
    s_physical_cache[ch] = physical;

    app_analog_signal_item_t item = s_channel_to_item[ch];
    float* raw_field = snapshot_field_ptr(&g_analog_signal_snapshot.raw, item);
    if (raw_field) {
        *raw_field = physical;
    }

    if (!s_initialized) {
        return;
    }

    s_filtered_cache[ch] = app_analog_signal_filter_step(item, physical);
    s_filtered_cache_valid[ch] = true;

    float* flt_field = snapshot_field_ptr(&g_analog_signal_snapshot.filtered, item);
    if (flt_field) {
        *flt_field = s_filtered_cache[ch];
    }
}

int app_analog_signal_get_cached_raw(adc_channel_t ch, uint16_t* raw) {
    if (ch >= ADC_CH_COUNT || raw == NULL || !s_raw_cache_valid[ch]) {
        return -1;
    }

    *raw = s_raw_cache[ch];
    return 0;
}

void app_analog_signal_process(void) {
    for (adc_channel_t ch = ADC_CH_V_IN; ch < ADC_CH_COUNT; ch++) {
        uint16_t raw;
        if (app_adc_get_pmt_raw(ch, &raw) == 0) {
            app_analog_signal_update_raw(ch, raw);
        }
    }
}

void app_analog_signal_snapshot_refresh_raw(void) {
    for (adc_channel_t ch = ADC_CH_V_IN; ch < ADC_CH_COUNT; ch++) {
        uint16_t raw;
        if (app_adc_get_pmt_raw(ch, &raw) == 0) {
            float physical = (float)raw * s_cal_gain[ch] + s_cal_offset[ch];
            app_analog_signal_item_t item = s_channel_to_item[ch];
            *snapshot_field_ptr(&g_analog_signal_snapshot.raw, item) = physical;
        }
    }
}

/**
 * @brief 初始化模拟量调理模块。
 *
 * 完成默认标定装载，以及每路滤波器初始化。
 *
 * @note ADC 采样模式初始化由 `app_adc_init()` 负责。
 *       本模块只消费原始结果并完成物理量换算与滤波。
 */
void app_analog_signal_init(void) {
    if (s_initialized) {
        return;
    }

    app_analog_signal_load_default_calibration();

    for (adc_channel_t ch = ADC_CH_V_IN; ch < ADC_CH_COUNT; ch++) {
        app_analog_signal_update_fast_calibration(ch, &s_default_calibration[ch]);
    }

    for (app_analog_signal_item_t item = APP_ANALOG_SIGNAL_ITEM_V_IN;
         item < APP_ANALOG_SIGNAL_ITEM_COUNT; item++) {
        app_analog_signal_filter_init_item(item);
    }

    s_initialized = true;
}

ATTR_RAMFUNC
void app_analog_signal_convert_raw(adc_channel_t ch, uint16_t raw, float* physical) {
    if (ch >= ADC_CH_COUNT || physical == NULL) {
        return;
    }
    *physical = (float)raw * s_cal_gain[ch] + s_cal_offset[ch];
}

int app_analog_signal_get_physical(adc_channel_t ch, float* physical) {
    if (ch >= ADC_CH_COUNT || physical == NULL || !s_raw_cache_valid[ch]) {
        return -1;
    }
    *physical = s_physical_cache[ch];
    return 0;
}

/**
 * @brief 读取单个目标模拟量。
 *
 * @param item  目标信号项。
 * @param mode  读取模式：原始值或滤波值。
 * @param value 输出物理量。
 * @return 0 成功，-1 失败。
 */
int app_analog_signal_read_item(
    app_analog_signal_item_t item, app_analog_signal_value_mode_t mode, float* value) {
    if (!app_analog_signal_item_is_valid(item) || value == NULL) {
        return -1;
    }

    if (!s_initialized) {
        app_analog_signal_init();
    }

    if (mode == APP_ANALOG_SIGNAL_VALUE_FILTERED) {
        adc_channel_t ch = s_item_to_channel[item];

        if (!s_filtered_cache_valid[ch]) {
            return -1;
        }
        *value = s_filtered_cache[ch];
        return 0;
    }

    return app_analog_signal_read_item_raw(item, value);
}

/**
 * @brief 一次读取全部目标模拟量。
 *
 * @param measurements 输出测量结构体。
 * @param mode         读取模式：原始值或滤波值。
 * @return 0 成功，-1 失败。
 */
int app_analog_signal_get_measurements(
    app_analog_signal_measurements_t* measurements, app_analog_signal_value_mode_t mode) {
    return app_analog_signal_read_measurements_internal(measurements, mode);
}

/**
 * @brief 设置指定 ADC 通道的标定参数。
 *
 * @param ch          ADC 通道。
 * @param calibration 标定参数。
 * @return 0 成功，-1 失败。
 */
int app_analog_signal_set_channel_calibration(
    adc_channel_t ch, const app_adc_calibration_t* calibration) {
    if (ch >= ADC_CH_COUNT || calibration == NULL) {
        return -1;
    }

    app_adc_set_calibration(ch, calibration);
    app_analog_signal_update_fast_calibration(ch, calibration);
    return 0;
}

/**
 * @brief 获取指定 ADC 通道的当前标定参数。
 *
 * @param ch          ADC 通道。
 * @param calibration 输出标定参数。
 * @return 0 成功，-1 失败。
 */
int app_analog_signal_get_channel_calibration(
    adc_channel_t ch, app_adc_calibration_t* calibration) {
    return app_adc_get_calibration(ch, calibration);
}
