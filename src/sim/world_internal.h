#ifndef SWIETA_WORLD_INTERNAL_H
#define SWIETA_WORLD_INTERNAL_H

#include "sim/world.h"

/* Shared internal helpers exported by world.c to modular level builders (levels_act1.c, etc.) */
void world_set_banner(GameWorld *w, const char *msg, float duration);
void world_set_objective(GameWorld *w, Vec3 pos, const char *txt);
void world_add_light(GameWorld *w, Vec3 pos, float r, float g, float b, float intensity, float radius);
void world_spawn_explosion(GameWorld *w, Vec3 pos, float radius, float r, float g, float b);
void world_clear_entities(GameWorld *w);

void world_set_loadout(GameWorld *w, const WeaponId *weapons, int count, uint32_t initial_owned);
void world_spawn_players(GameWorld *w, Vec3 spawn_pos, float yaw, uint32_t initial_owned, WeaponId start_weap);

int world_add_enemy(GameWorld *w, EnemyKind kind, Vec3 pos);
void world_add_pickup(GameWorld *w, PickupKind kind, Vec3 pos);
void world_add_weapon_pickup(GameWorld *w, WeaponId weap, Vec3 pos);
int world_add_door(GameWorld *w, int x0, int y0, int z0, int x1, int y1, int z1, uint32_t key, uint8_t vtype);
void world_open_door(GameWorld *w, DoorGate *d);
void world_add_relay(GameWorld *w, int vx, int vy, int vz);
void world_add_secret(GameWorld *w, Vec3 pos, float radius, const char *name);
int world_add_pushblock(GameWorld *w, int vx, int vy, int vz, int target_x, int target_z, int door_idx);
int world_add_switch(GameWorld *w, SwitchKind kind, Vec3 pos, int door_idx, int tag, const char *label);
void world_add_barrel(GameWorld *w, Vec3 pos);
void world_add_teleporter(GameWorld *w, Vec3 src, Vec3 dst, float dst_yaw);
void world_add_jumppad(GameWorld *w, Vec3 pos, Vec3 boost_vel);
void world_add_boulder(GameWorld *w, Vec3 start_pos, Vec3 vel, Vec3 trigger_pos, float trigger_radius, float stop_z);

void world_damage_enemy(GameWorld *w, Enemy *e, Player *attacker, int attacker_enemy_idx, int dmg);
void world_explode_at(GameWorld *w, Vec3 pos, float radius, int max_dmg, int owner_player);
void world_award_day_shard(GameWorld *w, int day, Player *p, const char *msg);

#endif /* SWIETA_WORLD_INTERNAL_H */
