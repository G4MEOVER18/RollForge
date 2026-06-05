// module_rolllab.c — RollLab research modes for RollForge
#include "module_rolllab.h"
#include <gui/canvas.h>
#include <lib/subghz/devices/devices.h>
#include <lib/toolbox/level_duration.h>
#include <notification/notification_messages.h>

// Interrupt globals
static volatile RollForgeApp* s_ref_app  = NULL;
static volatile RollForgeApp* s_work_app = NULL;
static volatile RollForgeApp* s_rl_rep   = NULL;

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------
static void rl_rx_ref_cb(bool level, uint32_t duration, void* ctx) {
    UNUSED(ctx);
    RollForgeApp* app = (RollForgeApp*)s_ref_app;
    if(!app) return;
    RfSig* sig = &app->rl.ref_sig;
    if(sig->ready || duration == 0) return;
    if(sig->count < RF_SIG_BUF) {
        uint32_t d = (duration > 0x7FFFFFFFUL) ? 0x7FFFFFFFUL : duration;
        sig->buf[sig->count++] = (level ? 0x80000000UL : 0UL) | d;
    }
    if((!level && duration >= RF_SILENCE_US && sig->count >= RF_MIN_EDGES) ||
       sig->count >= RF_SIG_BUF)
        sig->ready = true;
}

static void rl_rx_work_cb(bool level, uint32_t duration, void* ctx) {
    UNUSED(ctx);
    RollForgeApp* app = (RollForgeApp*)s_work_app;
    if(!app) return;
    ModRollLab* m = &app->rl;
    if(m->work_ready || duration == 0) return;
    if(m->work_count < RF_WORK_BUF) {
        uint32_t d = (duration > 0x7FFFFFFFUL) ? 0x7FFFFFFFUL : duration;
        m->work_buf[m->work_count++] = (level ? 0x80000000UL : 0UL) | d;
    }
    if((!level && duration >= RF_SILENCE_US && m->work_count >= RF_MIN_EDGES) ||
       m->work_count >= RF_WORK_BUF)
        m->work_ready = true;
}

static LevelDuration rl_replay_cb(void* ctx) {
    UNUSED(ctx);
    RollForgeApp* app = (RollForgeApp*)s_rl_rep;
    if(!app) return level_duration_reset();
    size_t pos = app->rl.tx_pos;
    if(pos >= app->rl.ref_sig.count) return level_duration_reset();
    app->rl.tx_pos = pos + 1;
    uint32_t p = app->rl.ref_sig.buf[pos];
    return level_duration_make((p & 0x80000000UL) != 0, (p & 0x7FFFFFFFUL) ?: 100);
}

// ---------------------------------------------------------------------------
// RF helpers
// ---------------------------------------------------------------------------
static void rl_ref_start(RollForgeApp* app) {
    RfSig* sig = &app->rl.ref_sig;
    sig->count = 0; sig->ready = false;
    s_ref_app = app;
    subghz_devices_idle(app->device);
    subghz_devices_start_async_rx(app->device, rl_rx_ref_cb, NULL);
    app->rf_op = RF_CAPTURING;
}

static void rl_ref_stop(RollForgeApp* app) {
    subghz_devices_stop_async_rx(app->device);
    s_ref_app = NULL;
    subghz_devices_idle(app->device);
    app->rf_op = RF_IDLE;
}

static void rl_work_start(RollForgeApp* app) {
    app->rl.work_count = 0; app->rl.work_ready = false;
    s_work_app = app;
    subghz_devices_idle(app->device);
    subghz_devices_start_async_rx(app->device, rl_rx_work_cb, NULL);
    app->rf_op = RF_CAPTURING;
}

static void rl_work_stop(RollForgeApp* app) {
    subghz_devices_stop_async_rx(app->device);
    s_work_app = NULL;
    subghz_devices_idle(app->device);
    app->rf_op = RF_IDLE;
}

static void rl_stop_any(RollForgeApp* app) {
    subghz_devices_stop_async_rx(app->device);
    subghz_devices_stop_async_tx(app->device);
    s_ref_app = NULL; s_work_app = NULL; s_rl_rep = NULL;
    subghz_devices_idle(app->device);
    app->rf_op = RF_IDLE;
}

// ---------------------------------------------------------------------------
// Signal analysis
// ---------------------------------------------------------------------------
static void rl_analyze(RollForgeApp* app) {
    RlAnalysis* a = &app->rl.analysis;
    const RfSig* sig = &app->rl.ref_sig;
    if(!sig->count) return;

    a->edges     = (uint32_t)sig->count;
    a->min_us    = 0xFFFFFFFFUL;
    a->max_us    = 0;
    uint64_t sum = 0;
    for(size_t i = 0; i < sig->count; i++) {
        uint32_t d = sig->buf[i] & 0x7FFFFFFFUL;
        if(d < a->min_us) a->min_us = d;
        if(d > a->max_us) a->max_us = d;
        sum += d;
    }
    a->avg_us   = (uint32_t)(sum / sig->count);
    a->total_ms = (uint32_t)(sum / 1000UL);

    // Preamble: first edge HIGH and >= 200µs
    a->preamble_ok = (sig->count > 0) && ((sig->buf[0] & 0x80000000UL) != 0) &&
                     ((sig->buf[0] & 0x7FFFFFFFUL) >= 200);

    // Bi-modal heuristic: if (max/min > 3) likely OOK/PWM
    a->looks_ook = (a->min_us > 0) && ((a->max_us / a->min_us) >= 2);
}

// Poll message queue for abort/back
static bool rl_poll_abort(RollForgeApp* app) {
    InputEvent ev;
    while(furi_message_queue_get(app->eq, &ev, 0) == FuriStatusOk)
        if(ev.key == InputKeyBack && ev.type == InputTypeShort) { app->abort = true; break; }
    return app->abort;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
static const char* rl_mode_names[] = { "Analyzer", "Replay", "Rollback", "SyncWin" };

void rl_enter(RollForgeApp* app) {
    app->rl.phase = RL_MENU;
    app->rl.menu_idx = 0;
    snprintf(app->rl.status, sizeof(app->rl.status), "Select mode");
}

void rl_exit(RollForgeApp* app) {
    rl_stop_any(app);
    app->rl.phase = RL_MENU;
}

void rl_input(RollForgeApp* app, InputEvent* ev) {
    if(ev->type != InputTypeShort) return;
    ModRollLab* m = &app->rl;

    switch(m->phase) {
    case RL_MENU:
        if(ev->key == InputKeyUp   && m->menu_idx > 0)                      m->menu_idx--;
        if(ev->key == InputKeyDown && m->menu_idx < 3)                      m->menu_idx++;
        if(ev->key == InputKeyOk) {
            m->mode  = (RlMode)m->menu_idx;
            m->phase = RL_CAPTURING;
            m->ref_sig.count = 0; m->ref_sig.ready = false;
            snprintf(m->status, sizeof(m->status), "Press keyfob...");
            rl_ref_start(app);
        }
        break;

    case RL_ANALYZE_VIEW:
        if(ev->key == InputKeyBack) { m->phase = RL_MENU; rl_stop_any(app); }
        break;

    case RL_REPLAY_READY:
        if(ev->key == InputKeyOk) {
            m->tx_pos = 0; s_rl_rep = app;
            subghz_devices_idle(app->device);
            subghz_devices_start_async_tx(app->device, rl_replay_cb, NULL);
            app->rf_op = RF_REPLAYING;
            m->phase = RL_REPLAYING;
            snprintf(m->status, sizeof(m->status), "Replaying...");
        }
        break;

    case RL_RESULT:
        if(ev->key == InputKeyBack) { m->phase = RL_MENU; rl_stop_any(app); }
        break;

    case RL_PROBE_READY:
        if(ev->key == InputKeyOk) {
            m->advance_count  = 0;
            m->advance_target = (m->mode == RL_ROLLBACK) ? 1 : 3;
            m->phase = RL_ADVANCE;
            rl_work_start(app);
            snprintf(m->status, sizeof(m->status), "Press keyfob 1/%u", m->advance_target);
        }
        break;

    default: break;
    }
}

void rl_tick(RollForgeApp* app) {
    ModRollLab* m = &app->rl;
    if(!app->device) {
        snprintf(m->status, sizeof(m->status), "ERR: No CC1101");
        return;
    }

    switch(m->phase) {
    case RL_MENU:
    case RL_ANALYZE_VIEW:
    case RL_REPLAY_READY:
    case RL_PROBE_READY:
    case RL_RESULT:
        break;

    case RL_CAPTURING: {
        rl_poll_abort(app);
        if(app->abort) {
            rl_ref_stop(app); m->phase = RL_MENU;
            app->abort = false;
            snprintf(m->status, sizeof(m->status), "Aborted");
            break;
        }
        // Timeout
        static uint32_t cap_start = 0;
        if(m->ref_sig.count == 0) cap_start = furi_get_tick();
        if(!m->ref_sig.ready && (furi_get_tick() - cap_start > RF_TIMEOUT_MS)) {
            rl_ref_stop(app);
            m->phase = RL_MENU;
            snprintf(m->status, sizeof(m->status), "Capture timeout");
            break;
        }
        if(m->ref_sig.ready && m->ref_sig.count >= RF_MIN_EDGES) {
            rl_ref_stop(app);
            if(m->mode == RL_ANALYZE) {
                rl_analyze(app);
                m->phase = RL_ANALYZE_VIEW;
            } else if(m->mode == RL_REPLAY) {
                snprintf(m->status, sizeof(m->status), "%u edges — [OK] Replay", (unsigned)m->ref_sig.count);
                m->phase = RL_REPLAY_READY;
            } else {
                snprintf(m->status, sizeof(m->status), "%u edges cap — [OK] Start probe", (unsigned)m->ref_sig.count);
                m->phase = RL_PROBE_READY;
            }
            notification_message(app->notif, &sequence_success);
        }
        break;
    }

    case RL_REPLAYING: {
        bool done = (m->tx_pos >= m->ref_sig.count) ||
                    subghz_devices_is_async_complete_tx(app->device);
        rl_poll_abort(app);
        if(done || app->abort) {
            s_rl_rep = NULL;
            subghz_devices_stop_async_tx(app->device);
            subghz_devices_idle(app->device);
            app->rf_op = RF_IDLE; app->abort = false;
            snprintf(m->status, sizeof(m->status), "Replay done. [Back]=Menu");
            m->phase = RL_REPLAY_READY;
            notification_message(app->notif, &sequence_success);
        }
        break;
    }

    case RL_ADVANCE: {
        rl_poll_abort(app);
        if(app->abort) {
            rl_work_stop(app); m->phase = RL_MENU;
            app->abort = false;
            snprintf(m->status, sizeof(m->status), "Aborted");
            break;
        }
        if(m->work_ready && m->work_count >= RF_MIN_EDGES) {
            rl_work_stop(app);
            m->advance_count++;
            notification_message(app->notif, &sequence_success);
            furi_delay_ms(200);
            if(m->advance_count >= m->advance_target) {
                m->phase = RL_PROBING;
                snprintf(m->status, sizeof(m->status), "Probing — replay cap...");
                // Replay original captured signal now
                m->tx_pos = 0; s_rl_rep = app;
                subghz_devices_idle(app->device);
                subghz_devices_start_async_tx(app->device, rl_replay_cb, NULL);
                app->rf_op = RF_REPLAYING;
            } else {
                rl_work_start(app);
                snprintf(m->status, sizeof(m->status), "Press keyfob %u/%u",
                    m->advance_count + 1, m->advance_target);
            }
        }
        break;
    }

    case RL_PROBING: {
        rl_poll_abort(app);
        bool done = (m->tx_pos >= m->ref_sig.count) ||
                    subghz_devices_is_async_complete_tx(app->device);
        if(done || app->abort) {
            s_rl_rep = NULL;
            subghz_devices_stop_async_tx(app->device);
            subghz_devices_idle(app->device);
            app->rf_op = RF_IDLE; app->abort = false;
            if(m->mode == RL_ROLLBACK)
                snprintf(m->status, sizeof(m->status), "Rollback probe done");
            else
                snprintf(m->status, sizeof(m->status), "SyncWin probe done");
            m->phase = RL_RESULT;
            notification_message(app->notif, &sequence_success);
        }
        break;
    }
    }
}

void rl_draw(Canvas* canvas, RollForgeApp* app) {
    ModRollLab* m = &app->rl;
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "RollForge > RollLab");
    canvas_draw_line(canvas, 0, 12, 127, 12);
    canvas_set_font(canvas, FontSecondary);

    if(m->phase == RL_MENU) {
        for(uint8_t i = 0; i < 4; i++) {
            if(i == m->menu_idx) {
                canvas_draw_box(canvas, 0, 14 + i * 12, 127, 11);
                canvas_set_color(canvas, ColorWhite);
            }
            canvas_draw_str(canvas, 2, 23 + i * 12, rl_mode_names[i]);
            if(i == m->menu_idx) canvas_set_color(canvas, ColorBlack);
        }
        return;
    }

    // Mode label
    if(m->mode < 4) canvas_draw_str(canvas, 0, 24, rl_mode_names[m->mode]);
    canvas_draw_str_aligned(canvas, 127, 24, AlignRight, AlignTop,
        app->rf_op == RF_CAPTURING ? "[RX]" :
        app->rf_op == RF_REPLAYING ? "[TX]" : "");

    if(m->phase == RL_ANALYZE_VIEW) {
        RlAnalysis* a = &m->analysis;
        char buf[32];
        snprintf(buf, sizeof(buf), "Edges: %lu  %s", (unsigned long)a->edges, a->looks_ook ? "OOK" : "FM?");
        canvas_draw_str(canvas, 0, 35, buf);
        snprintf(buf, sizeof(buf), "min:%lu avg:%lu us", (unsigned long)a->min_us, (unsigned long)a->avg_us);
        canvas_draw_str(canvas, 0, 46, buf);
        snprintf(buf, sizeof(buf), "len:%lums %s", (unsigned long)a->total_ms, a->preamble_ok ? "Preamble OK" : "");
        canvas_draw_str(canvas, 0, 57, buf);
    } else {
        canvas_draw_str(canvas, 0, 37, m->status);
        if(m->phase == RL_ADVANCE) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Count: %u/%u", m->advance_count, m->advance_target);
            canvas_draw_str(canvas, 0, 50, buf);
        }
        canvas_draw_str(canvas, 0, 63, "[Back]=Menu");
    }
}
