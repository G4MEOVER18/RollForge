#pragma once
#include "rollforge_app.h"

void rl_enter(RollForgeApp* app);
void rl_exit(RollForgeApp* app);
void rl_input(RollForgeApp* app, InputEvent* ev);
void rl_tick(RollForgeApp* app);
void rl_draw(Canvas* canvas, RollForgeApp* app);
