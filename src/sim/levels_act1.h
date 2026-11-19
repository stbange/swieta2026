#ifndef SWIETA_LEVELS_ACT1_H
#define SWIETA_LEVELS_ACT1_H

#include "sim/world.h"

void build_day02_tombraider(GameWorld *w);
void build_day03_counterstrike(GameWorld *w);
void build_day04_quake(GameWorld *w);
void build_day05_blood(GameWorld *w);
void build_day06_doom(GameWorld *w);

/* Per-day custom interactions, tick logic, and biome dynamic lights */
void act1_handle_switch_use(GameWorld *w, Player *p, SwitchTrigger *sw);
void act1_step_level(GameWorld *w, float dt);
void act1_rebuild_lights(GameWorld *w, float flick);

#endif /* SWIETA_LEVELS_ACT1_H */
