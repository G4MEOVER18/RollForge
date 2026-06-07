#pragma once
#include "rollforge_app.h"

bool rollforge_rf_init(RollForgeApp* app);
void rollforge_rf_deinit(RollForgeApp* app);
void rollforge_rf_idle(RollForgeApp* app);

// Sichere TX/RX-Starter: stoppen laufende Op, laden preset+freq, starten async
void rollforge_rf_start_tx(RollForgeApp* app, FuriHalSubGhzAsyncTxCallback cb, void* ctx);
void rollforge_rf_start_rx(RollForgeApp* app, FuriHalSubGhzCaptureCallback cb, void* ctx);

// Antenne wechseln (INT ↔ EXT); nur wirksam wenn EXT verbunden
void rollforge_rf_switch(RollForgeApp* app);
