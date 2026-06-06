#pragma once
#include "rollforge_app.h"

bool rollforge_rf_init(RollForgeApp* app);
void rollforge_rf_deinit(RollForgeApp* app);
void rollforge_rf_idle(RollForgeApp* app);

// Sichere TX/RX-Starter: idle → load_preset → set_frequency → start_async
// Müssen vor jedem neuen async-TX oder async-RX verwendet werden.
void rollforge_rf_start_tx(RollForgeApp* app, FuriHalSubGhzAsyncTxCallback cb, void* ctx);
void rollforge_rf_start_rx(RollForgeApp* app, FuriHalSubGhzAsyncRxCallback cb, void* ctx);
