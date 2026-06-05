#pragma once
#include "rollforge_app.h"

void jm_enter(RollForgeApp* app);
void jm_exit(RollForgeApp* app);
void jm_input(RollForgeApp* app, InputEvent* ev);
void jm_tick(RollForgeApp* app);
void jm_draw(Canvas* canvas, RollForgeApp* app);
