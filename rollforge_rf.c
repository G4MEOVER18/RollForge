// rollforge_rf.c — shared CC1101 init/deinit (ext first, int fallback)
#include "rollforge_rf.h"
#include <lib/subghz/devices/devices.h>

bool rollforge_rf_init(RollForgeApp* app) {
    subghz_devices_init();
    app->device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_EXT_NAME);
    app->dev_is_ext = (app->device != NULL);
    if(!app->device) app->device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    if(!app->device) {
        subghz_devices_deinit();
        return false;
    }
    subghz_devices_begin(app->device);
    subghz_devices_reset(app->device);
    subghz_devices_load_preset(app->device, FuriHalSubGhzPresetOok650Async, NULL);
    subghz_devices_set_frequency(app->device, app->frequency);
    subghz_devices_idle(app->device);
    app->rf_op = RF_IDLE;
    return true;
}

void rollforge_rf_deinit(RollForgeApp* app) {
    if(app->device) {
        subghz_devices_stop_async_tx(app->device);
        subghz_devices_stop_async_rx(app->device);
        subghz_devices_sleep(app->device);
        subghz_devices_end(app->device);
        app->device = NULL;
    }
    subghz_devices_deinit();
    app->rf_op = RF_IDLE;
}

void rollforge_rf_idle(RollForgeApp* app) {
    if(app->device) {
        subghz_devices_stop_async_tx(app->device);
        subghz_devices_stop_async_rx(app->device);
        subghz_devices_idle(app->device);
    }
    app->rf_op = RF_IDLE;
}

void rollforge_rf_start_tx(RollForgeApp* app, FuriHalSubGhzAsyncTxCallback cb, void* ctx) {
    if(!app->device) return;
    subghz_devices_idle(app->device);
    subghz_devices_load_preset(app->device, FuriHalSubGhzPresetOok650Async, NULL);
    subghz_devices_set_frequency(app->device, app->frequency);
    subghz_devices_start_async_tx(app->device, cb, ctx);
}

void rollforge_rf_start_rx(RollForgeApp* app, FuriHalSubGhzAsyncRxCallback cb, void* ctx) {
    if(!app->device) return;
    subghz_devices_idle(app->device);
    subghz_devices_load_preset(app->device, FuriHalSubGhzPresetOok650Async, NULL);
    subghz_devices_set_frequency(app->device, app->frequency);
    subghz_devices_start_async_rx(app->device, cb, ctx);
}
