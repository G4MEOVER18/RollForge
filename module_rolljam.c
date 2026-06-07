// module_rolljam.c — RollJam non-blocking state machine for RollForge
#include "module_rolljam.h"
#include "rollforge_rf.h"
#include "rollforge_storage.h"
#include <gui/canvas.h>
#include <lib/subghz/devices/devices.h>
#include <lib/toolbox/level_duration.h>
#include <notification/notification_messages.h>

#define RJ_JAM_MS  180U
#define RJ_RX_MS    80U

static volatile bool           s_jam  = false;
static volatile RfSig*         s_cap  = NULL;
static volatile RollForgeApp*  s_rep  = NULL;

// ---------------------------------------------------------------------------
// Interrupt callbacks
// ---------------------------------------------------------------------------
static void rj_rx_cb(bool level, uint32_t duration, void* ctx) {
    UNUSED(ctx);
    RfSig* sig = (RfSig*)s_cap;
    if(!sig || sig->ready || duration == 0) return;
    if(sig->count < RF_SIG_BUF) {
        uint32_t d = (duration > 0x7FFFFFFFUL) ? 0x7FFFFFFFUL : duration;
        sig->buf[sig->count++] = (level ? 0x80000000UL : 0UL) | d;
    }
    if((!level && duration >= RF_SILENCE_US && sig->count >= RF_MIN_EDGES) ||
       sig->count >= RF_SIG_BUF)
        sig->ready = true;
}

static LevelDuration rj_jam_cb(void* ctx) {
    UNUSED(ctx);
    if(!s_jam) return level_duration_reset();
    static bool lvl = false;
    lvl = !lvl;
    return level_duration_make(lvl, 250);
}

static LevelDuration rj_replay_cb(void* ctx) {
    UNUSED(ctx);
    RollForgeApp* app = (RollForgeApp*)s_rep;
    if(!app) return level_duration_reset();
    size_t pos = app->rj.tx_pos;
    if(pos >= app->rj.sig_a.count) return level_duration_reset();
    app->rj.tx_pos = pos + 1;
    uint32_t p = app->rj.sig_a.buf[pos];
    return level_duration_make((p & 0x80000000UL) != 0, (p & 0x7FFFFFFFUL) ?: 100);
}

// ---------------------------------------------------------------------------
// RF helpers — all guard app->device
// ---------------------------------------------------------------------------
static void rj_jam_start(RollForgeApp* app) {
    if(!app->device) return;
    s_jam = true;
    rollforge_rf_start_tx(app, rj_jam_cb, NULL);
    app->rf_op = RF_JAMMING;
}

static void rj_jam_stop(RollForgeApp* app) {
    s_jam = false;
    if(app->device && (app->rf_op == RF_JAMMING || app->rf_op == RF_REPLAYING))
        subghz_devices_stop_async_tx(app->device);
    app->rf_op = RF_IDLE;
}

static void rj_cap_start(RollForgeApp* app, RfSig* sig) {
    if(!app->device) return;
    sig->count = 0; sig->ready = false;
    s_cap = sig;
    rollforge_rf_start_rx(app, rj_rx_cb, NULL);
    app->rf_op = RF_CAPTURING;
}

static void rj_cap_stop(RollForgeApp* app) {
    s_cap = NULL;
    if(app->device && app->rf_op == RF_CAPTURING)
        subghz_devices_stop_async_rx(app->device);
    app->rf_op = RF_IDLE;
}

static void rj_rf_clean(RollForgeApp* app) {
    s_jam = false; s_cap = NULL; s_rep = NULL;
    rollforge_rf_idle(app);
}

// ---------------------------------------------------------------------------
// Non-blocking interlaced jam/rx helper — starts the first jam window
// ---------------------------------------------------------------------------
static void rj_phase_start(RollForgeApp* app, RfSig* sig, RjPhase jam_state) {
    sig->count = 0; sig->ready = false;
    app->rj.phase_start_ms = furi_get_tick();
    app->rj.step_start_ms  = app->rj.phase_start_ms;
    app->rj.phase = jam_state;
    rj_jam_start(app);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void rj_enter(RollForgeApp* app) {
    app->rj.phase = RJ_IDLE;
    snprintf(app->rj.status, sizeof(app->rj.status), "[OK] Start  [Back] Menu");
}

void rj_exit(RollForgeApp* app) {
    rj_rf_clean(app);
    app->rj.phase = RJ_IDLE;
}

void rj_input(RollForgeApp* app, InputEvent* ev) {
    if(ev->type != InputTypeShort) return;
    ModRollJam* m = &app->rj;
    if(ev->key == InputKeyOk) {
        if(m->phase == RJ_IDLE) {
            snprintf(m->status, sizeof(m->status), "Phase 1: Jam+Capture A...");
            rj_phase_start(app, &m->sig_a, RJ_P1_JAM);
        } else if(m->phase == RJ_CAPTURED) {
            snprintf(m->status, sizeof(m->status), "Phase 2: Jam+Capture B...");
            rj_phase_start(app, &m->sig_b, RJ_P2_JAM);
        } else if(m->phase == RJ_DONE) {
            m->phase = RJ_IDLE;
            snprintf(m->status, sizeof(m->status), "[OK] Start  [Back] Menu");
        }
    } else if(ev->key == InputKeyDown) {
        // Save aktuelles Signal als .sub
        if(m->phase == RJ_CAPTURED && m->sig_a.count > 0) {
            bool ok = rollforge_save_sig((const RfSig*)&m->sig_a, app->frequency, "rj_A");
            snprintf(m->status, sizeof(m->status), ok ? "A gespeichert" : "Save Fehler");
        } else if(m->phase == RJ_DONE && m->sig_b.count > 0) {
            bool ok = rollforge_save_sig((const RfSig*)&m->sig_b, app->frequency, "rj_B");
            snprintf(m->status, sizeof(m->status), ok ? "B gespeichert" : "Save Fehler");
        }
    }
}

void rj_tick(RollForgeApp* app) {
    ModRollJam* m = &app->rj;
    if(!app->device) {
        snprintf(m->status, sizeof(m->status), "ERR: No CC1101");
        return;
    }

    uint32_t now = furi_get_tick();

    switch(m->phase) {
    case RJ_IDLE:
    case RJ_CAPTURED:
    case RJ_DONE:
        break;

    // ----- Phase 1 jam window -----
    case RJ_P1_JAM:
        if(now - m->phase_start_ms >= RF_TIMEOUT_MS) {
            rj_jam_stop(app);
            m->phase = RJ_IDLE;
            snprintf(m->status, sizeof(m->status), "Phase 1 Timeout");
        } else if(now - m->step_start_ms >= RJ_JAM_MS) {
            rj_jam_stop(app);
            rj_cap_start(app, &m->sig_a);
            m->step_start_ms = now;
            m->phase = RJ_P1_RX;
            snprintf(m->status, sizeof(m->status), "Ph1 RX...");
        }
        break;

    // ----- Phase 1 RX window -----
    case RJ_P1_RX:
        if(m->sig_a.ready && m->sig_a.count >= RF_MIN_EDGES) {
            rj_cap_stop(app);
            snprintf(m->status, sizeof(m->status), "A: %lu edges -- [OK] Ph2", (unsigned long)m->sig_a.count);
            m->phase = RJ_CAPTURED;
            notification_message(app->notif, &sequence_success);
        } else if(now - m->step_start_ms >= RJ_RX_MS) {
            rj_cap_stop(app);
            if(now - m->phase_start_ms >= RF_TIMEOUT_MS) {
                m->phase = RJ_IDLE;
                snprintf(m->status, sizeof(m->status), "Phase 1 Timeout");
            } else {
                rj_jam_start(app);
                m->step_start_ms = now;
                m->phase = RJ_P1_JAM;
                snprintf(m->status, sizeof(m->status), "Ph1 Jam...");
            }
        }
        break;

    // ----- Phase 2 jam window -----
    case RJ_P2_JAM:
        if(now - m->phase_start_ms >= RF_TIMEOUT_MS) {
            rj_jam_stop(app);
            m->phase = RJ_IDLE;
            snprintf(m->status, sizeof(m->status), "Phase 2 Timeout");
        } else if(now - m->step_start_ms >= RJ_JAM_MS) {
            rj_jam_stop(app);
            rj_cap_start(app, &m->sig_b);
            m->step_start_ms = now;
            m->phase = RJ_P2_RX;
            snprintf(m->status, sizeof(m->status), "Ph2 RX...");
        }
        break;

    // ----- Phase 2 RX window -----
    case RJ_P2_RX:
        if(m->sig_b.ready && m->sig_b.count >= RF_MIN_EDGES) {
            rj_cap_stop(app);
            // Immediately start replay
            m->tx_pos = 0; s_rep = app;
            rollforge_rf_start_tx(app, rj_replay_cb, NULL);
            app->rf_op = RF_REPLAYING;
            m->phase = RJ_REPLAY;
            snprintf(m->status, sizeof(m->status), "Replaying A... (B saved)");
        } else if(now - m->step_start_ms >= RJ_RX_MS) {
            rj_cap_stop(app);
            if(now - m->phase_start_ms >= RF_TIMEOUT_MS) {
                m->phase = RJ_IDLE;
                snprintf(m->status, sizeof(m->status), "Phase 2 Timeout");
            } else {
                rj_jam_start(app);
                m->step_start_ms = now;
                m->phase = RJ_P2_JAM;
                snprintf(m->status, sizeof(m->status), "Ph2 Jam...");
            }
        }
        break;

    // ----- Replay -----
    case RJ_REPLAY: {
        bool done = (m->tx_pos >= m->sig_a.count) ||
                    subghz_devices_is_async_complete_tx(app->device);
        if(done) {
            s_rep = NULL;
            subghz_devices_stop_async_tx(app->device);
            subghz_devices_idle(app->device);
            app->rf_op = RF_IDLE;
            snprintf(m->status, sizeof(m->status), "Done! B valid. [OK] Again");
            m->phase = RJ_DONE;
            notification_message(app->notif, &sequence_success);
        }
        break;
    }
    }
}

void rj_draw(Canvas* canvas, RollForgeApp* app) {
    ModRollJam* m = &app->rj;
    static const char* phase_labels[] = {
        "IDLE", "PH1:JAM", "PH1:RX", "CAPTURED", "PH2:JAM", "PH2:RX", "REPLAY", "DONE"
    };

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "RollForge > RollJam");
    canvas_draw_line(canvas, 0, 12, 127, 12);
    canvas_set_font(canvas, FontSecondary);

    uint8_t pi = (uint8_t)m->phase;
    if(pi < 8) canvas_draw_str(canvas, 0, 24, phase_labels[pi]);

    if(app->rf_op == RF_JAMMING)
        canvas_draw_str_aligned(canvas, 127, 24, AlignRight, AlignTop, "[JAM]");
    else if(app->rf_op == RF_CAPTURING)
        canvas_draw_str_aligned(canvas, 127, 24, AlignRight, AlignTop, "[RX]");
    else if(app->rf_op == RF_REPLAYING)
        canvas_draw_str_aligned(canvas, 127, 24, AlignRight, AlignTop, "[TX]");

    canvas_draw_str(canvas, 0, 37, m->status);

    if(m->phase == RJ_REPLAY) {
        char buf[32];
        snprintf(buf, sizeof(buf), "TX %lu/%lu",
            (unsigned long)m->tx_pos, (unsigned long)m->sig_a.count);
        canvas_draw_str(canvas, 0, 50, buf);
    } else if(m->phase == RJ_CAPTURED || m->phase == RJ_DONE) {
        char buf[48];
        snprintf(buf, sizeof(buf), "A:%lu  B:%lu",
            (unsigned long)m->sig_a.count, (unsigned long)m->sig_b.count);
        canvas_draw_str(canvas, 0, 50, buf);
    }

    if(m->phase == RJ_CAPTURED || m->phase == RJ_DONE)
        canvas_draw_str(canvas, 0, 63, "[Back]=Menu  [v]=Save");
    else
        canvas_draw_str(canvas, 0, 63, "[Back]=Menu");
}
