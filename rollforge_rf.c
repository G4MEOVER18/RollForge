// rollforge_rf.c — shared CC1101 init/deinit (ext first, int fallback)
#include "rollforge_rf.h"
#include <lib/subghz/devices/devices.h>

bool rollforge_rf_init(RollForgeApp* app) {
    subghz_devices_init();

    // Ext CC1101: is_connect() prüft physische SPI-Verbindung vor begin()
    // Für int CC1101: .begin=NULL → subghz_devices_begin() gibt false zurück,
    // das ist kein Fehler sondern normal (return-Wert NICHT als Fehlerindikator nutzen!)
    const SubGhzDevice* dev = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_EXT_NAME);
    if(dev && subghz_devices_is_connect(dev)) {
        subghz_devices_begin(dev); // ext: begin wirklich ausführen, return ignorieren
        app->device     = dev;
        app->dev_is_ext = true;
    } else {
        dev = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
        if(!dev) {
            // rollforge_rf_deinit() ruft subghz_devices_deinit() auf — kein Doppelaufruf!
            return false;
        }
        subghz_devices_begin(dev); // int: begin=NULL → false, kein Fehler
        app->device     = dev;
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

void rollforge_rf_start_rx(RollForgeApp* app, FuriHalSubGhzCaptureCallback cb, void* ctx) {
    if(!app->device) return;
    subghz_devices_idle(app->device);
    subghz_devices_load_preset(app->device, FuriHalSubGhzPresetOok650Async, NULL);
    subghz_devices_set_frequency(app->device, app->frequency);
    subghz_devices_start_async_rx(app->device, cb, ctx);
}
