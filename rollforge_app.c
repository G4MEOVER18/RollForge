// rollforge_app.c — RollForge unified SubGHz app entry point
#include "rollforge_app.h"
#include "rollforge_rf.h"
#include "module_rolljam.h"
#include "module_rolllab.h"
#include "module_jammer.h"
#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>

#define MENU_COUNT 3

static const char* MENU_LABELS[MENU_COUNT] = {
    "RollJam  (Jam+Capture+Replay)",
    "RollLab  (Analyzer/Research)",
    "RF Jammer (CC1101)",
};

// ---------------------------------------------------------------------------
// GUI callbacks
// ---------------------------------------------------------------------------
static void rf_draw_menu(Canvas* canvas, RollForgeApp* app) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "RollForge v1.0");
    canvas_draw_line(canvas, 0, 12, 127, 12);
    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < MENU_COUNT; i++) {
        if(i == app->menu_idx) {
            canvas_draw_box(canvas, 0, 14 + i * 15, 127, 13);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 2, 24 + i * 15, MENU_LABELS[i]);
        if(i == app->menu_idx) canvas_set_color(canvas, ColorBlack);
    }
    canvas_set_color(canvas, ColorBlack);

    // RF indicator bottom-right
    const char* dev_label = app->device
        ? (app->dev_is_ext ? "CC1101-EXT" : "CC1101-INT")
        : "CC1101 ERR";
    canvas_draw_str_aligned(canvas, 127, 63, AlignRight, AlignBottom, dev_label);
}

static void rollforge_draw_cb(Canvas* canvas, void* ctx) {
    RollForgeApp* app = ctx;
    canvas_clear(canvas);
    switch(app->active) {
    case MOD_MENU:    rf_draw_menu(canvas, app);     break;
    case MOD_ROLLJAM: rj_draw(canvas, app);          break;
    case MOD_ROLLLAB: rl_draw(canvas, app);          break;
    case MOD_JAMMER:  jm_draw(canvas, app);          break;
    }
}

static void rollforge_input_cb(InputEvent* ev, void* ctx) {
    RollForgeApp* app = ctx;
    if(ev->type == InputTypeShort && ev->key == InputKeyBack) {
        app->abort = true;
    }
    furi_message_queue_put(app->eq, ev, 0);
}

// ---------------------------------------------------------------------------
// Module dispatch
// ---------------------------------------------------------------------------
static void mod_enter(RollForgeApp* app, ActiveMod m) {
    app->active = m;
    switch(m) {
    case MOD_ROLLJAM: rj_enter(app); break;
    case MOD_ROLLLAB: rl_enter(app); break;
    case MOD_JAMMER:  jm_enter(app); break;
    default: break;
    }
}

static void mod_leave(RollForgeApp* app) {
    switch(app->active) {
    case MOD_ROLLJAM: rj_exit(app); break;
    case MOD_ROLLLAB: rl_exit(app); break;
    case MOD_JAMMER:  jm_exit(app); break;
    default: break;
    }
    app->active    = MOD_MENU;
    app->abort     = false;
    // restore default frequency
    if(app->device) {
        subghz_devices_set_frequency(app->device, RF_FREQ_DEF);
        subghz_devices_idle(app->device);
    }
    app->rf_op = RF_IDLE;
}

static void mod_input(RollForgeApp* app, InputEvent* ev) {
    switch(app->active) {
    case MOD_ROLLJAM: rj_input(app, ev); break;
    case MOD_ROLLLAB: rl_input(app, ev); break;
    case MOD_JAMMER:  jm_input(app, ev); break;
    default: break;
    }
}

static void mod_tick(RollForgeApp* app) {
    switch(app->active) {
    case MOD_ROLLJAM: rj_tick(app); break;
    case MOD_ROLLLAB: rl_tick(app); break;
    case MOD_JAMMER:  jm_tick(app); break;
    default: break;
    }
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int32_t rollforge_app(void* p) {
    UNUSED(p);

    RollForgeApp* app = malloc(sizeof(RollForgeApp));
    furi_check(app);
    memset(app, 0, sizeof(RollForgeApp));

    app->active    = MOD_MENU;
    app->menu_idx  = 0;
    app->frequency = RF_FREQ_DEF;
    app->abort     = false;
    app->rf_op     = RF_IDLE;

    app->eq    = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->vp    = view_port_alloc();
    app->gui   = furi_record_open(RECORD_GUI);
    app->notif = furi_record_open(RECORD_NOTIFICATION);

    view_port_draw_callback_set(app->vp, rollforge_draw_cb, app);
    view_port_input_callback_set(app->vp, rollforge_input_cb, app);
    gui_add_view_port(app->gui, app->vp, GuiLayerFullscreen);

    rollforge_rf_init(app);  // ext → int fallback; app->device = NULL if missing

    bool running = true;
    while(running) {
        InputEvent ev;
        bool got_ev = (furi_message_queue_get(app->eq, &ev, 20) == FuriStatusOk);

        // Device guard: if CC1101 absent, block all RF states
        if(!app->device && app->active != MOD_MENU) {
            mod_leave(app);
            // show brief error (already set in mod tick)
        }

        if(app->active == MOD_MENU) {
            if(app->abort) { running = false; app->abort = false; }
            else if(got_ev && ev.type == InputTypeShort) {
                switch(ev.key) {
                case InputKeyUp:
                    if(app->menu_idx > 0) app->menu_idx--;
                    break;
                case InputKeyDown:
                    if(app->menu_idx < MENU_COUNT - 1) app->menu_idx++;
                    break;
                case InputKeyOk:
                    mod_enter(app, (ActiveMod)(app->menu_idx + 1));
                    break;
                case InputKeyBack:
                    running = false;
                    break;
                default: break;
                }
            }
        } else {
            if(app->abort) {
                mod_leave(app);
            } else {
                if(got_ev) {
                    if(ev.key == InputKeyBack && ev.type == InputTypeShort) {
                        mod_leave(app);
                    } else {
                        mod_input(app, &ev);
                    }
                }
                mod_tick(app);
            }
        }

        view_port_update(app->vp);
    }

    rollforge_rf_deinit(app);
    gui_remove_view_port(app->gui, app->vp);
    view_port_free(app->vp);
    furi_message_queue_free(app->eq);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(app);
    return 0;
}
