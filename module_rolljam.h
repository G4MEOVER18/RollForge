#pragma once
#include "rollforge_app.h"

void rj_enter(RollForgeApp* app);
void rj_exit(RollForgeApp* app);
void rj_input(RollForgeApp* app, InputEvent* ev);
void rj_tick(RollForgeApp* app);
void rj_draw(Canvas* canvas, RollForgeApp* app);
