// module_jammer.c — CC1101 OOK interference jammer for RollForge
#include "module_jammer.h"
#include <gui/canvas.h>
#include <lib/subghz/devices/devices.h>
#include <lib/toolbox/level_duration.h>

static const uint32_t JM_FREQS[]       = { 315000000UL, 433920000UL, 868000000UL, 915000000UL };
static const char*    JM_FREQ_LABELS[] = { "315 MHz", "433.92 MHz", "868 MHz", "915 MHz" };

static volatile bool s_jm_active = false;

static LevelDuration jm_tx_cb(void* ctx) {
    UNUSED(ctx);
    if(!s_jm_active) return level_duration_reset();
    static bool lvl = false;
    lvl = !lvl;
    return level_duration_make(lvl, 250);
}

void jm_enter(RollForgeApp* app) {
    app->jm.phase    = JM_IDLE;
    app->jm.freq_idx = 1;  // default 433.92 MHz
    snprintf(app->jm.status, sizeof(app->jm.status), "[OK] Start  [U/D] Freq");
}

void jm_exit(RollForgeApp* app) {
    if(app->jm.phase == JM_JAMMING) {
        s_jm_active = false;
        subghz_devices_stop_async_tx(app->device);
        subghz_devices_idle(app->device);
    }
    app->jm.phase = JM_IDLE;
    app->rf_op = RF_IDLE;
}

void jm_input(RollForgeApp* app, InputEvent* ev) {
    if(ev->type != InputTypeShort) return;
    ModJammer* m = &app->jm;
    switch(ev->key) {
    case InputKeyUp:
        if(m->freq_idx > 0) m->freq_idx--;
        app->frequency = JM_FREQS[m->freq_idx];
        if(m->phase == JM_JAMMING) {
            // reconfigure frequency
            s_jm_active = false;
            subghz_devices_stop_async_tx(app->device);
            subghz_devices_idle(app->device);
            subghz_devices_set_frequency(app->device, app->frequency);
            s_jm_active = true;
            subghz_devices_start_async_tx(app->device, jm_tx_cb, NULL);
        }
        snprintf(m->status, sizeof(m->status), "%s", JM_FREQ_LABELS[m->freq_idx]);
        break;
    case InputKeyDown:
        if(m->freq_idx < JM_FREQ_COUNT - 1) m->freq_idx++;
        app->frequency = JM_FREQS[m->freq_idx];
        if(m->phase == JM_JAMMING) {
            s_jm_active = false;
            subghz_devices_stop_async_tx(app->device);
            subghz_devices_idle(app->device);
            subghz_devices_set_frequency(app->device, app->frequency);
            s_jm_active = true;
            subghz_devices_start_async_tx(app->device, jm_tx_cb, NULL);
        }
        snprintf(m->status, sizeof(m->status), "%s", JM_FREQ_LABELS[m->freq_idx]);
        break;
    case InputKeyOk:
        if(m->phase == JM_IDLE) {
            app->frequency = JM_FREQS[m->freq_idx];
            subghz_devices_set_frequency(app->device, app->frequency);
            s_jm_active = true;
            subghz_devices_idle(app->device);
            subghz_devices_start_async_tx(app->device, jm_tx_cb, NULL);
            app->rf_op = RF_JAMMING;
            m->phase   = JM_JAMMING;
            snprintf(m->status, sizeof(m->status), "JAMMING %s", JM_FREQ_LABELS[m->freq_idx]);
        } else {
            s_jm_active = false;
            subghz_devices_stop_async_tx(app->device);
            subghz_devices_idle(app->device);
            app->rf_op = RF_IDLE;
            m->phase   = JM_IDLE;
            snprintf(m->status, sizeof(m->status), "Stopped. [OK] Start");
        }
        break;
    default: break;
    }
}

void jm_tick(RollForgeApp* app) {
    if(!app->device) {
        snprintf(app->jm.status, sizeof(app->jm.status), "ERR: No CC1101");
    }
}

void jm_draw(Canvas* canvas, RollForgeApp* app) {
    ModJammer* m = &app->jm;
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "RollForge > Jammer");
    canvas_draw_line(canvas, 0, 12, 127, 12);
    canvas_set_font(canvas, FontSecondary);

    // Frequency list
    for(uint8_t i = 0; i < JM_FREQ_COUNT; i++) {
        if(i == m->freq_idx) {
            canvas_draw_box(canvas, 0, 13 + i * 10, 85, 9);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 2, 21 + i * 10, JM_FREQ_LABELS[i]);
        if(i == m->freq_idx) canvas_set_color(canvas, ColorBlack);
    }

    // State indicator right side
    if(m->phase == JM_JAMMING) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 90, 30, "JAM");
        canvas_draw_str(canvas, 90, 42, "ON");
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 63, m->status);
}
