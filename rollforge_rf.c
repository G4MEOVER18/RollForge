// rollforge_rf.c — shared CC1101 init/deinit (ext first, int fallback)
#include "rollforge_rf.h"
#include <lib/subghz/devices/devices.h>

bool rollforge_rf_init(RollForgeApp* app) {
    subghz_devices_init();
    app->device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_EXT_NAME);
    if(!app->device) app->device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    if(!app->device) return false;
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
