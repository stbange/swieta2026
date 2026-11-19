#include "sim/levels_act1.h"
#include "sim/world_internal.h"

#include <math.h>
#include <stdio.h>

/* ============================================================================
 * DAY 2 (Dec 2) — TOMB RAIDER HOMAGE: "VAULT OF THE PIXEL SUN"
 * ============================================================================ */
void build_day02_tombraider(GameWorld *w) {
    w->scene = SCENE_DAY02;
    w->active_day = 2;
    world_clear_entities(w);
    voxel_world_clear(&w->voxels);

    w->cistern_flooded = false;
    w->boss_phase = 1;
    w->boss_defeated = false;
    w->checkpoint_stage = 0;

    static const WeaponId kLoadout[4] = {
        WEAP_DUAL_PISTOLS, WEAP_SHOTGUN, WEAP_FLARE, WEAP_MAGNUM
    };
    uint32_t init_owned = (1u << WEAP_DUAL_PISTOLS) | (1u << WEAP_FLARE);
    world_set_loadout(w, kLoadout, 4, init_owned);

    /* Base temple foundation */
    voxel_fill_box(&w->voxels, 16, 0, 4, 78, 0, 120, VOX_SANDSTONE);

    /* ---- BEAT 1: SUNKEN TEMPLE ANTECHAMBER & PUSH-BLOCK PUZZLE (z=84..118) ---- */
    voxel_fill_box(&w->voxels, 28, 1, 84, 60, 10, 118, VOX_SANDSTONE);
    voxel_fill_box(&w->voxels, 29, 1, 85, 59, 9, 117, VOX_AIR);
    /* Decorative columns & flickering torch pedestals */
    voxel_fill_box(&w->voxels, 35, 1, 108, 36, 9, 109, VOX_STONE);
    voxel_fill_box(&w->voxels, 52, 1, 108, 53, 9, 109, VOX_STONE);
    voxel_set(&w->voxels, 37, 2, 108, VOX_FIRE);
    voxel_set(&w->voxels, 51, 2, 108, VOX_FIRE);
    voxel_set(&w->voxels, 37, 2, 92, VOX_FIRE);
    voxel_set(&w->voxels, 51, 2, 92, VOX_FIRE);

    /* Golden Pressure Plate at x=43..44, y=0, z=94..95 */
    voxel_fill_box(&w->voxels, 43, 0, 94, 44, 0, 95, VOX_GOLD);
    /* Door #0: Temple Cistern Gate (opens when Push-Block settles on Golden Plate!) */
    int d_cistern = world_add_door(w, 42, 1, 84, 46, 4, 84, 0u, VOX_DOOR_LOCKED);
    world_add_pushblock(w, 43, 1, 96, 43, 94, d_cistern);

    /* Secret #1: Dark Spike Crypt on East Wall (x=60..74, z=92..106) */
    voxel_fill_box(&w->voxels, 60, 1, 92, 74, 7, 106, VOX_STONE);
    voxel_fill_box(&w->voxels, 60, 1, 93, 73, 6, 105, VOX_AIR);
    voxel_fill_box(&w->voxels, 60, 1, 97, 60, 3, 100, VOX_AIR);
    voxel_fill_box(&w->voxels, 63, 1, 93, 64, 1, 105, VOX_SPIKES); /* Jump over with Flare light! */
    world_add_secret(w, vec3_make(70.0f, 1.5f, 99.0f), 4.0f, "SUN-PRIEST SPIKE CRYPT");

    /* ---- BEAT 2: 3-LEVEL FLOODED CISTERN (x=24..64, z=50..83) ---- */
    voxel_fill_box(&w->voxels, 24, 1, 50, 64, 12, 83, VOX_SANDSTONE);
    voxel_fill_box(&w->voxels, 25, 1, 51, 63, 11, 82, VOX_AIR);
    voxel_fill_box(&w->voxels, 42, 1, 83, 46, 4, 83, VOX_AIR); /* Connect cleanly behind Door #0 */
    /* Entry landing on South edge */
    voxel_fill_box(&w->voxels, 40, 1, 80, 48, 1, 82, VOX_STONE);
    /* West climbable/stepped ledge gallery up to Cistern Valve (y=4) */
    voxel_fill_box(&w->voxels, 25, 1, 76, 30, 1, 81, VOX_STONE);
    voxel_fill_box(&w->voxels, 25, 1, 73, 30, 2, 75, VOX_STONE);
    voxel_fill_box(&w->voxels, 25, 1, 63, 30, 3, 72, VOX_STONE);
    /* East balcony holding Shotgun (y=4, accessible via mantle or swimming after flood) */
    voxel_fill_box(&w->voxels, 56, 1, 62, 63, 3, 72, VOX_STONE);
    voxel_fill_box(&w->voxels, 56, 1, 73, 61, 2, 75, VOX_STONE);
    voxel_fill_box(&w->voxels, 56, 1, 76, 61, 1, 79, VOX_STONE);
    /* Initial shallow pool in cistern floor */
    voxel_fill_box(&w->voxels, 31, 1, 54, 55, 1, 78, VOX_WATER);
    /* Upper North Sanctuary Deck (y=5, top surface 5.0 -> reachable once cistern floods!) */
    voxel_fill_box(&w->voxels, 25, 1, 50, 63, 4, 53, VOX_SANDSTONE);
    /* Door #1: Upper Sanctuary Gate (opens when Cistern Valve flooded!) */
    int d_boulder = world_add_door(w, 42, 5, 50, 46, 8, 50, 0u, VOX_DOOR_LOCKED);
    /* Switch #0: Cistern Water Valve on West Ledge (28.5, 4.0, 66.0) */
    voxel_set(&w->voxels, 27, 4, 66, VOX_GOLD);
    world_add_switch(w, SW_CISTERN_VALVE, vec3_make(28.5f, 4.5f, 66.0f), d_boulder, 0, "CISTERN WATER VALVE");

    /* Secret #2: Underwater Offering Alcove in NW Cistern corner */
    voxel_fill_box(&w->voxels, 25, 1, 54, 29, 3, 58, VOX_AIR);
    voxel_fill_box(&w->voxels, 25, 1, 54, 29, 1, 58, VOX_WATER);
    world_add_secret(w, vec3_make(27.0f, 2.0f, 56.0f), 3.0f, "SUNKEN CISTERN OFFERING");

    /* ---- BEAT 3: ROLLING BOULDER CORRIDOR (x=38..50, z=28..49, floor y=5) ---- */
    voxel_fill_box(&w->voxels, 38, 1, 28, 50, 4, 49, VOX_SANDSTONE);
    voxel_fill_box(&w->voxels, 38, 5, 28, 38, 10, 49, VOX_SANDSTONE);
    voxel_fill_box(&w->voxels, 50, 5, 28, 50, 10, 49, VOX_SANDSTONE);
    voxel_fill_box(&w->voxels, 38, 10, 28, 50, 10, 49, VOX_SANDSTONE);
    /* East safety alcove holding Blue Relic Key */
    voxel_fill_box(&w->voxels, 50, 1, 35, 54, 4, 41, VOX_SANDSTONE);
    voxel_fill_box(&w->voxels, 50, 5, 35, 54, 9, 41, VOX_SANDSTONE);
    voxel_fill_box(&w->voxels, 50, 5, 36, 53, 8, 40, VOX_AIR);
    voxel_set(&w->voxels, 52, 5, 38, VOX_GOLD);
    /* Rolling Boulder Trap (triggers when player passes z=43, rolls north -> z=30!) */
    world_add_boulder(w, vec3_make(44.0f, 6.2f, 48.0f), vec3_make(0.0f, 0.0f, -10.5f),
                      vec3_make(44.0f, 5.5f, 43.0f), 3.5f, 30.0f);
    /* Door #2: Blue Relic Valley Gate at z=28 */
    world_add_door(w, 42, 5, 28, 46, 8, 28, KEY_BLUE, VOX_DOOR_BLUE);

    /* ---- BEAT 4: SUNLIT LOST VALLEY & T-REX COURTYARD (x=18..76, z=6..27, floor y=5) ---- */
    voxel_fill_box(&w->voxels, 18, 1, 6, 76, 4, 27, VOX_STONE);
    voxel_fill_box(&w->voxels, 18, 5, 6, 76, 14, 6, VOX_LEAVES);
    voxel_fill_box(&w->voxels, 18, 5, 6, 18, 14, 27, VOX_STONE);
    voxel_fill_box(&w->voxels, 76, 5, 6, 76, 14, 27, VOX_STONE);
    voxel_fill_box(&w->voxels, 18, 5, 27, 37, 12, 27, VOX_STONE);
    voxel_fill_box(&w->voxels, 51, 5, 27, 76, 12, 27, VOX_STONE);
    /* Valley rock pillars for cover against T-Rex */
    voxel_fill_box(&w->voxels, 34, 5, 14, 36, 8, 16, VOX_SANDSTONE);
    voxel_fill_box(&w->voxels, 54, 5, 14, 56, 8, 16, VOX_SANDSTONE);

    /* Sunlit Waterfall & Secret #3 Waterfall Grotto on West Cliff */
    voxel_fill_box(&w->voxels, 19, 5, 14, 24, 9, 20, VOX_AIR);
    voxel_fill_box(&w->voxels, 24, 5, 15, 24, 12, 19, VOX_WATER);
    world_add_secret(w, vec3_make(21.0f, 6.0f, 17.0f), 3.2f, "WATERFALL GROTTO");

    /* Scion Prism Golden Exit Altar */
    voxel_fill_box(&w->voxels, 43, 5, 8, 45, 5, 10, VOX_GOLD);
    world_add_switch(w, SW_EXIT_SHARD, vec3_make(44.5f, 6.0f, 9.5f), -1, 2, "SCION PRISM ALTAR");

    /* Return Portal */
    w->return_portal_pos = vec3_make(31.5f, 1.0f, 114.5f);

    /* ---- ENEMIES ---- */
    world_add_enemy(w, ENEMY_WOLF,        vec3_make(38.0f, 1.0f, 102.0f));
    world_add_enemy(w, ENEMY_WOLF,        vec3_make(50.0f, 1.0f, 98.0f));
    world_add_enemy(w, ENEMY_BAT,         vec3_make(44.0f, 5.2f, 90.0f));
    world_add_enemy(w, ENEMY_BAT,         vec3_make(38.0f, 6.2f, 68.0f));
    world_add_enemy(w, ENEMY_STONE_GUARD, vec3_make(58.0f, 4.0f, 66.0f));
    world_add_enemy(w, ENEMY_STONE_GUARD, vec3_make(44.0f, 5.0f, 34.0f));
    world_add_enemy(w, ENEMY_WOLF,        vec3_make(32.0f, 5.0f, 20.0f));
    world_add_enemy(w, ENEMY_BOSS_TREX,   vec3_make(46.0f, 5.0f, 16.0f)); /* index 7: T-Rex Boss! */

    /* ---- PICKUPS ---- */
    world_add_weapon_pickup(w, WEAP_MAGNUM,  vec3_make(70.0f, 1.2f, 99.0f)); /* Secret #1 */
    world_add_pickup(w, PICKUP_UTILITY,      vec3_make(34.0f, 1.2f, 92.0f)); /* Flares */
    world_add_weapon_pickup(w, WEAP_SHOTGUN, vec3_make(59.0f, 4.2f, 65.0f)); /* Cistern Balcony */
    world_add_pickup(w, PICKUP_MEDKIT,       vec3_make(27.0f, 2.2f, 56.0f)); /* Secret #2 */
    world_add_pickup(w, PICKUP_ARMOR,        vec3_make(27.0f, 2.2f, 57.5f));
    world_add_pickup(w, PICKUP_KEY_BLUE,     vec3_make(51.5f, 5.8f, 38.0f)); /* Boulder Alcove */
    world_add_pickup(w, PICKUP_SHELLS,       vec3_make(21.0f, 5.5f, 17.0f)); /* Secret #3 */
    world_add_pickup(w, PICKUP_MEDKIT,       vec3_make(62.0f, 5.2f, 22.0f));
    world_add_pickup(w, PICKUP_BULLETS,      vec3_make(30.0f, 5.2f, 12.0f));

    world_spawn_players(w, vec3_make(44.0f, 1.0f, 114.0f), 0.0f, init_owned, WEAP_DUAL_PISTOLS);
    world_set_objective(w, vec3_make(44.0f, 1.5f, 96.0f), "PUSH GOLDEN STONE BLOCK ONTO PLATE (USE E)");
    world_set_banner(w, "DAY 2: VAULT OF THE PIXEL SUN - PUSH BLOCK ONTO GOLDEN PLATE!", 6.0f);
}

/* ============================================================================
 * DAY 3 (Dec 3) — COUNTER-STRIKE HOMAGE: "de_yule_dust"
 * ============================================================================ */
void build_day03_counterstrike(GameWorld *w) {
    w->scene = SCENE_DAY03;
    w->active_day = 3;
    world_clear_entities(w);
    voxel_world_clear(&w->voxels);

    w->bombs_defused = 0;
    w->boss_phase = 1;
    w->boss_defeated = false;
    w->checkpoint_stage = 0;

    static const WeaponId kLoadout[5] = {
        WEAP_USP, WEAP_DEAGLE, WEAP_CARBINE, WEAP_SNIPER, WEAP_FLASH_SMOKE
    };
    uint32_t init_owned = (1u << WEAP_USP) | (1u << WEAP_FLASH_SMOKE);
    world_set_loadout(w, kLoadout, 5, init_owned);

    /* Sandy sunlit de_yule_dust ground & perimeter walls */
    voxel_fill_box(&w->voxels, 10, 0, 10, 118, 0, 120, VOX_SANDSTONE);
    voxel_fill_box(&w->voxels, 10, 1, 10, 118, 11, 10, VOX_SANDSTONE);
    voxel_fill_box(&w->voxels, 10, 1, 120, 118, 11, 120, VOX_SANDSTONE);
    voxel_fill_box(&w->voxels, 10, 1, 10, 10, 11, 120, VOX_SANDSTONE);
    voxel_fill_box(&w->voxels, 118, 1, 10, 118, 11, 120, VOX_SANDSTONE);

    /* ---- BEAT 1: CT BAZAAR SPAWN & MID ARCHWAY (x=46..74, z=74..118) ---- */
    voxel_fill_box(&w->voxels, 45, 1, 96, 45, 8, 119, VOX_SANDSTONE);
    voxel_fill_box(&w->voxels, 75, 1, 74, 75, 8, 119, VOX_SANDSTONE);
    /* Festive winter arch & holiday gift crates */
    voxel_fill_box(&w->voxels, 46, 7, 94, 74, 8, 96, VOX_SANDSTONE);
    voxel_fill_box(&w->voxels, 56, 1, 102, 58, 2, 104, VOX_CRATE);
    voxel_set(&w->voxels, 66, 1, 110, VOX_GIFT);
    /* Switch #0: CT Spawn Buy-Zone Crate */
    voxel_fill_box(&w->voxels, 60, 1, 107, 61, 1, 108, VOX_CRATE);
    voxel_set(&w->voxels, 60, 2, 107, VOX_GOLD);
    world_add_switch(w, SW_CS_BUY_CRATE, vec3_make(60.5f, 1.8f, 107.5f), -1, 0, "CT BUY-ZONE CRATE ($)");

    /* ---- BEAT 2: WEST UNDERPASS TUNNEL & BOMBSITE A (x=14..45, z=56..95) ---- */
    voxel_fill_box(&w->voxels, 14, 1, 56, 45, 8, 56, VOX_SANDSTONE);
    voxel_fill_box(&w->voxels, 45, 1, 56, 45, 8, 78, VOX_SANDSTONE);
    /* Open archway from Mid (x=45, z=80..85) into Tunnel */
    voxel_fill_box(&w->voxels, 45, 1, 80, 45, 4, 85, VOX_AIR);
    /* Bombsite A crates & hazard floor */
    voxel_fill_box(&w->voxels, 25, 0, 62, 31, 0, 68, VOX_HAZARD);
    voxel_fill_box(&w->voxels, 22, 1, 66, 24, 2, 68, VOX_CRATE);
    voxel_fill_box(&w->voxels, 34, 1, 62, 36, 2, 64, VOX_CRATE);
    /* Door #0: Catwalk Security Gate opened after Bombsite A defused! */
    int d_catwalk = world_add_door(w, 45, 1, 60, 45, 4, 64, 0u, VOX_DOOR_LOCKED);
    /* Switch #1: Corrupted C4 Bomb A */
    voxel_set(&w->voxels, 28, 1, 65, VOX_RELAY_CORE);
    world_add_switch(w, SW_CS_C4_BOMB, vec3_make(28.5f, 1.5f, 65.5f), d_catwalk, 1, "C4 BOMBSITE A");

    /* Secret #1: Smuggler Crate behind breakable glass in Tunnel */
    voxel_fill_box(&w->voxels, 14, 1, 83, 20, 5, 90, VOX_SANDSTONE);
    voxel_fill_box(&w->voxels, 15, 1, 84, 19, 4, 89, VOX_AIR);
    voxel_fill_box(&w->voxels, 20, 1, 85, 20, 3, 88, VOX_GLASS);
    world_add_secret(w, vec3_make(17.0f, 1.5f, 86.5f), 3.0f, "TUNNEL SMUGGLER STASH");

    /* ---- BEAT 3: MID OVERPASS CATWALK & LONG BAZAAR (x=46..108, z=44..72) ---- */
    /* Stepped sandstone stairs from x=46..50 up to Catwalk y=5 */
    for (int s = 0; s < 4; ++s) {
        voxel_fill_box(&w->voxels, 46 + s, 1, 60, 46 + s, 1 + s, 64, VOX_SANDSTONE);
    }
    /* Mid Catwalk deck (x=50..74, z=54..66, top y=5) */
    voxel_fill_box(&w->voxels, 50, 1, 54, 74, 4, 66, VOX_SANDSTONE);
    /* Switch #2: Mid Catwalk Buy Crate */
    voxel_set(&w->voxels, 60, 5, 58, VOX_GOLD);
    world_add_switch(w, SW_CS_BUY_CRATE, vec3_make(60.5f, 5.8f, 58.5f), -1, 1, "MID BUY-ZONE CRATE ($)");
    /* Stairs down from Catwalk East side (x=75..78) to Long Bazaar (y=1) */
    for (int s = 0; s < 4; ++s) {
        voxel_fill_box(&w->voxels, 75 + s, 1, 58, 75 + s, 4 - s, 62, VOX_SANDSTONE);
    }
    /* Secret #2: Sniper Nest Balcony north of Catwalk */
    voxel_fill_box(&w->voxels, 56, 1, 44, 64, 4, 53, VOX_SANDSTONE);
    world_add_secret(w, vec3_make(60.0f, 5.5f, 48.0f), 3.5f, "CATWALK SNIPER NEST");

    /* Long Bazaar cover crates (x=80..108, z=44..72) */
    voxel_fill_box(&w->voxels, 86, 1, 54, 88, 2, 56, VOX_CRATE);
    voxel_fill_box(&w->voxels, 98, 1, 48, 100, 2, 50, VOX_CRATE);

    /* ---- BEAT 4: BOMBSITE B COURTYARD & EXTRACTION (x=74..114, z=14..42) ---- */
    voxel_fill_box(&w->voxels, 74, 1, 14, 74, 8, 44, VOX_SANDSTONE);
    voxel_fill_box(&w->voxels, 74, 1, 42, 86, 8, 42, VOX_SANDSTONE);
    voxel_fill_box(&w->voxels, 96, 1, 42, 117, 8, 42, VOX_SANDSTONE);
    voxel_fill_box(&w->voxels, 88, 0, 22, 95, 0, 28, VOX_HAZARD);
    voxel_fill_box(&w->voxels, 82, 1, 26, 84, 2, 28, VOX_CRATE);
    voxel_fill_box(&w->voxels, 98, 1, 28, 100, 2, 30, VOX_CRATE);
    /* Switch #3: Corrupted C4 Bomb B */
    voxel_set(&w->voxels, 92, 1, 24, VOX_RELAY_CORE);
    world_add_switch(w, SW_CS_C4_BOMB, vec3_make(92.5f, 1.5f, 24.5f), -1, 2, "C4 BOMBSITE B");

    /* Secret #3: Holiday Vault behind Cracked Wall East of Site B */
    voxel_fill_box(&w->voxels, 109, 1, 20, 116, 6, 28, VOX_SANDSTONE);
    voxel_fill_box(&w->voxels, 110, 1, 21, 115, 5, 27, VOX_AIR);
    voxel_fill_box(&w->voxels, 109, 1, 23, 109, 3, 25, VOX_GLASS);
    world_add_secret(w, vec3_make(112.5f, 1.5f, 24.0f), 3.0f, "BAZAAR HOLIDAY VAULT");

    /* Switch #4: Extraction Pliers Shrine (enabled when both C4 bombs defused!) */
    voxel_fill_box(&w->voxels, 91, 1, 15, 93, 1, 17, VOX_GOLD);
    world_add_switch(w, SW_EXIT_SHARD, vec3_make(92.5f, 2.0f, 16.5f), -1, 3, "DEFUSAL PLIERS EXTRACTION");

    w->return_portal_pos = vec3_make(49.5f, 1.0f, 114.5f);

    /* ---- ENEMIES (Tactical Syndicate Riflemen & Snipers) ---- */
    world_add_enemy(w, ENEMY_SYNDICATE_RIFLE,  vec3_make(58.0f, 1.0f, 88.0f));
    world_add_enemy(w, ENEMY_SYNDICATE_SNIPER, vec3_make(64.0f, 1.0f, 78.0f));
    world_add_enemy(w, ENEMY_SYNDICATE_RIFLE,  vec3_make(36.0f, 1.0f, 76.0f));
    world_add_enemy(w, ENEMY_SYNDICATE_RIFLE,  vec3_make(26.0f, 1.0f, 62.0f));
    world_add_enemy(w, ENEMY_SYNDICATE_SNIPER, vec3_make(94.0f, 1.0f, 58.0f));
    world_add_enemy(w, ENEMY_SYNDICATE_RIFLE,  vec3_make(88.0f, 1.0f, 46.0f));
    world_add_enemy(w, ENEMY_SYNDICATE_RIFLE,  vec3_make(84.0f, 1.0f, 24.0f));
    world_add_enemy(w, ENEMY_SYNDICATE_SNIPER, vec3_make(102.0f, 1.0f, 20.0f));
    world_add_enemy(w, ENEMY_PIGGUARD,         vec3_make(92.0f, 1.0f, 22.0f)); /* Syndicate Heavy Captain */

    /* ---- PICKUPS ---- */
    world_add_pickup(w, PICKUP_CASH,         vec3_make(17.0f, 1.2f, 86.5f)); /* Secret #1 */
    world_add_pickup(w, PICKUP_ARMOR,        vec3_make(17.0f, 1.2f, 88.0f));
    world_add_weapon_pickup(w, WEAP_CARBINE, vec3_make(34.0f, 1.2f, 68.0f)); /* Free loot at Site A */
    world_add_pickup(w, PICKUP_UTILITY,      vec3_make(54.0f, 5.2f, 62.0f)); /* Flash/Smoke refill */
    world_add_pickup(w, PICKUP_BULLETS,      vec3_make(60.0f, 5.2f, 48.0f)); /* Secret #2 */
    world_add_weapon_pickup(w, WEAP_SNIPER,  vec3_make(60.0f, 5.2f, 46.5f)); /* Secret #2 Sniper! */
    world_add_pickup(w, PICKUP_MEDKIT,       vec3_make(112.5f, 1.2f, 24.0f));/* Secret #3 */
    world_add_pickup(w, PICKUP_ARMOR,        vec3_make(112.5f, 1.2f, 26.0f));

    world_spawn_players(w, vec3_make(60.0f, 1.0f, 113.0f), 0.0f, init_owned, WEAP_USP);
    for (int i = 0; i < w->player_count; ++i) {
        w->players[i].cash = 1000;
        w->players[i].ammo_utility = 6;
    }
    world_set_objective(w, vec3_make(28.5f, 1.5f, 65.5f), "BUY GEAR AT CRATE (E) - CLEAR & DEFUSE BOMBSITE A");
    world_set_banner(w, "DAY 3: DE_YULE_DUST - BUY GEAR (E), USE FLASH/SMOKE, DEFUSE A & B!", 6.0f);
}

/* ============================================================================
 * DAY 4 (Dec 4) — QUAKE HOMAGE: "SLIPGATE OF BRASS & RUST"
 * ============================================================================ */
void build_day04_quake(GameWorld *w) {
    w->scene = SCENE_DAY04;
    w->active_day = 4;
    world_clear_entities(w);
    voxel_world_clear(&w->voxels);

    w->boss_phase = 1;
    w->boss_defeated = false;
    w->checkpoint_stage = 0;

    static const WeaponId kLoadout[4] = {
        WEAP_SHOTGUN, WEAP_NAILGUN, WEAP_ROCKET, WEAP_THUNDERBOLT
    };
    uint32_t init_owned = (1u << WEAP_SHOTGUN) | (1u << WEAP_NAILGUN);
    world_set_loadout(w, kLoadout, 4, init_owned);

    /* Gothic brass & copper dungeon foundation */
    voxel_fill_box(&w->voxels, 20, 0, 4, 118, 0, 120, VOX_RUST_BRASS);

    /* ---- BEAT 1: BRASS SLIPGATE ATRIUM & ROCKET JUMP VAULT (x=24..58, z=86..118) ---- */
    voxel_fill_box(&w->voxels, 24, 1, 86, 58, 12, 118, VOX_RUST_BRASS);
    voxel_fill_box(&w->voxels, 25, 1, 87, 57, 11, 117, VOX_AIR);
    /* Glowing lava moat flanking the central Rocket Launcher bridge */
    voxel_fill_box(&w->voxels, 26, 0, 94, 37, 0, 102, VOX_LAVA);
    voxel_fill_box(&w->voxels, 43, 0, 94, 56, 0, 102, VOX_LAVA);
    /* High brass gallery at y=6 (reachable via Jump Pad #0 OR Rocket Jump!) */
    voxel_fill_box(&w->voxels, 25, 5, 87, 57, 5, 92, VOX_RUST_BRASS);
    voxel_fill_box(&w->voxels, 51, 0, 104, 54, 0, 107, VOX_SLIPGATE);
    world_add_jumppad(w, vec3_make(52.5f, 1.0f, 105.5f), vec3_make(0.0f, 13.5f, -7.0f));
    /* Door #0: Portcullis opened by Gallery Switch #0 */
    int d_trench = world_add_door(w, 38, 1, 86, 42, 4, 86, 0u, VOX_DOOR_LOCKED);
    voxel_set(&w->voxels, 40, 6, 88, VOX_GOLD);
    world_add_switch(w, SW_DOOR_LEVER, vec3_make(40.5f, 6.5f, 89.0f), d_trench, 1, "BRASS PORTCULLIS LEVER");

    /* Secret #1: High Rocket-Jump Ledge in SW Atrium corner (y=7) */
    voxel_fill_box(&w->voxels, 25, 6, 104, 31, 6, 111, VOX_RUST_BRASS);
    world_add_secret(w, vec3_make(28.0f, 7.5f, 107.5f), 3.5f, "ROCKETEER QUAD ALCOVE");

    /* ---- BEAT 2: FLOODED RUST TRENCH & THUNDERBOLT (x=24..64, z=48..85) ---- */
    voxel_fill_box(&w->voxels, 24, 1, 48, 64, 12, 85, VOX_RUST_BRASS);
    voxel_fill_box(&w->voxels, 25, 1, 49, 63, 11, 84, VOX_AIR);
    voxel_fill_box(&w->voxels, 38, 1, 85, 42, 4, 85, VOX_AIR); /* Connect cleanly behind Door #0 */
    /* Flooded water trench (y=1..2) + dry stone walkways (y=3) */
    voxel_fill_box(&w->voxels, 32, 1, 54, 56, 2, 78, VOX_WATER);
    voxel_fill_box(&w->voxels, 25, 1, 49, 31, 2, 84, VOX_STONE);
    voxel_fill_box(&w->voxels, 57, 1, 49, 63, 2, 84, VOX_STONE);
    voxel_fill_box(&w->voxels, 32, 1, 80, 56, 2, 84, VOX_STONE);
    voxel_fill_box(&w->voxels, 32, 1, 49, 56, 2, 53, VOX_STONE);
    /* Steps from Atrium y=1 up to Walkway y=3 */
    voxel_fill_box(&w->voxels, 38, 1, 84, 42, 1, 84, VOX_STONE);
    voxel_fill_box(&w->voxels, 38, 2, 84, 42, 4, 84, VOX_AIR);
    voxel_fill_box(&w->voxels, 38, 1, 82, 42, 2, 83, VOX_STONE);
    /* Door #1: Blue Rune Gate leading to Slipgate #0 */
    world_add_door(w, 55, 3, 51, 55, 6, 53, KEY_BLUE, VOX_DOOR_BLUE);
    /* Slipgate Teleporter #0 at (60, 3, 52) -> teleports to Arena (74, 3, 76) & TELEFRAGS guard! */
    voxel_fill_box(&w->voxels, 59, 2, 51, 61, 2, 53, VOX_SLIPGATE);
    world_add_teleporter(w, vec3_make(60.0f, 3.0f, 52.0f), vec3_make(74.0f, 3.0f, 76.0f), 0.0f);

    /* ---- BEAT 3: TWIN SLIPGATE ARENA & QUAD GAUNTLET (x=66..114, z=46..84) ---- */
    voxel_fill_box(&w->voxels, 66, 1, 46, 114, 14, 84, VOX_RUST_BRASS);
    voxel_fill_box(&w->voxels, 67, 1, 47, 113, 13, 83, VOX_AIR);
    voxel_fill_box(&w->voxels, 67, 1, 47, 113, 2, 83, VOX_STONE);
    /* Jump Pad #1 boosting to Red Rune Platform (y=8) */
    voxel_fill_box(&w->voxels, 98, 2, 62, 100, 2, 64, VOX_SLIPGATE);
    world_add_jumppad(w, vec3_make(99.0f, 3.0f, 63.0f), vec3_make(6.0f, 13.0f, -6.0f));
    voxel_fill_box(&w->voxels, 102, 7, 48, 112, 7, 60, VOX_RUST_BRASS);
    /* Secret #2: High SE Archway Balcony */
    voxel_fill_box(&w->voxels, 102, 7, 72, 112, 7, 82, VOX_RUST_BRASS);
    world_add_secret(w, vec3_make(107.0f, 8.5f, 77.0f), 3.5f, "BRASS ARCHWAY CACHE");

    /* Door #2: Red Rune Gate guarding Slipgate #1 -> Shambler Cathedral */
    voxel_fill_box(&w->voxels, 86, 3, 47, 94, 7, 52, VOX_RUST_BRASS);
    voxel_fill_box(&w->voxels, 87, 3, 48, 93, 6, 51, VOX_AIR);
    world_add_door(w, 88, 3, 52, 92, 6, 52, KEY_RED, VOX_DOOR_RED);
    voxel_fill_box(&w->voxels, 89, 2, 48, 91, 2, 50, VOX_SLIPGATE);
    world_add_teleporter(w, vec3_make(90.0f, 3.0f, 49.0f), vec3_make(64.0f, 5.0f, 33.0f), 0.0f);

    /* ---- BEAT 4: THE SHAMBLER CATHEDRAL (x=36..92, z=6..36, y=5) ---- */
    voxel_fill_box(&w->voxels, 36, 1, 6, 92, 4, 36, VOX_RUST_BRASS);
    voxel_fill_box(&w->voxels, 36, 5, 6, 92, 16, 6, VOX_RUST_BRASS);
    voxel_fill_box(&w->voxels, 36, 5, 36, 92, 16, 36, VOX_RUST_BRASS);
    voxel_fill_box(&w->voxels, 36, 5, 6, 36, 16, 36, VOX_RUST_BRASS);
    voxel_fill_box(&w->voxels, 92, 5, 6, 92, 16, 36, VOX_RUST_BRASS);
    /* Lava channels & cover pillars */
    voxel_fill_box(&w->voxels, 44, 4, 12, 46, 4, 28, VOX_LAVA);
    voxel_fill_box(&w->voxels, 82, 4, 12, 84, 4, 28, VOX_LAVA);
    voxel_fill_box(&w->voxels, 54, 5, 22, 55, 9, 23, VOX_STONE);
    voxel_fill_box(&w->voxels, 72, 5, 22, 73, 9, 23, VOX_STONE);
    /* Secret #3: Cracked Sanctum Wall on West Side */
    voxel_fill_box(&w->voxels, 37, 5, 14, 42, 9, 20, VOX_STONE);
    voxel_fill_box(&w->voxels, 38, 5, 15, 41, 8, 19, VOX_AIR);
    voxel_fill_box(&w->voxels, 42, 5, 16, 42, 7, 18, VOX_CRACKED);
    world_add_secret(w, vec3_make(39.5f, 6.0f, 17.0f), 3.0f, "SLIPGATE SANCTUM");

    /* Switch #1: Slipgate Rune Exit Altar */
    voxel_fill_box(&w->voxels, 63, 5, 8, 65, 5, 10, VOX_SLIPGATE);
    world_add_switch(w, SW_EXIT_SHARD, vec3_make(64.5f, 6.0f, 9.5f), -1, 4, "SLIPGATE RUNE ALTAR");

    w->return_portal_pos = vec3_make(28.5f, 1.0f, 114.5f);

    /* ---- ENEMIES ---- */
    world_add_enemy(w, ENEMY_FIEND,         vec3_make(34.0f, 1.0f, 106.0f));
    world_add_enemy(w, ENEMY_OGRE,          vec3_make(46.0f, 6.0f, 90.0f));
    world_add_enemy(w, ENEMY_FIEND,         vec3_make(40.0f, 1.0f, 66.0f)); /* Standing in water trench! */
    world_add_enemy(w, ENEMY_DEATH_KNIGHT,  vec3_make(48.0f, 1.0f, 62.0f)); /* Standing in water trench! */
    world_add_enemy(w, ENEMY_DEATH_KNIGHT,  vec3_make(74.0f, 3.0f, 76.0f)); /* Index 4: Telefrag victim! */
    world_add_enemy(w, ENEMY_OGRE,          vec3_make(88.0f, 3.0f, 68.0f));
    world_add_enemy(w, ENEMY_DEATH_KNIGHT,  vec3_make(96.0f, 3.0f, 56.0f));
    world_add_enemy(w, ENEMY_BOSS_SHAMBLER, vec3_make(64.0f, 5.0f, 17.0f)); /* Index 7: Shambler Boss! */

    /* ---- PICKUPS ---- */
    world_add_weapon_pickup(w, WEAP_ROCKET,      vec3_make(40.5f, 1.2f, 98.0f));
    world_add_pickup(w, PICKUP_QUAD,             vec3_make(28.0f, 7.2f, 107.5f)); /* Secret #1 */
    world_add_pickup(w, PICKUP_ARMOR,            vec3_make(29.5f, 7.2f, 107.5f));
    world_add_weapon_pickup(w, WEAP_THUNDERBOLT, vec3_make(28.0f, 3.2f, 70.0f));
    world_add_pickup(w, PICKUP_KEY_BLUE,         vec3_make(28.0f, 3.2f, 54.0f));
    world_add_pickup(w, PICKUP_QUAD,             vec3_make(90.0f, 3.2f, 64.0f));  /* Arena Quad! */
    world_add_pickup(w, PICKUP_KEY_RED,          vec3_make(107.0f, 8.2f, 54.0f));
    world_add_pickup(w, PICKUP_ROCKETS,          vec3_make(107.0f, 8.2f, 77.0f)); /* Secret #2 */
    world_add_pickup(w, PICKUP_MEDKIT,           vec3_make(39.5f, 5.5f, 17.0f));  /* Secret #3 */
    world_add_pickup(w, PICKUP_CELLS,            vec3_make(76.0f, 5.2f, 26.0f));

    world_spawn_players(w, vec3_make(40.0f, 1.0f, 113.0f), 0.0f, init_owned, WEAP_NAILGUN);
    world_set_objective(w, vec3_make(40.5f, 6.5f, 89.0f), "GRAB ROCKET LAUNCHER - JUMP TO GALLERY & FLIP LEVER");
    world_set_banner(w, "DAY 4: SLIPGATE OF BRASS & RUST - ROCKET JUMP & CLAIM QUAD DAMAGE!", 6.0f);
}

/* ============================================================================
 * DAY 5 (Dec 5) — BLOOD HOMAGE: "CARNIVAL OF THE DAMNED"
 * ============================================================================ */
void build_day05_blood(GameWorld *w) {
    w->scene = SCENE_DAY05;
    w->active_day = 5;
    world_clear_entities(w);
    voxel_world_clear(&w->voxels);

    w->train_activated = false;
    w->boss_phase = 1;
    w->boss_defeated = false;
    w->checkpoint_stage = 0;

    static const WeaponId kLoadout[5] = {
        WEAP_PITCHFORK, WEAP_FLAREGUN, WEAP_AEROSOL, WEAP_TNT, WEAP_VOODOO
    };
    uint32_t init_owned = (1u << WEAP_PITCHFORK) | (1u << WEAP_FLAREGUN) | (1u << WEAP_TNT);
    world_set_loadout(w, kLoadout, 5, init_owned);

    /* Dark gothic railway & carnival grounds */
    voxel_fill_box(&w->voxels, 16, 0, 4, 100, 0, 120, VOX_CRYPT_STONE);

    /* ---- BEAT 1: VICTORIAN RAILWAY STATION & CRACKED CRYPT WALL (x=20..56, z=88..118) ---- */
    voxel_fill_box(&w->voxels, 20, 1, 88, 56, 9, 118, VOX_CRYPT_STONE);
    voxel_fill_box(&w->voxels, 21, 1, 89, 55, 8, 117, VOX_AIR);
    /* Cracked Crypt Gate at x=36..40, y=1..3, z=88 -> Blast with TNT! */
    voxel_fill_box(&w->voxels, 36, 1, 88, 40, 3, 88, VOX_CRACKED);
    /* Ticket booth */
    voxel_fill_box(&w->voxels, 22, 1, 95, 29, 4, 102, VOX_WOOD);
    voxel_fill_box(&w->voxels, 23, 1, 96, 28, 3, 101, VOX_AIR);
    voxel_fill_box(&w->voxels, 29, 1, 97, 29, 3, 100, VOX_GLASS);
    /* Secret #1: Bell Tower Crypt behind east cracked wall */
    voxel_fill_box(&w->voxels, 48, 1, 100, 55, 5, 109, VOX_CRYPT_STONE);
    voxel_fill_box(&w->voxels, 49, 1, 101, 54, 4, 108, VOX_AIR);
    voxel_fill_box(&w->voxels, 48, 1, 103, 48, 3, 106, VOX_CRACKED);
    world_add_secret(w, vec3_make(51.5f, 1.5f, 104.5f), 3.0f, "BELL TOWER CRYPT");

    /* ---- BEAT 2: HAUNTED GHOST TRAIN YARD (x=18..76, z=56..87) ---- */
    voxel_fill_box(&w->voxels, 18, 1, 56, 76, 9, 56, VOX_CRYPT_STONE);
    voxel_fill_box(&w->voxels, 18, 1, 56, 18, 9, 88, VOX_CRYPT_STONE);
    voxel_fill_box(&w->voxels, 76, 1, 56, 76, 9, 88, VOX_CRYPT_STONE);
    /* 3-Car Voxel Ghost Train (x=22..70, z=66..74) */
    voxel_fill_box(&w->voxels, 22, 1, 66, 70, 5, 74, VOX_WOOD);
    voxel_fill_box(&w->voxels, 23, 2, 67, 69, 4, 73, VOX_AIR);
    voxel_fill_box(&w->voxels, 36, 2, 74, 40, 4, 74, VOX_AIR); /* Boarding doors */
    voxel_fill_box(&w->voxels, 50, 2, 66, 54, 4, 66, VOX_AIR);
    voxel_fill_box(&w->voxels, 36, 1, 75, 40, 1, 75, VOX_WOOD); /* Boarding step */
    /* Secret #2: Ghost Caboose behind stained glass */
    voxel_fill_box(&w->voxels, 28, 2, 67, 28, 4, 73, VOX_GLASS);
    world_add_secret(w, vec3_make(25.0f, 2.5f, 70.0f), 3.0f, "GHOST CABOOSE");

    /* Door #0: Iron Carnival Gate (opens when Ghost Train Throttle flipped!) */
    int d_carnival = world_add_door(w, 50, 1, 56, 54, 4, 56, 0u, VOX_DOOR_LOCKED);
    /* Switch #0: Ghost Train Throttle in Locomotive Engine */
    voxel_set(&w->voxels, 68, 2, 70, VOX_GOLD);
    world_add_switch(w, SW_GHOST_TRAIN, vec3_make(67.5f, 2.5f, 70.0f), d_carnival, 0, "GHOST TRAIN THROTTLE");

    /* ---- BEAT 3: CURSED CARNIVAL MIDWAY (x=26..96, z=32..55) ---- */
    voxel_fill_box(&w->voxels, 26, 1, 32, 96, 10, 32, VOX_RED_CARNIVAL);
    voxel_fill_box(&w->voxels, 26, 1, 32, 26, 10, 56, VOX_RED_CARNIVAL);
    voxel_fill_box(&w->voxels, 96, 1, 32, 96, 10, 56, VOX_RED_CARNIVAL);
    /* Fortune Tent (West) & Funhouse Vault (East) */
    voxel_fill_box(&w->voxels, 30, 1, 40, 42, 6, 49, VOX_RED_CARNIVAL);
    voxel_fill_box(&w->voxels, 31, 1, 41, 41, 5, 48, VOX_AIR);
    voxel_fill_box(&w->voxels, 42, 1, 43, 42, 3, 46, VOX_AIR);
    voxel_fill_box(&w->voxels, 80, 1, 40, 92, 6, 49, VOX_CRYPT_STONE);
    voxel_fill_box(&w->voxels, 81, 1, 41, 91, 5, 48, VOX_AIR);
    voxel_fill_box(&w->voxels, 80, 1, 43, 80, 3, 46, VOX_AIR);
    /* Secret #3: Backstage Freakshow Cache behind cracked tent wall */
    voxel_fill_box(&w->voxels, 87, 1, 33, 95, 5, 39, VOX_RED_CARNIVAL);
    voxel_fill_box(&w->voxels, 88, 1, 34, 94, 4, 38, VOX_AIR);
    voxel_fill_box(&w->voxels, 87, 1, 35, 87, 3, 37, VOX_CRACKED);
    world_add_secret(w, vec3_make(91.0f, 1.5f, 36.0f), 3.0f, "CARNIVAL BACKSTAGE");
    /* Door #1: Red Skull Big Top Gate at z=32 */
    world_add_door(w, 58, 1, 32, 62, 4, 32, KEY_RED, VOX_DOOR_RED);

    /* ---- BEAT 4: THE CRIMSON BIG TOP & GARGOYLE MATRIARCH (x=34..86, z=6..31) ---- */
    voxel_fill_box(&w->voxels, 34, 1, 6, 86, 13, 6, VOX_RED_CARNIVAL);
    voxel_fill_box(&w->voxels, 34, 1, 6, 34, 13, 32, VOX_RED_CARNIVAL);
    voxel_fill_box(&w->voxels, 86, 1, 6, 86, 13, 32, VOX_RED_CARNIVAL);
    voxel_fill_box(&w->voxels, 44, 0, 12, 76, 0, 26, VOX_CARPET);
    /* Crimson fire braziers */
    voxel_set(&w->voxels, 42, 2, 12, VOX_FIRE);
    voxel_set(&w->voxels, 78, 2, 12, VOX_FIRE);
    voxel_set(&w->voxels, 42, 2, 26, VOX_FIRE);
    voxel_set(&w->voxels, 78, 2, 26, VOX_FIRE);
    /* Switch #1: Crimson Chalice Exit Pedestal */
    voxel_fill_box(&w->voxels, 59, 1, 8, 61, 1, 10, VOX_GOLD);
    world_add_switch(w, SW_EXIT_SHARD, vec3_make(60.5f, 2.0f, 9.5f), -1, 5, "CRIMSON CHALICE ALTAR");

    w->return_portal_pos = vec3_make(24.5f, 1.0f, 114.5f);

    /* Explosive powder barrels */
    world_add_barrel(w, vec3_make(36.0f, 1.0f, 98.0f));
    world_add_barrel(w, vec3_make(40.0f, 1.0f, 98.0f));
    world_add_barrel(w, vec3_make(54.0f, 1.0f, 46.0f));
    world_add_barrel(w, vec3_make(52.0f, 1.0f, 18.0f));
    world_add_barrel(w, vec3_make(68.0f, 1.0f, 18.0f));

    /* ---- ENEMIES ---- */
    world_add_enemy(w, ENEMY_CULTIST,        vec3_make(34.0f, 1.0f, 102.0f));
    world_add_enemy(w, ENEMY_GHOUL,          vec3_make(44.0f, 1.0f, 96.0f));
    world_add_enemy(w, ENEMY_CULTIST,        vec3_make(48.0f, 2.0f, 70.0f));
    world_add_enemy(w, ENEMY_GHOUL,          vec3_make(58.0f, 2.0f, 70.0f));
    world_add_enemy(w, ENEMY_CULTIST,        vec3_make(56.0f, 1.0f, 44.0f));
    world_add_enemy(w, ENEMY_GARGOYLE,       vec3_make(66.0f, 4.5f, 42.0f));
    world_add_enemy(w, ENEMY_GARGOYLE,       vec3_make(84.0f, 4.5f, 44.0f));
    world_add_enemy(w, ENEMY_BOSS_MATRIARCH, vec3_make(60.0f, 1.0f, 16.0f)); /* Index 7: Matriarch Boss! */

    /* ---- PICKUPS ---- */
    world_add_weapon_pickup(w, WEAP_AEROSOL, vec3_make(25.5f, 1.2f, 98.5f)); /* Ticket Booth */
    world_add_pickup(w, PICKUP_ARMOR,        vec3_make(51.5f, 1.2f, 104.5f));/* Secret #1 */
    world_add_pickup(w, PICKUP_ROCKETS,      vec3_make(25.0f, 2.2f, 70.0f)); /* Secret #2 */
    world_add_weapon_pickup(w, WEAP_VOODOO,  vec3_make(36.0f, 1.2f, 44.5f)); /* Fortune Tent */
    world_add_pickup(w, PICKUP_KEY_RED,      vec3_make(86.0f, 1.2f, 44.5f)); /* Funhouse Vault */
    world_add_pickup(w, PICKUP_MEDKIT,       vec3_make(91.0f, 1.2f, 36.0f)); /* Secret #3 */
    world_add_pickup(w, PICKUP_CELLS,        vec3_make(48.0f, 1.2f, 24.0f));
    world_add_pickup(w, PICKUP_MEDKIT,       vec3_make(72.0f, 1.2f, 24.0f));

    world_spawn_players(w, vec3_make(38.0f, 1.0f, 113.0f), 0.0f, init_owned, WEAP_FLAREGUN);
    world_set_objective(w, vec3_make(38.0f, 2.0f, 88.0f), "BLAST CRACKED CRYPT WALL WITH TNT (KEY 4) - BOARD GHOST TRAIN");
    world_set_banner(w, "DAY 5: CARNIVAL OF THE DAMNED - USE ALT-FIRE (RMB) ON EVERY WEAPON!", 6.0f);
}

/* ============================================================================
 * DAY 6 (Dec 6 — ACT I CLIMAX) — DOOM HOMAGE: "E1M24: BRIMSTONE HANGAR"
 * ============================================================================ */
void build_day06_doom(GameWorld *w) {
    w->scene = SCENE_DAY06;
    w->active_day = 6;
    world_clear_entities(w);
    voxel_world_clear(&w->voxels);

    w->boss_phase = 1;
    w->boss_defeated = false;
    w->checkpoint_stage = 0;

    static const WeaponId kLoadout[5] = {
        WEAP_CHAINSAW, WEAP_SUPER_SHOTGUN, WEAP_CHAINGUN, WEAP_ROCKET, WEAP_BFG
    };
    uint32_t init_owned = (1u << WEAP_CHAINSAW) | (1u << WEAP_SUPER_SHOTGUN);
    world_set_loadout(w, kLoadout, 5, init_owned);

    /* UAC Techbase floor & outer perimeter */
    voxel_fill_box(&w->voxels, 12, 0, 4, 116, 0, 120, VOX_TECH_WALL);

    /* ---- BEAT 1: CENTRAL E1M24 HANGAR & NUKAGE TRENCH (x=40..84, z=70..118) ---- */
    voxel_fill_box(&w->voxels, 40, 1, 40, 84, 9, 118, VOX_TECH_WALL);
    voxel_fill_box(&w->voxels, 41, 1, 41, 83, 8, 117, VOX_AIR);
    /* Glowing Green Radioactive Nukage Pit on West Side (x=42..52, z=70..92) */
    voxel_fill_box(&w->voxels, 42, 0, 70, 52, 0, 92, VOX_SLIME);
    voxel_fill_box(&w->voxels, 53, 1, 70, 53, 2, 92, VOX_HAZARD);
    voxel_fill_box(&w->voxels, 53, 1, 80, 53, 2, 84, VOX_AIR); /* Entry steps into slime */

    /* Secret #1: Berserk Room off Slime Trench (x=32..39, z=76..84) */
    voxel_fill_box(&w->voxels, 32, 1, 76, 40, 6, 84, VOX_TECH_WALL);
    voxel_fill_box(&w->voxels, 33, 1, 77, 40, 5, 83, VOX_AIR);
    world_add_secret(w, vec3_make(36.0f, 1.5f, 80.0f), 3.0f, "NUKAGE BERSERK ALCOVE");

    /* ---- BEAT 2: BLUE KEYCARD WEST WING & INFIGHTING ARENA (x=14..40, z=42..70) ---- */
    voxel_fill_box(&w->voxels, 14, 1, 42, 40, 8, 70, VOX_TECH_WALL);
    voxel_fill_box(&w->voxels, 15, 1, 43, 39, 7, 69, VOX_AIR);
    /* Door #0: Blue Keycard Door (x=40, z=61..65) */
    world_add_door(w, 40, 1, 61, 40, 4, 65, KEY_BLUE, VOX_DOOR_BLUE);
    /* Secret #2: Cracked Computer Panel Armory */
    voxel_fill_box(&w->voxels, 15, 1, 63, 21, 5, 69, VOX_METAL);
    voxel_fill_box(&w->voxels, 16, 1, 64, 20, 4, 68, VOX_AIR);
    voxel_fill_box(&w->voxels, 21, 1, 65, 21, 3, 67, VOX_CRACKED);
    world_add_secret(w, vec3_make(18.0f, 1.5f, 66.0f), 2.8f, "UAC SECRET ARMORY");

    /* ---- BEAT 3: YELLOW KEYCARD EAST WING & YULE-BFG VAULT (x=84..114, z=42..72) ---- */
    voxel_fill_box(&w->voxels, 84, 1, 42, 114, 8, 72, VOX_TECH_WALL);
    voxel_fill_box(&w->voxels, 85, 1, 43, 113, 7, 71, VOX_AIR);
    /* Door #1: Yellow Keycard Door (x=84, z=61..65) */
    world_add_door(w, 84, 1, 61, 84, 4, 65, KEY_YELLOW, VOX_DOOR_YELLOW);
    /* Secret #3: BFG Cell Vault behind reinforced glass */
    voxel_fill_box(&w->voxels, 104, 1, 63, 113, 5, 71, VOX_METAL);
    voxel_fill_box(&w->voxels, 105, 1, 64, 112, 4, 70, VOX_AIR);
    voxel_fill_box(&w->voxels, 104, 1, 65, 104, 3, 68, VOX_GLASS);
    world_add_secret(w, vec3_make(108.5f, 1.5f, 67.0f), 3.0f, "BFG CELL VAULT");

    /* ---- BEAT 4: BRIMSTONE REACTOR & HORNED CYBER-TITAN (x=36..88, z=6..40) ---- */
    voxel_fill_box(&w->voxels, 36, 1, 6, 88, 13, 40, VOX_STONE);
    voxel_fill_box(&w->voxels, 37, 1, 7, 87, 12, 39, VOX_AIR);
    /* Door #2: Red Keycard Reactor Gate at x=60..64, z=40 */
    world_add_door(w, 60, 1, 40, 64, 4, 40, KEY_RED, VOX_DOOR_RED);
    /* Glowing Lava Reactor Moat + Gothic Marble Pillars */
    voxel_fill_box(&w->voxels, 38, 0, 8, 42, 0, 38, VOX_LAVA);
    voxel_fill_box(&w->voxels, 82, 0, 8, 86, 0, 38, VOX_LAVA);
    voxel_fill_box(&w->voxels, 50, 1, 24, 52, 6, 26, VOX_BRICK);
    voxel_fill_box(&w->voxels, 72, 1, 24, 74, 6, 26, VOX_BRICK);
    voxel_fill_box(&w->voxels, 50, 1, 14, 52, 6, 16, VOX_BRICK);
    voxel_fill_box(&w->voxels, 72, 1, 14, 74, 6, 16, VOX_BRICK);

    /* Switch #0: Hellfire Exit Shrine (Act I Climax!) */
    voxel_fill_box(&w->voxels, 61, 1, 8, 63, 1, 10, VOX_GOLD);
    world_add_switch(w, SW_EXIT_SHARD, vec3_make(62.5f, 2.0f, 9.5f), -1, 6, "HELLFIRE EXIT SWITCH");

    w->return_portal_pos = vec3_make(46.5f, 1.0f, 114.5f);

    /* Explosive Red Nukage Barrel Clusters (Chain-Reaction Showpiece!) */
    world_add_barrel(w, vec3_make(60.0f, 1.0f, 98.0f));
    world_add_barrel(w, vec3_make(62.2f, 1.0f, 98.0f));
    world_add_barrel(w, vec3_make(64.4f, 1.0f, 98.0f));
    world_add_barrel(w, vec3_make(62.0f, 1.0f, 95.8f));
    world_add_barrel(w, vec3_make(92.0f, 1.0f, 62.0f));
    world_add_barrel(w, vec3_make(94.2f, 1.0f, 62.0f));
    world_add_barrel(w, vec3_make(58.0f, 1.0f, 26.0f));
    world_add_barrel(w, vec3_make(66.0f, 1.0f, 26.0f));

    /* ---- ENEMIES (Imps, Pinkies, Cacodemons + Cyber-Titan Climax!) ---- */
    world_add_enemy(w, ENEMY_IMP,             vec3_make(58.0f, 1.0f, 96.0f));
    world_add_enemy(w, ENEMY_IMP,             vec3_make(66.0f, 1.0f, 96.0f));
    world_add_enemy(w, ENEMY_PINKY,           vec3_make(72.0f, 1.0f, 88.0f));
    world_add_enemy(w, ENEMY_IMP,             vec3_make(30.0f, 1.0f, 58.0f)); /* West Infighting Wing */
    world_add_enemy(w, ENEMY_PINKY,           vec3_make(26.0f, 1.0f, 54.0f));
    world_add_enemy(w, ENEMY_CACODEMON,       vec3_make(24.0f, 4.2f, 48.0f));
    world_add_enemy(w, ENEMY_IMP,             vec3_make(94.0f, 1.0f, 58.0f)); /* East BFG Wing */
    world_add_enemy(w, ENEMY_CACODEMON,       vec3_make(102.0f, 4.2f, 52.0f));
    world_add_enemy(w, ENEMY_PINKY,           vec3_make(98.0f, 1.0f, 48.0f));
    world_add_enemy(w, ENEMY_BOSS_CYBERTITAN, vec3_make(62.0f, 1.0f, 18.0f)); /* Index 9: Cyber-Titan Boss! */

    /* ---- PICKUPS ---- */
    world_add_pickup(w, PICKUP_RADSUIT,          vec3_make(55.0f, 1.2f, 92.0f)); /* Safe slime crossing! */
    world_add_weapon_pickup(w, WEAP_CHAINGUN,    vec3_make(76.0f, 1.2f, 98.0f));
    world_add_pickup(w, PICKUP_KEY_BLUE,         vec3_make(46.0f, 1.2f, 74.0f)); /* In Slime Trench */
    world_add_pickup(w, PICKUP_BERSERK,          vec3_make(36.0f, 1.2f, 80.0f)); /* Secret #1 */
    world_add_weapon_pickup(w, WEAP_ROCKET,      vec3_make(24.0f, 1.2f, 58.0f)); /* West Wing */
    world_add_pickup(w, PICKUP_KEY_YELLOW,       vec3_make(20.0f, 1.2f, 46.0f)); /* West Wing */
    world_add_pickup(w, PICKUP_ARMOR,            vec3_make(18.0f, 1.2f, 66.0f)); /* Secret #2 */
    world_add_pickup(w, PICKUP_ROCKETS,          vec3_make(19.5f, 1.2f, 66.0f));
    world_add_weapon_pickup(w, WEAP_BFG,         vec3_make(102.0f, 1.2f, 56.0f));/* East Vault Yule-BFG! */
    world_add_pickup(w, PICKUP_KEY_RED,          vec3_make(106.0f, 1.2f, 46.0f));/* East Vault */
    world_add_pickup(w, PICKUP_CELLS,            vec3_make(108.5f, 1.2f, 67.0f));/* Secret #3 */
    world_add_pickup(w, PICKUP_MEDKIT,           vec3_make(108.5f, 1.2f, 68.5f));
    world_add_pickup(w, PICKUP_MEDKIT,           vec3_make(46.0f, 1.2f, 32.0f));
    world_add_pickup(w, PICKUP_SHELLS,           vec3_make(78.0f, 1.2f, 32.0f));

    world_spawn_players(w, vec3_make(62.0f, 1.0f, 113.0f), 0.0f, init_owned, WEAP_SUPER_SHOTGUN);
    world_set_objective(w, vec3_make(46.0f, 1.5f, 74.0f), "GRAB RAD-SUIT - CROSS NUKAGE FOR BLUE KEYCARD");
    world_set_banner(w, "DAY 6: E1M24 BRIMSTONE HANGAR - ACT I CLIMAX! COLLECT ALL 3 KEYS!", 6.0f);
}

/* ============================================================================
 * ACT I SWITCH INTERACTIONS, LEVEL TICK HOOKS & BIOME LIGHTING
 * ============================================================================ */
void act1_handle_switch_use(GameWorld *w, Player *p, SwitchTrigger *sw) {
    if (!w || !p || !sw || !sw->active) return;

    if (sw->kind == SW_CISTERN_VALVE) {
        if (sw->activated) return;
        sw->activated = true;
        w->cistern_flooded = true;
        /* Raise Flooded Cistern water level to y=4 (surface y=5.0) so player swims to Upper Sanctuary! */
        voxel_fill_box(&w->voxels, 31, 1, 54, 55, 4, 79, VOX_WATER);
        voxel_fill_box(&w->voxels, 25, 1, 54, 30, 4, 62, VOX_WATER);
        if (sw->door_idx >= 0 && sw->door_idx < w->door_count) {
            world_open_door(w, &w->doors[sw->door_idx]);
        }
        w->checkpoint_pos = vec3_make(44.0f, 5.0f, 52.0f);
        w->checkpoint_stage = 2;
        p->score += 350;
        w->sound_events |= SFX_SWITCH;
        world_set_objective(w, vec3_make(44.0f, 5.5f, 44.0f), "SWIM ACROSS FLOODED CISTERN - SURVIVE BOULDER HALL");
        world_set_banner(w, "CISTERN FLOODED! SWIM NORTH TO UPPER SANCTUARY! [CHECKPOINT 2]", 5.5f);
    } else if (sw->kind == SW_DOOR_LEVER) {
        if (sw->activated) return;
        sw->activated = true;
        if (sw->door_idx >= 0 && sw->door_idx < w->door_count) {
            world_open_door(w, &w->doors[sw->door_idx]);
        }
        w->checkpoint_pos = vec3_make(40.0f, 1.0f, 84.0f);
        w->checkpoint_stage = sw->tag;
        p->score += 250;
        w->sound_events |= SFX_SWITCH;
        world_set_objective(w, vec3_make(28.0f, 3.5f, 54.0f), "ENTER FLOODED TRENCH - GRAB THUNDERBOLT & BLUE RUNE");
        world_set_banner(w, "BRASS PORTCULLIS RAISED! ENTER THE FLOODED TRENCH! [CHECKPOINT 1]", 5.0f);
    } else if (sw->kind == SW_CS_BUY_CRATE) {
        /* Smart Counter-Strike Buy-Zone progression */
        if (!p->has_defuse_kit && p->cash >= 400) {
            p->cash -= 400;
            p->has_defuse_kit = true;
            w->sound_events |= SFX_PICKUP;
            world_set_banner(w, "BOUGHT DEFUSE KIT (-$400)! C4 DEFUSAL SPEED DOUBLED!", 4.0f);
        } else if (!(p->owned_weapons & (1u << WEAP_DEAGLE)) && p->cash >= 700) {
            p->cash -= 700;
            p->owned_weapons |= (1u << WEAP_DEAGLE);
            p->ammo_bullets += 35;
            p->weapon = WEAP_DEAGLE;
            w->sound_events |= SFX_PICKUP;
            world_set_banner(w, "BOUGHT HEAVY DEAGLE (-$700)! (KEY 2)", 4.0f);
        } else if (p->armor < 80 && p->cash >= 650) {
            p->cash -= 650;
            p->armor = 100;
            w->sound_events |= SFX_PICKUP;
            world_set_banner(w, "BOUGHT KEVLAR + HELMET (-$650)!", 3.5f);
        } else if (!(p->owned_weapons & (1u << WEAP_CARBINE)) && p->cash >= 1500) {
            p->cash -= 1500;
            p->owned_weapons |= (1u << WEAP_CARBINE);
            p->ammo_bullets += 90;
            p->weapon = WEAP_CARBINE;
            w->sound_events |= SFX_PICKUP;
            world_set_banner(w, "BOUGHT BURST CARBINE (-$1500)! STAND STILL FOR LASER AIM!", 4.5f);
        } else if (!(p->owned_weapons & (1u << WEAP_SNIPER)) && p->cash >= 2200) {
            p->cash -= 2200;
            p->owned_weapons |= (1u << WEAP_SNIPER);
            p->ammo_bullets += 20;
            p->weapon = WEAP_SNIPER;
            w->sound_events |= SFX_PICKUP;
            world_set_banner(w, "BOUGHT BOLT SNIPER (-$2200)! RMB TOGGLES 2.5X SCOPE!", 4.5f);
        } else if (p->cash >= 300) {
            p->cash -= 300;
            p->ammo_bullets += 40;
            p->ammo_utility += 3;
            w->sound_events |= SFX_PICKUP;
            world_set_banner(w, "RESUPPLIED AMMO + FLASH/SMOKE GRENADES (-$300)!", 3.5f);
        } else {
            char mb[96];
            snprintf(mb, sizeof(mb), "INSUFFICIENT FUNDS ($%d) - ELIMINATE SYNDICATE TROOPERS!", p->cash);
            world_set_banner(w, mb, 3.0f);
        }
    } else if (sw->kind == SW_CS_C4_BOMB) {
        if (sw->activated) return;
        /* Pressing E advances or completes C4 defusal cleanly (2x faster with Defuse Kit!) */
        float step = p->has_defuse_kit ? 0.52f : 0.28f;
        sw->progress += step;
        p->defuse_progress = sw->progress;
        w->sound_events |= SFX_SWITCH;
        if (sw->progress >= 1.0f) {
            sw->activated = true;
            sw->progress = 1.0f;
            p->defuse_progress = 0.0f;
            w->bombs_defused++;
            p->cash += 1200;
            p->score += 400;
            voxel_set(&w->voxels, (int)floorf(sw->pos.x), 1, (int)floorf(sw->pos.z), VOX_NEON_CYAN);
            if (sw->door_idx >= 0 && sw->door_idx < w->door_count) {
                world_open_door(w, &w->doors[sw->door_idx]);
            }
            if (w->bombs_defused == 1) {
                w->checkpoint_pos = vec3_make(54.0f, 5.0f, 60.0f);
                w->checkpoint_stage = 1;
                world_set_objective(w, vec3_make(92.5f, 1.5f, 24.5f), "CROSS CATWALK -> LONG BAZAAR -> DEFUSE BOMBSITE B");
                world_set_banner(w, "BOMBSITE A DEFUSED (+$1200)! CATWALK GATE OPEN! [CHECKPOINT 1]", 5.5f);
            } else {
                w->checkpoint_pos = vec3_make(92.0f, 1.0f, 24.0f);
                w->checkpoint_stage = 2;
                world_set_objective(w, vec3_make(92.5f, 2.0f, 16.5f), "ALL C4 DEFUSED - CLAIM DEFUSAL PLIERS SHARD (E)!");
                world_set_banner(w, "COUNTER-TERRORISTS WIN! CLAIM DEFUSAL PLIERS SHARD!", 6.0f);
            }
        } else {
            char db[96];
            snprintf(db, sizeof(db), "DEFUSING %s... %d%% (TAP/HOLD E%s)",
                     sw->label, (int)(sw->progress * 100.0f), p->has_defuse_kit ? " - KIT 2X" : "");
            world_set_banner(w, db, 2.0f);
        }
    } else if (sw->kind == SW_GHOST_TRAIN) {
        if (sw->activated) return;
        sw->activated = true;
        w->train_activated = true;
        if (sw->door_idx >= 0 && sw->door_idx < w->door_count) {
            world_open_door(w, &w->doors[sw->door_idx]);
        }
        world_spawn_explosion(w, vec3_make(52.0f, 2.5f, 56.0f), 3.0f, 1.0f, 0.35f, 0.15f);
        w->checkpoint_pos = vec3_make(52.0f, 1.0f, 58.0f);
        w->checkpoint_stage = 2;
        p->score += 350;
        w->sound_events |= SFX_EXPLODE;
        world_set_objective(w, vec3_make(86.0f, 1.5f, 44.5f), "ENTER CARNIVAL MIDWAY - GRAB VOODOO DOLL & RED SKULL KEY");
        world_set_banner(w, "GHOST TRAIN RAMMED THE CARNIVAL GATE! [CHECKPOINT 2]", 5.5f);
    } else if (sw->kind == SW_EXIT_SHARD) {
        if (w->scene == SCENE_DAY03) {
            if (w->bombs_defused < 2) {
                world_set_banner(w, "DEFUSE BOTH BOMBSITES A & B FIRST!", 3.5f);
                return;
            }
            world_award_day_shard(w, 3, p, "DAY 3 COMPLETE! DEFUSAL PLIERS SHARD LIT ON GRAND TREE!");
        } else {
            if (!w->boss_defeated) {
                world_set_banner(w, "DEFEAT THE REALM BOSS GUARDIAN FIRST!", 3.5f);
                return;
            }
            static const char *kWinMsgs[7] = {
                "", "",
                "DAY 2 COMPLETE! SCION PRISM SHARD LIT ON GRAND TREE!",
                "DAY 3 COMPLETE! DEFUSAL PLIERS SHARD LIT ON GRAND TREE!",
                "DAY 4 COMPLETE! SLIPGATE RUNE SHARD LIT ON GRAND TREE!",
                "DAY 5 COMPLETE! CRIMSON CHALICE SHARD LIT ON GRAND TREE!",
                "ACT I COMPLETE! HELLFIRE KEY SHARD LIT ON GRAND TREE!"
            };
            int day = sw->tag;
            if (day >= 2 && day <= 6) {
                world_award_day_shard(w, day, p, kWinMsgs[day]);
            }
        }
    }
}

void act1_step_level(GameWorld *w, float dt) {
    /* Rolling Boulder traps (Day 2 Tomb Raider) */
    for (int i = 0; i < w->boulder_count; ++i) {
        BoulderTrap *b = &w->boulders[i];
        if (!b->active) continue;
        if (!b->rolling) {
            for (int pi = 0; pi < w->player_count; ++pi) {
                if (w->players[pi].active && w->players[pi].hp > 0 &&
                    vec3_dist(w->players[pi].pos, b->trigger_pos) < b->trigger_radius) {
                    b->rolling = true;
                    w->sound_events |= SFX_EXPLODE;
                    world_set_banner(w, "WATCH OUT! ROLLING TEMPLE BOULDER! DUCK INTO THE EAST ALCOVE!", 4.5f);
                    break;
                }
            }
        } else {
            b->pos = vec3_add(b->pos, vec3_scale(b->vel, dt));
            for (int pi = 0; pi < w->player_count; ++pi) {
                Player *p = &w->players[pi];
                if (!p->active || p->hp <= 0) continue;
                if (vec3_dist(vec3_add(p->pos, vec3_make(0, 0.9f, 0)), b->pos) < 1.45f) {
                    p->damage_flash = 0.4f;
                    p->vel.z -= 6.0f;
                    p->hp -= 25;
                    if (p->hp < 1) p->hp = 1; /* Dramatic knockback without frustrating instant-death */
                    w->sound_events |= SFX_HURT;
                }
            }
            if (b->pos.z <= b->stop_z) {
                b->active = false;
                world_spawn_explosion(w, b->pos, 2.2f, 0.85f, 0.70f, 0.45f);
            }
        }
    }

    /* Update objective pointer cleanly when Day 2 Cistern valve / Day 5 Cracked wall breached */
    if (w->scene == SCENE_DAY05 && w->checkpoint_stage == 0 &&
        voxel_get(&w->voxels, 38, 2, 88) == VOX_AIR) {
        w->checkpoint_stage = 1;
        w->checkpoint_pos = vec3_make(38.0f, 1.0f, 84.0f);
        world_set_objective(w, vec3_make(67.5f, 2.5f, 70.0f), "BOARD GHOST TRAIN -> FLIP LOCOMOTIVE THROTTLE (E)");
        world_set_banner(w, "CRYPT WALL BREACHED! BOARD THE GHOST TRAIN! [CHECKPOINT 1]", 5.0f);
    }
}

void act1_rebuild_lights(GameWorld *w, float flick) {
    if (w->scene == SCENE_DAY02) {
        world_add_light(w, vec3_make(37.5f, 3.2f, 108.0f), 1.0f, 0.62f, 0.22f, 2.4f * flick, 22.0f);
        world_add_light(w, vec3_make(51.5f, 3.2f, 108.0f), 1.0f, 0.62f, 0.22f, 2.4f * flick, 22.0f);
        world_add_light(w, vec3_make(44.0f, 3.5f, 94.0f),  1.0f, 0.84f, 0.35f, 1.8f, 18.0f);
        world_add_light(w, vec3_make(44.0f, 8.0f, 66.0f),  0.35f, 0.85f, 1.0f, 2.2f, 26.0f);
        world_add_light(w, vec3_make(44.0f, 11.0f, 16.0f), 1.0f, 0.90f, 0.62f, 2.8f, 36.0f);
    } else if (w->scene == SCENE_DAY03) {
        world_add_light(w, vec3_make(60.0f, 10.0f, 106.0f), 1.0f, 0.92f, 0.72f, 2.2f, 32.0f);
        world_add_light(w, vec3_make(28.0f, 7.5f, 65.0f),   1.0f, 0.45f, 0.22f, 2.0f * flick, 24.0f);
        world_add_light(w, vec3_make(60.0f, 10.0f, 58.0f),  1.0f, 0.90f, 0.70f, 2.2f, 30.0f);
        world_add_light(w, vec3_make(92.0f, 8.0f, 24.0f),   1.0f, 0.45f, 0.22f, 2.2f * flick, 26.0f);
    } else if (w->scene == SCENE_DAY04) {
        world_add_light(w, vec3_make(40.0f, 4.5f, 98.0f),  1.0f, 0.42f, 0.14f, 2.6f * flick, 24.0f);
        world_add_light(w, vec3_make(44.0f, 5.0f, 66.0f),  0.30f, 0.76f, 1.0f, 2.0f, 26.0f);
        world_add_light(w, vec3_make(90.0f, 6.0f, 64.0f),  0.28f, 0.52f, 1.0f, 2.6f, 28.0f);
        world_add_light(w, vec3_make(64.0f, 10.0f, 17.0f), 0.85f, 0.40f, 1.0f, 2.6f * flick, 34.0f);
    } else if (w->scene == SCENE_DAY05) {
        world_add_light(w, vec3_make(38.0f, 4.5f, 104.0f), 1.0f, 0.35f, 0.18f, 2.2f * flick, 22.0f);
        world_add_light(w, vec3_make(46.0f, 5.0f, 70.0f),  0.85f, 0.32f, 0.22f, 2.0f * flick, 26.0f);
        world_add_light(w, vec3_make(58.0f, 5.5f, 44.0f),  1.0f, 0.22f, 0.35f, 2.2f * flick, 28.0f);
        world_add_light(w, vec3_make(60.0f, 6.0f, 16.0f),  1.0f, 0.18f, 0.16f, 2.8f * flick, 34.0f);
    } else if (w->scene == SCENE_DAY06) {
        float pulse = 0.65f + 0.35f * sinf(w->time_sec * 5.5f);
        world_add_light(w, vec3_make(62.0f, 6.0f, 98.0f),  0.85f, 0.88f, 0.96f, 2.0f * pulse, 26.0f);
        world_add_light(w, vec3_make(46.0f, 3.0f, 80.0f),  0.28f, 1.0f, 0.26f,  2.2f, 22.0f);
        world_add_light(w, vec3_make(26.0f, 5.5f, 56.0f),  0.30f, 0.55f, 1.0f,  2.0f, 24.0f);
        world_add_light(w, vec3_make(98.0f, 5.5f, 56.0f),  1.0f, 0.86f, 0.22f,  2.0f, 24.0f);
        world_add_light(w, vec3_make(62.0f, 7.0f, 18.0f),  1.0f, 0.26f, 0.12f,  3.0f * flick, 36.0f);
    }
}
