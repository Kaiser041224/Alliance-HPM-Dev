#include "app_debug_adc.h"

#include "app_adc.h"
#include "app_analog_signal.h"
#include "app_debug_rtt.h"
#include "intf_adc.h"
#include "intf_clock.h"

#include <stdbool.h>
#include <stdint.h>

static const char* adc_ch_names[ADC_CH_COUNT] = {
    [ADC_CH_V_IN] = "V_IN  ",  [ADC_CH_I_IN] = "I_IN  ",   [ADC_CH_I_L] = "I_L   ",
    [ADC_CH_V_OUT] = "V_OUT ", [ADC_CH_I_OUT] = "I_OUT ", [ADC_CH_I_AUX] = "I_AUX ",
};

static const char* analog_signal_names[APP_ANALOG_SIGNAL_ITEM_COUNT] = {
    [APP_ANALOG_SIGNAL_ITEM_V_IN]  = "V_IN  ",
    [APP_ANALOG_SIGNAL_ITEM_I_IN]  = "I_IN  ",
    [APP_ANALOG_SIGNAL_ITEM_I_L]   = "I_L   ",
    [APP_ANALOG_SIGNAL_ITEM_V_OUT] = "V_OUT ",
    [APP_ANALOG_SIGNAL_ITEM_I_OUT] = "I_OUT ",
    [APP_ANALOG_SIGNAL_ITEM_I_AUX] = "I_AUX ",
};

static const char* analog_signal_units[APP_ANALOG_SIGNAL_ITEM_COUNT] = {
    [APP_ANALOG_SIGNAL_ITEM_V_IN]   = "V",
    [APP_ANALOG_SIGNAL_ITEM_I_IN]   = "A",
    [APP_ANALOG_SIGNAL_ITEM_I_L]    = "A",
    [APP_ANALOG_SIGNAL_ITEM_V_OUT] = "V",
    [APP_ANALOG_SIGNAL_ITEM_I_OUT] = "A",
    [APP_ANALOG_SIGNAL_ITEM_I_AUX]   = "A",
};

static uint8_t app_debug_adc_inst(adc_channel_t ch) {
    return (ch == ADC_CH_I_IN || ch == ADC_CH_I_L || ch == ADC_CH_V_OUT) ? 1U : 0U;
}

static uint32_t app_debug_adc_rate_hz(uint32_t delta, uint32_t elapsed_cycles) {
    if (elapsed_cycles == 0U) {
        return 0U;
    }

    uint32_t cpu_hz = intf_clock_get_cpu_freq();
    if (cpu_hz == 0U) {
        return 0U;
    }

    return (uint32_t)(((uint64_t)delta * cpu_hz + elapsed_cycles / 2U) / elapsed_cycles);
}

static uint32_t app_debug_adc_elapsed_us(uint32_t elapsed_cycles) {
    uint32_t cpu_hz = intf_clock_get_cpu_freq();
    if (cpu_hz == 0U) {
        return 0U;
    }

    return (uint32_t)(((uint64_t)elapsed_cycles * 1000000ULL + cpu_hz / 2U) / cpu_hz);
}

static void app_debug_adc_print_ratio(const char *label, uint32_t numerator, uint32_t denominator) {
    if (denominator == 0U) {
        app_debug_printf("%s=%s", label, (numerator > 0U) ? "INF" : "NA");
        return;
    }

    uint32_t permille = (uint32_t)(((uint64_t)numerator * 1000ULL + denominator / 2U) / denominator);
    app_debug_printf("%s=%lu.%03lu", label,
                     (unsigned long)(permille / 1000U),
                     (unsigned long)(permille % 1000U));
}

void app_debug_adc_dump_channels(void) {
    app_debug_printf("[ADC]  Channel  Inst  Raw(hex)  Raw(dec)   ADC(mV)  Sense(mV)  Physical\r\n");

    for (adc_channel_t ch = ADC_CH_V_IN; ch < ADC_CH_COUNT; ch++) {
        uint16_t raw = app_adc_read_raw(ch);
        float adc_mv = 0.0f;
        float sense_mv = 0.0f;
        float physical = 0.0f;

        (void)app_adc_read_adc_voltage_mv(ch, &adc_mv);
        (void)app_adc_read_sense_voltage_mv(ch, &sense_mv);
        (void)app_adc_read_physical(ch, &physical);

        app_debug_printf(
            "[ADC]  %s   ADC%d  0x%04X   %5u   %8.1f   %9.1f   %8.3f\r\n", adc_ch_names[ch],
            app_debug_adc_inst(ch), raw, raw, adc_mv, sense_mv, physical);
    }
}

void app_debug_adc_dump_pmt(void) {
    uint16_t val[ADC_CH_COUNT];
    bool valid = false;

    for (adc_channel_t ch = ADC_CH_V_IN; ch < ADC_CH_COUNT; ch++) {
        if (app_adc_get_pmt_raw(ch, &val[ch]) != 0) {
            val[ch] = 0;
        } else {
            valid = true;
        }
    }

    if (!valid)
        return;
    app_debug_printf("--------------------------------------------------\r\n");
    app_debug_printf("[ADC]  Channel  Inst  Raw(hex)  Raw(dec)     mV\r\n");

    for (adc_channel_t ch = ADC_CH_V_IN; ch < ADC_CH_COUNT; ch++) {
        float mv = (float)val[ch] * 3300.0f / 65535.0f;
        app_debug_printf(
            "[ADC]  %s   ADC%d  0x%04X   %5u   %7.1f\r\n", adc_ch_names[ch],
            app_debug_adc_inst(ch), val[ch], val[ch], mv);
    }
}

void app_debug_adc_dump_analog_signal(void) {
    const float *raw_vals[] = {
        &g_analog_signal_snapshot.raw.v_in_v,   &g_analog_signal_snapshot.raw.i_in_a,
        &g_analog_signal_snapshot.raw.i_l_a,    &g_analog_signal_snapshot.raw.v_out_v,
        &g_analog_signal_snapshot.raw.i_out_a, &g_analog_signal_snapshot.raw.i_aux_a,
    };
    const float *flt_vals[] = {
        &g_analog_signal_snapshot.filtered.v_in_v,   &g_analog_signal_snapshot.filtered.i_in_a,
        &g_analog_signal_snapshot.filtered.i_l_a,    &g_analog_signal_snapshot.filtered.v_out_v,
        &g_analog_signal_snapshot.filtered.i_out_a, &g_analog_signal_snapshot.filtered.i_aux_a,
    };

    app_debug_printf("--------------------------------------------------\r\n");
    app_debug_printf("[ADC]  Signal   Unit     Raw        Filtered\r\n");

    for (app_analog_signal_item_t item = APP_ANALOG_SIGNAL_ITEM_V_IN; item < APP_ANALOG_SIGNAL_ITEM_COUNT; item++) {
        app_debug_printf("[ADC]  %s   %s    %8.3f    %8.3f\r\n",
                         analog_signal_names[item], analog_signal_units[item],
                         *raw_vals[item], *flt_vals[item]);
    }
}

void app_debug_adc_dump_diag(void) {
    static bool initialized;
    static uint32_t last_cycle;
    static intf_adc_diag_snapshot_t last;

    intf_adc_diag_snapshot_t now;
    if (intf_adc_get_diag_snapshot(&now) != 0) {
        return;
    }

    uint32_t cycle = intf_clock_get_cycle();
    uint32_t elapsed_cycles = initialized ? (cycle - last_cycle) : 0U;
    uint32_t elapsed_us = app_debug_adc_elapsed_us(elapsed_cycles);

    app_debug_printf("--------------------------------------------------\r\n");
    app_debug_printf("[ADC_DIAG] elapsed=%lu us (delta rates use real dump interval)\r\n",
                     (unsigned long)elapsed_us);
    app_debug_printf("[ADC_DIAG] inst irq(+d) gen(+d) pmt(+d,hz) cb(+d,hz) bad(+d) drop(+d)\r\n");

    uint32_t pmt_delta[INTF_ADC_INSTANCE_COUNT] = {0U};
    uint32_t cb_delta[INTF_ADC_INSTANCE_COUNT] = {0U};

    for (uint8_t inst = 0; inst < INTF_ADC_INSTANCE_COUNT; inst++) {
        uint32_t irq_delta = initialized ? now.irq_entry[inst] - last.irq_entry[inst] : 0U;
        uint32_t generic_delta = initialized ? now.generic_entry[inst] - last.generic_entry[inst] : 0U;
        pmt_delta[inst] = initialized ? now.pmt_complete[inst] - last.pmt_complete[inst] : 0U;
        cb_delta[inst] = initialized ? now.pmt_callback[inst] - last.pmt_callback[inst] : 0U;
        uint32_t bad_delta = initialized ? now.pmt_invalid[inst] - last.pmt_invalid[inst] : 0U;
        uint32_t drop_delta = initialized ? now.pmt_startup_drop[inst] - last.pmt_startup_drop[inst] : 0U;

        app_debug_printf(
            "[ADC_DIAG] ADC%u irq=%lu(+%lu) gen=%lu(+%lu) pmt=%lu(+%lu,%luHz) cb=%lu(+%lu,%luHz) bad=%lu(+%lu) drop=%lu(+%lu)\r\n",
            inst,
            (unsigned long)now.irq_entry[inst], (unsigned long)irq_delta,
            (unsigned long)now.generic_entry[inst], (unsigned long)generic_delta,
            (unsigned long)now.pmt_complete[inst], (unsigned long)pmt_delta[inst],
            (unsigned long)app_debug_adc_rate_hz(pmt_delta[inst], elapsed_cycles),
            (unsigned long)now.pmt_callback[inst], (unsigned long)cb_delta[inst],
            (unsigned long)app_debug_adc_rate_hz(cb_delta[inst], elapsed_cycles),
            (unsigned long)now.pmt_invalid[inst], (unsigned long)bad_delta,
            (unsigned long)now.pmt_startup_drop[inst], (unsigned long)drop_delta);

        uint32_t cycle_delta = initialized ? now.pmt_invalid_cycle[inst] - last.pmt_invalid_cycle[inst] : 0U;
        uint32_t trig_delta = initialized ? now.pmt_invalid_trig[inst] - last.pmt_invalid_trig[inst] : 0U;
        uint32_t channel_delta = initialized ? now.pmt_invalid_channel[inst] - last.pmt_invalid_channel[inst] : 0U;
        app_debug_printf(
            "[ADC_DIAG] ADC%u invalid_detail cycle=%lu(+%lu) trig=%lu(+%lu) channel=%lu(+%lu)\r\n",
            inst,
            (unsigned long)now.pmt_invalid_cycle[inst], (unsigned long)cycle_delta,
            (unsigned long)now.pmt_invalid_trig[inst], (unsigned long)trig_delta,
            (unsigned long)now.pmt_invalid_channel[inst], (unsigned long)channel_delta);
    }

    uint32_t via_delta = initialized ? now.adc1_handled_in_adc0_irq - last.adc1_handled_in_adc0_irq : 0U;
    app_debug_printf(
        "[ADC_DIAG] adc1_in_adc0_irq=%lu(+%lu,%luHz) ratio ADC1/ADC0: ",
        (unsigned long)now.adc1_handled_in_adc0_irq,
        (unsigned long)via_delta,
        (unsigned long)app_debug_adc_rate_hz(via_delta, elapsed_cycles));
    app_debug_adc_print_ratio("pmt", pmt_delta[1], pmt_delta[0]);
    app_debug_printf(" ");
    app_debug_adc_print_ratio("cb", cb_delta[1], cb_delta[0]);
    app_debug_printf("\r\n");

    last = now;
    last_cycle = cycle;
    initialized = true;
}

void app_debug_adc_run_tests(void) {
    app_debug_printf("\r\n[ADC] === ADC Latest Channel Scan ===\r\n");

    uint16_t val[ADC_CH_COUNT];

    for (adc_channel_t ch = ADC_CH_V_IN; ch < ADC_CH_COUNT; ch++) {
        app_adc_read_raw(ch);
        app_adc_read_raw(ch);
        val[ch] = app_adc_read_raw(ch);
    }
    app_debug_printf("--------------------------------------------------\r\n");
    app_debug_printf("[ADC]  Channel  Inst  Raw(hex)  Raw(dec)   ADC(mV)\r\n");
    for (adc_channel_t ch = ADC_CH_V_IN; ch < ADC_CH_COUNT; ch++) {
        float mv = 0.0f;
        (void)app_adc_read_adc_voltage_mv(ch, &mv);
        app_debug_printf(
            "[ADC]  %s   ADC%d  0x%04X   %5u   %8.1f\r\n", adc_ch_names[ch],
            app_debug_adc_inst(ch), val[ch], val[ch], mv);
    }
}
