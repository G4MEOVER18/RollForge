// rollforge_rf.c — shared CC1101 init/deinit (ext first, int fallback)
#include "rollforge_rf.h"
#include <lib/subghz/devices/devices.h>
#include <furi_hal_power.h>

// ---- OTG-Stromversorgung für externes CC1101-Modul -------------------------

static bool s_otg_by_app = false;

static void rf_power_on(void) {
    uint8_t attempts = 0;
    while(!furi_hal_power_is_otg_enabled() && attempts++ < 5) {
        furi_hal_power_enable_otg();
        furi_delay_ms(10);
    }
    if(furi_hal_power_is_otg_enabled()) s_otg_by_app = true;
}

static void rf_power_off(void) {
    if(s_otg_by_app && furi_hal_power_is_otg_enabled()) {
        furi_hal_power_disable_otg();
        s_otg_by_app = false;
    }
}

// ---- Hilfe: aktuelle Op sauber stoppen ------------------------------------
// furi_hal prüft: stop_async_tx braucht state==AsyncTx, stop_async_rx braucht
// state==AsyncRx. Nur den passenden Stop aufrufen!

static void rf_stop_current(RollForgeApp* app) {
    if(!app->device) return;
    if(app->rf_op == RF_JAMMING || app->rf_op == RF_REPLAYING)
        subghz_devices_stop_async_tx(app->device);
    else if(app->rf_op == RF_CAPTURING)
        subghz_devices_stop_async_rx(app->device);
    // RF_IDLE: nichts zu stoppen
}

// ---- Öffentliche API -------------------------------------------------------

bool rollforge_rf_init(RollForgeApp* app) {
    subghz_devices_init();

    // INT CC1101: begin=NULL → subghz_devices_begin() ist ein No-Op, kein Fehler
    app->device_int = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    if(!app->device_int) {
        subghz_devices_deinit();
        return false;
    }
    subghz_devices_begin(app->device_int);

    // EXT CC1101: OTG-Strom, dann is_connect() vor begin() prüfen
    rf_power_on();
    const SubGhzDevice* ext = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_EXT_NAME);
    if(ext && subghz_devices_is_connect(ext)) {
        subghz_devices_begin(ext);
        app->device_ext = ext;
        app->device     = ext;
        app->dev_is_ext = true;
    } else {
        rf_power_off();
        app->device_ext = NULL;
        app->device     = app->device_int;
        app->dev_is_ext = false;
    }

    subghz_devices_reset(app->device);
    subghz_devices_load_preset(app->device, FuriHalSubGhzPresetOok650Async, NULL);
    subghz_devices_set_frequency(app->device, app->frequency);
    subghz_devices_idle(app->device);
    app->rf_op = RF_IDLE;
    return true;
}

void rollforge_rf_deinit(RollForgeApp* app) {
    if(app->device) {
        rf_stop_current(app);          // state → Idle
        subghz_devices_sleep(app->device); // braucht state==Idle
    }
    if(app->device_ext) {
        subghz_devices_end(app->device_ext);
        app->device_ext = NULL;
    }
    if(app->device_int) {
        subghz_devices_end(app->device_int);
        app->device_int = NULL;
    }
    app->device = NULL;
    rf_power_off();
    subghz_devices_deinit();
    app->rf_op = RF_IDLE;
}

void rollforge_rf_idle(RollForgeApp* app) {
    rf_stop_current(app);
    app->rf_op = RF_IDLE;
}

void rollforge_rf_start_tx(RollForgeApp* app, FuriHalSubGhzAsyncTxCallback cb, void* ctx) {
    if(!app->device) return;
    rf_stop_current(app);   // state → Idle
    subghz_devices_load_preset(app->device, FuriHalSubGhzPresetOok650Async, NULL);
    subghz_devices_set_frequency(app->device, app->frequency);
    subghz_devices_start_async_tx(app->device, cb, ctx);
}

void rollforge_rf_start_rx(RollForgeApp* app, FuriHalSubGhzCaptureCallback cb, void* ctx) {
    if(!app->device) return;
    rf_stop_current(app);   // state → Idle
    subghz_devices_load_preset(app->device, FuriHalSubGhzPresetOok650Async, NULL);
    subghz_devices_set_frequency(app->device, app->frequency);
    subghz_devices_start_async_rx(app->device, cb, ctx);
}

void rollforge_rf_switch(RollForgeApp* app) {
    if(!app->device_ext) return;
    rollforge_rf_idle(app);   // laufende Op stoppen
    if(app->dev_is_ext) {
        app->device     = app->device_int;
        app->dev_is_ext = false;
    } else {
        app->device     = app->device_ext;
        app->dev_is_ext = true;
    }
    // Neues Gerät konfigurieren
    subghz_devices_reset(app->device);
    subghz_devices_load_preset(app->device, FuriHalSubGhzPresetOok650Async, NULL);
    subghz_devices_set_frequency(app->device, app->frequency);
}
