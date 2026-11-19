#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "audio/retro_audio.h"
#include "core/calendar.h"
#include "net/net_proto.h"
#include "platform/touch.h"
#include "render/gl_renderer.h"
#include "sim/world.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>

EM_JS(void, swieta_js_send_packet, (const uint8_t *ptr, int len, int reliable), {
    if (window.SwietaNet && window.SwietaNet.sendPacket) {
        const bytes = HEAPU8.slice(ptr, ptr + len);
        window.SwietaNet.sendPacket(bytes, reliable !== 0);
    }
});

EM_JS(int, swieta_js_is_xr_active, (void), {
    return (window.SwietaXR && window.SwietaXR.active) ? 1 : 0;
});

EM_JS(void, swieta_js_save_progress, (const char *str), {
    try { localStorage.setItem("swieta2026_progress", UTF8ToString(str)); } catch (e) {}
});

EM_JS(int, swieta_js_load_progress, (char *out_buf, int max_len), {
    try {
        const val = localStorage.getItem("swieta2026_progress");
        if (!val) return 0;
        stringToUTF8(val, out_buf, max_len);
        return 1;
    } catch (e) { return 0; }
});
#else
#define EMSCRIPTEN_KEEPALIVE
static void swieta_js_send_packet(const uint8_t *ptr, int len, int reliable) {
    (void)ptr; (void)len; (void)reliable;
}
static int swieta_js_is_xr_active(void) { return 0; }
#endif

typedef struct {
    SDL_Window *window;
    SDL_GLContext gl_ctx;
    SDL_AudioStream *audio_stream;
    SDL_Gamepad *gamepads[WORLD_MAX_PLAYERS];

    GlRenderer renderer;
    GameWorld world;
    RetroAudioSynth audio;
    TouchController touch;

    uint64_t last_ticks_ns;
    float accum_sec;
    bool mouse_captured;
    float mouse_dx;
    float mouse_dy;
    bool mouse_left;
    bool mouse_right;
    int pending_weapon;

    /* Network state: 0=offline, 1=host, 2=client */
    int net_role;
    int net_local_slot;
    uint32_t net_seq;
    float net_send_accum;
    PlayerInput remote_inputs[WORLD_MAX_PLAYERS];
    uint32_t last_saved_shards;
} AppState;

static AppState *g_app = NULL;

static SwietaDateTime get_current_datetime(void) {
    SwietaDateTime dt = {2026, 12, 1, 0, 0, 0};
    SDL_Time ticks = 0;
    SDL_DateTime sdl_dt;
    if (SDL_GetCurrentTime(&ticks) && SDL_TimeToDateTime(ticks, &sdl_dt, true)) {
        dt.year   = sdl_dt.year;
        dt.month  = sdl_dt.month;
        dt.day    = sdl_dt.day;
        dt.hour   = sdl_dt.hour;
        dt.minute = sdl_dt.minute;
        dt.second = sdl_dt.second;
    }
    return dt;
}

static void save_progress(AppState *app) {
    char code[32];
    if (!swieta_progress_encode(app->world.shard_mask, code, sizeof(code))) return;
#ifdef __EMSCRIPTEN__
    swieta_js_save_progress(code);
#else
    char *pref = SDL_GetPrefPath("stbange", "swieta2026");
    if (pref) {
        char path[512];
        snprintf(path, sizeof(path), "%sprogress.txt", pref);
        FILE *f = fopen(path, "wb");
        if (f) {
            fwrite(code, 1, strlen(code), f);
            fclose(f);
        }
        SDL_free(pref);
    }
#endif
    app->last_saved_shards = app->world.shard_mask;
}

static void load_progress(AppState *app) {
    char buf[32] = {0};
    bool ok = false;
#ifdef __EMSCRIPTEN__
    ok = swieta_js_load_progress(buf, (int)sizeof(buf)) != 0;
#else
    char *pref = SDL_GetPrefPath("stbange", "swieta2026");
    if (pref) {
        char path[512];
        snprintf(path, sizeof(path), "%sprogress.txt", pref);
        FILE *f = fopen(path, "rb");
        if (f) {
            size_t n = fread(buf, 1, sizeof(buf) - 1, f);
            buf[n] = '\0';
            fclose(f);
            ok = n > 0;
        }
        SDL_free(pref);
    }
#endif
    uint32_t mask = 0;
    if (ok && swieta_progress_decode(buf, &mask)) {
        app->world.shard_mask = mask;
        app->last_saved_shards = mask;
    }
}

static void refresh_gamepads(AppState *app) {
    int count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    for (int i = 0; i < WORLD_MAX_PLAYERS; ++i) {
        if (app->gamepads[i]) {
            SDL_CloseGamepad(app->gamepads[i]);
            app->gamepads[i] = NULL;
        }
    }
    if (ids) {
        for (int i = 0; i < count && i < WORLD_MAX_PLAYERS; ++i) {
            app->gamepads[i] = SDL_OpenGamepad(ids[i]);
        }
        SDL_free(ids);
    }
}

static void gather_local_inputs(AppState *app, PlayerInput out_inputs[WORLD_MAX_PLAYERS]) {
    memset(out_inputs, 0, sizeof(PlayerInput) * WORLD_MAX_PLAYERS);
    for (int i = 0; i < WORLD_MAX_PLAYERS; ++i) out_inputs[i].select_weapon = -1;

    const bool *keys = SDL_GetKeyboardState(NULL);
    PlayerInput *p0 = &out_inputs[0];
    if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])    p0->move_y += 1.0f;
    if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN])  p0->move_y -= 1.0f;
    if (keys[SDL_SCANCODE_D]) p0->move_x += 1.0f;
    if (keys[SDL_SCANCODE_A]) p0->move_x -= 1.0f;
    if (keys[SDL_SCANCODE_LEFT]  || keys[SDL_SCANCODE_Q]) p0->look_dx -= 0.045f;
    if (keys[SDL_SCANCODE_RIGHT])                         p0->look_dx += 0.045f;

    p0->look_dx += app->mouse_dx;
    p0->look_dy += app->mouse_dy;
    app->mouse_dx = 0.0f;
    app->mouse_dy = 0.0f;

    if (app->mouse_left || keys[SDL_SCANCODE_F])  p0->buttons |= INP_FIRE;
    if (app->mouse_right || keys[SDL_SCANCODE_G]) p0->buttons |= INP_ALT;
    if (keys[SDL_SCANCODE_SPACE])                 p0->buttons |= INP_JUMP;
    if (keys[SDL_SCANCODE_E])                     p0->buttons |= INP_USE;
    if (keys[SDL_SCANCODE_V])                     p0->buttons |= INP_JETPACK;
    if (keys[SDL_SCANCODE_R])                     p0->buttons |= INP_HOLO;
    if (keys[SDL_SCANCODE_LSHIFT])                p0->buttons |= INP_SPRINT;
    if (app->pending_weapon >= 0) {
        p0->select_weapon = (int8_t)app->pending_weapon;
        app->pending_weapon = -1;
    }

    /* Merge adaptive mobile touch controls into P1 */
    if (app->touch.enabled) {
        p0->move_x = sw_clampf(p0->move_x + app->touch.move_x, -1.0f, 1.0f);
        p0->move_y = sw_clampf(p0->move_y + app->touch.move_y, -1.0f, 1.0f);
        if (app->touch.sprint) p0->buttons |= INP_SPRINT;
        float tdx = 0.0f, tdy = 0.0f;
        touch_consume_look(&app->touch, &tdx, &tdy);
        p0->look_dx += tdx;
        p0->look_dy += tdy;
        if (app->touch.buttons & TOUCH_BTN_FIRE)   p0->buttons |= INP_FIRE;
        if (app->touch.buttons & TOUCH_BTN_ALT)    p0->buttons |= INP_ALT;
        if (app->touch.buttons & TOUCH_BTN_JUMP)   p0->buttons |= INP_JUMP;
        if (app->touch.buttons & TOUCH_BTN_USE)    p0->buttons |= INP_USE;
        if (app->touch.buttons & TOUCH_BTN_WEAPON) p0->buttons |= INP_NEXT_WEAP;
        if (app->touch.buttons & TOUCH_BTN_HOLO)   p0->buttons |= INP_HOLO;
    }

    /* P2 local keyboard fallback for effortless 2P split-screen testing */
    if (app->world.player_count >= 2 && app->net_role == 0) {
        PlayerInput *p1 = &out_inputs[1];
        if (keys[SDL_SCANCODE_I]) p1->move_y += 1.0f;
        if (keys[SDL_SCANCODE_K]) p1->move_y -= 1.0f;
        if (keys[SDL_SCANCODE_L]) p1->move_x += 1.0f;
        if (keys[SDL_SCANCODE_J]) p1->move_x -= 1.0f;
        if (keys[SDL_SCANCODE_U]) p1->look_dx -= 0.045f;
        if (keys[SDL_SCANCODE_O]) p1->look_dx += 0.045f;
        if (keys[SDL_SCANCODE_RETURN]) p1->buttons |= INP_FIRE;
        if (keys[SDL_SCANCODE_RSHIFT]) p1->buttons |= INP_JUMP;
        if (keys[SDL_SCANCODE_P])      p1->buttons |= INP_USE;
    }

    /* SDL3 Gamepads for P1..P4 */
    for (int i = 0; i < app->world.player_count && i < WORLD_MAX_PLAYERS; ++i) {
        SDL_Gamepad *gp = app->gamepads[i];
        if (!gp) continue;
        float lx = (float)SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
        float ly = -(float)SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
        float rx = (float)SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_RIGHTX) / 32767.0f;
        float ry = (float)SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_RIGHTY) / 32767.0f;
        if (fabsf(lx) > 0.18f) out_inputs[i].move_x = sw_clampf(out_inputs[i].move_x + lx, -1.0f, 1.0f);
        if (fabsf(ly) > 0.18f) out_inputs[i].move_y = sw_clampf(out_inputs[i].move_y + ly, -1.0f, 1.0f);
        if (fabsf(rx) > 0.18f) out_inputs[i].look_dx += rx * 0.06f;
        if (fabsf(ry) > 0.18f) out_inputs[i].look_dy += ry * 0.04f;
        if (SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_SOUTH)) out_inputs[i].buttons |= INP_JUMP;
        if (SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_WEST))  out_inputs[i].buttons |= INP_USE;
        if (SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_NORTH)) out_inputs[i].buttons |= INP_NEXT_WEAP;
        if (SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_EAST))  out_inputs[i].buttons |= INP_ALT;
        if (SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 12000) out_inputs[i].buttons |= INP_FIRE;
        if (SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 12000)  out_inputs[i].buttons |= INP_HOLO;
    }
}

static void pump_simulation_and_net(AppState *app, float dt) {
    if (dt > 0.1f) dt = 0.1f;
    app->accum_sec += dt;
    app->net_send_accum += dt;

    while (app->accum_sec >= WORLD_FIXED_DT) {
        PlayerInput inputs[WORLD_MAX_PLAYERS];
        gather_local_inputs(app, inputs);

        if (app->net_role == 2) {
            /* Client: send input to host & predict locally */
            NetInputPacket pkt;
            pkt.player_slot = (uint8_t)app->net_local_slot;
            pkt.seq = ++app->net_seq;
            pkt.move_x = inputs[0].move_x;
            pkt.move_y = inputs[0].move_y;
            pkt.yaw = app->world.players[app->net_local_slot].yaw + inputs[0].look_dx;
            pkt.pitch = sw_clampf(app->world.players[app->net_local_slot].pitch - inputs[0].look_dy, -1.35f, 1.35f);
            pkt.buttons = inputs[0].buttons;
            pkt.weapon = (uint8_t)app->world.players[app->net_local_slot].weapon;
            uint8_t buf[NET_MAX_PACKET_SIZE];
            size_t n = net_encode_input(&pkt, buf, sizeof(buf));
            if (n > 0) swieta_js_send_packet(buf, (int)n, 0);

            PlayerInput client_in[WORLD_MAX_PLAYERS];
            memset(client_in, 0, sizeof(client_in));
            for (int i = 0; i < WORLD_MAX_PLAYERS; ++i) client_in[i].select_weapon = -1;
            client_in[app->net_local_slot] = inputs[0];
            world_step(&app->world, client_in, WORLD_FIXED_DT);
        } else {
            /* Offline or Host-Authoritative Listen-Server */
            if (app->net_role == 1) {
                for (int i = 1; i < app->world.player_count; ++i) {
                    inputs[i] = app->remote_inputs[i];
                    app->remote_inputs[i].look_dx = 0.0f;
                    app->remote_inputs[i].look_dy = 0.0f;
                }
            }
            world_step(&app->world, inputs, WORLD_FIXED_DT);
            if (app->net_role == 1 && app->net_send_accum >= 0.05f) {
                app->net_send_accum = 0.0f;
                NetSnapshotPacket snap;
                world_build_snapshot(&app->world, &snap);
                uint8_t buf[NET_MAX_PACKET_SIZE];
                size_t n = net_encode_snapshot(&snap, buf, sizeof(buf));
                if (n > 0) swieta_js_send_packet(buf, (int)n, 0);
            }
        }

        retro_audio_trigger_events(&app->audio, app->world.sound_events);
        app->accum_sec -= WORLD_FIXED_DT;
    }

    if (app->world.shard_mask != app->last_saved_shards) {
        save_progress(app);
    }

    /* Feed retro audio stream */
    if (app->audio_stream && SDL_GetAudioStreamQueued(app->audio_stream) < AUDIO_SAMPLE_RATE / 4) {
        float pcm[512];
        int shards = swieta_progress_count_shards(app->world.shard_mask);
        retro_audio_render_samples(&app->audio, app->world.scene, shards, pcm, 512);
        SDL_PutAudioStreamData(app->audio_stream, pcm, (int)sizeof(pcm));
    }
}

/* Emscripten / Web Bridge Exported C API */
EMSCRIPTEN_KEEPALIVE void swieta_set_split_screen(int count) {
    if (!g_app) return;
    world_set_player_count(&g_app->world, count);
}

EMSCRIPTEN_KEEPALIVE void swieta_set_unlock_all(int enabled) {
    if (!g_app) return;
    g_app->world.unlock_all = (enabled != 0);
}

EMSCRIPTEN_KEEPALIVE void swieta_set_retro_res(int w, int h) {
    if (!g_app) return;
    gl_renderer_set_retro_res(&g_app->renderer, w, h);
}

EMSCRIPTEN_KEEPALIVE void swieta_resize(int w, int h) {
    if (!g_app || !g_app->window || w <= 0 || h <= 0) return;
    SDL_SetWindowSize(g_app->window, w, h);
}

EMSCRIPTEN_KEEPALIVE void swieta_toggle_touch(int enabled) {
    if (!g_app) return;
    touch_set_enabled(&g_app->touch, enabled != 0);
}

EMSCRIPTEN_KEEPALIVE void swieta_jump_to_day(int day) {
    if (!g_app) return;
    if (day <= 0) {
        world_load_hub(&g_app->world);
    } else {
        world_load_day(&g_app->world, day);
    }
}

EMSCRIPTEN_KEEPALIVE void swieta_net_set_role(int role, int local_slot) {
    if (!g_app) return;
    g_app->net_role = role;
    g_app->net_local_slot = (local_slot >= 0 && local_slot < WORLD_MAX_PLAYERS) ? local_slot : 0;
    if (role > 0 && g_app->world.player_count < 2) {
        world_set_player_count(&g_app->world, 2);
    }
}

EMSCRIPTEN_KEEPALIVE void swieta_net_on_packet(const uint8_t *data, int len) {
    if (!g_app || !data || len <= 0) return;
    uint8_t type = net_peek_packet_type(data, (size_t)len);
    if (type == NET_PKT_INPUT && g_app->net_role == 1) {
        NetInputPacket pkt;
        if (net_decode_input(data, (size_t)len, &pkt) && pkt.player_slot < WORLD_MAX_PLAYERS) {
            int s = pkt.player_slot;
            if (s >= g_app->world.player_count) {
                world_set_player_count(&g_app->world, s + 1);
            }
            g_app->remote_inputs[s].move_x = pkt.move_x;
            g_app->remote_inputs[s].move_y = pkt.move_y;
            g_app->remote_inputs[s].buttons = pkt.buttons;
            g_app->remote_inputs[s].select_weapon = (int8_t)pkt.weapon;
            g_app->world.players[s].yaw = pkt.yaw;
            g_app->world.players[s].pitch = pkt.pitch;
        }
    } else if (type == NET_PKT_SNAPSHOT && g_app->net_role == 2) {
        NetSnapshotPacket snap;
        if (net_decode_snapshot(data, (size_t)len, &snap)) {
            world_apply_snapshot(&g_app->world, &snap, g_app->net_local_slot);
        }
    }
}

EMSCRIPTEN_KEEPALIVE void swieta_xr_begin_frame(float dt) {
    if (!g_app) return;
    pump_simulation_and_net(g_app, dt);
}

EMSCRIPTEN_KEEPALIVE void swieta_xr_render_eye(const float *proj16, const float *view16,
                                               int vx, int vy, int vw, int vh) {
    if (!g_app || !proj16 || !view16) return;
    Mat4 proj, view;
    memcpy(proj.m, proj16, sizeof(proj.m));
    memcpy(view.m, view16, sizeof(view.m));
    /* Translate XR view into player world position */
    Vec3 eye = player_eye_pos(&g_app->world.players[0]);
    Mat4 world_view = mat4_mul(view, mat4_translate(vec3_scale(eye, -1.0f)));
    GLint bound_fbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &bound_fbo);
    gl_renderer_render_xr_eye(&g_app->renderer, &g_app->world, &proj, &world_view,
                              (GLuint)bound_fbo, vx, vy, vw, vh);
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

#ifdef __EMSCRIPTEN__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    AppState *app = (AppState *)calloc(1, sizeof(AppState));
    if (!app) return SDL_APP_FAILURE;
    *appstate = app;
    g_app = app;
    app->pending_weapon = -1;

    app->window = SDL_CreateWindow("SWIETA 2026: The Grand Hall of 24 Portals",
                                   960, 540, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!app->window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    app->gl_ctx = SDL_GL_CreateContext(app->window);
    if (!app->gl_ctx) {
        SDL_Log("SDL_GL_CreateContext failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_GL_SetSwapInterval(1);

    if (!gl_renderer_init(&app->renderer, 640, 360)) {
        SDL_Log("gl_renderer_init failed");
        return SDL_APP_FAILURE;
    }

    /* Default to unlock_all=true during playtest development (can toggle with F1 or UI) */
    bool unlock_all = true;
    int start_day = 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--strict-calendar") == 0) unlock_all = false;
        if (strcmp(argv[i], "--unlock-all") == 0) unlock_all = true;
        if (strncmp(argv[i], "--day=", 6) == 0) start_day = atoi(argv[i] + 6);
    }

    world_init(&app->world, get_current_datetime(), unlock_all, 1);
    load_progress(app);
    if (start_day >= 1 && start_day <= SWIETA_PORTAL_COUNT) {
        world_load_day(&app->world, start_day);
    }
    touch_init(&app->touch);
    retro_audio_init(&app->audio);
    refresh_gamepads(app);

    SDL_AudioSpec spec;
    spec.format = SDL_AUDIO_F32;
    spec.channels = 1;
    spec.freq = AUDIO_SAMPLE_RATE;
    app->audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (app->audio_stream) {
        SDL_ResumeAudioStreamDevice(app->audio_stream);
    }

    app->last_ticks_ns = SDL_GetTicksNS();
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *ev) {
    AppState *app = (AppState *)appstate;
    if (!app || !ev) return SDL_APP_CONTINUE;

    switch (ev->type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;

        case SDL_EVENT_GAMEPAD_ADDED:
        case SDL_EVENT_GAMEPAD_REMOVED:
            refresh_gamepads(app);
            break;

        case SDL_EVENT_FINGER_DOWN:
            touch_on_down(&app->touch, (int64_t)ev->tfinger.fingerID, ev->tfinger.x, ev->tfinger.y);
            break;
        case SDL_EVENT_FINGER_MOTION:
            touch_on_move(&app->touch, (int64_t)ev->tfinger.fingerID, ev->tfinger.x, ev->tfinger.y);
            break;
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_CANCELED:
            touch_on_up(&app->touch, (int64_t)ev->tfinger.fingerID);
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (!app->touch.enabled) {
                if (!app->mouse_captured) {
                    SDL_SetWindowRelativeMouseMode(app->window, true);
                    app->mouse_captured = true;
                }
                if (ev->button.button == SDL_BUTTON_LEFT)  app->mouse_left = true;
                if (ev->button.button == SDL_BUTTON_RIGHT) app->mouse_right = true;
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (ev->button.button == SDL_BUTTON_LEFT)  app->mouse_left = false;
            if (ev->button.button == SDL_BUTTON_RIGHT) app->mouse_right = false;
            break;

        case SDL_EVENT_MOUSE_MOTION:
            if (app->mouse_captured) {
                app->mouse_dx += ev->motion.xrel * 0.0032f;
                app->mouse_dy += ev->motion.yrel * 0.0028f;
            }
            break;

        case SDL_EVENT_KEY_DOWN:
            if (ev->key.scancode == SDL_SCANCODE_ESCAPE) {
                if (app->mouse_captured) {
                    SDL_SetWindowRelativeMouseMode(app->window, false);
                    app->mouse_captured = false;
                }
            } else if (ev->key.scancode >= SDL_SCANCODE_1 && ev->key.scancode <= SDL_SCANCODE_5) {
                int slot = (int)(ev->key.scancode - SDL_SCANCODE_1);
                if (slot < app->world.level_weapon_count) {
                    WeaponId w_id = world_slot_weapon(&app->world, slot);
                    if ((app->world.players[0].owned_weapons & (1u << (unsigned)w_id)) != 0u) {
                        app->pending_weapon = (int)w_id;
                    }
                }
            } else if (ev->key.scancode == SDL_SCANCODE_TAB) {
                int cur_slot = 0;
                int cnt = app->world.level_weapon_count > 0 ? app->world.level_weapon_count : 1;
                for (int s = 0; s < cnt; ++s) {
                    if (app->world.level_weapons[s] == app->world.players[0].weapon) { cur_slot = s; break; }
                }
                for (int step = 1; step <= cnt; ++step) {
                    WeaponId cand = app->world.level_weapons[(cur_slot + step) % cnt];
                    if ((app->world.players[0].owned_weapons & (1u << (unsigned)cand)) != 0u) {
                        app->pending_weapon = (int)cand;
                        break;
                    }
                }
            } else if (ev->key.scancode == SDL_SCANCODE_H) {
                world_load_hub(&app->world);
            } else if (ev->key.scancode == SDL_SCANCODE_M) {
                app->audio.muted = !app->audio.muted;
            } else if (ev->key.scancode == SDL_SCANCODE_F1) {
                app->world.unlock_all = !app->world.unlock_all;
            } else if (ev->key.scancode == SDL_SCANCODE_F2) {
                int maxd = app->renderer.retro_w > app->renderer.retro_h ? app->renderer.retro_w : app->renderer.retro_h;
                if (maxd == 640) {
                    gl_renderer_set_retro_res(&app->renderer, 960, 540);
                } else if (maxd == 960) {
                    gl_renderer_set_retro_res(&app->renderer, 480, 270);
                } else if (maxd == 480) {
                    gl_renderer_set_retro_res(&app->renderer, 320, 240);
                } else {
                    gl_renderer_set_retro_res(&app->renderer, 640, 360);
                }
            } else if (ev->key.scancode == SDL_SCANCODE_F3) {
                int next = (app->world.player_count == 1) ? 2 : (app->world.player_count == 2 ? 4 : 1);
                world_set_player_count(&app->world, next);
            } else if (ev->key.scancode == SDL_SCANCODE_F4) {
                touch_set_enabled(&app->touch, !app->touch.enabled);
            }
            break;

        default:
            break;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    AppState *app = (AppState *)appstate;
    if (!app) return SDL_APP_FAILURE;

    if (swieta_js_is_xr_active()) {
        return SDL_APP_CONTINUE; /* WebXR XRSession RAF drives simulation & stereo rendering */
    }

    uint64_t now_ns = SDL_GetTicksNS();
    float dt = (float)(now_ns - app->last_ticks_ns) * 1e-9f;
    app->last_ticks_ns = now_ns;

    pump_simulation_and_net(app, dt);

    int w = 960, h = 540;
    SDL_GetWindowSizeInPixels(app->window, &w, &h);
    gl_renderer_render_frame(&app->renderer, &app->world, &app->touch, w, h);
    SDL_GL_SwapWindow(app->window);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    (void)result;
    AppState *app = (AppState *)appstate;
    if (!app) return;
    save_progress(app);
    for (int i = 0; i < WORLD_MAX_PLAYERS; ++i) {
        if (app->gamepads[i]) SDL_CloseGamepad(app->gamepads[i]);
    }
    if (app->audio_stream) SDL_DestroyAudioStream(app->audio_stream);
    gl_renderer_shutdown(&app->renderer);
    if (app->gl_ctx) SDL_GL_DestroyContext(app->gl_ctx);
    if (app->window) SDL_DestroyWindow(app->window);
    free(app);
    g_app = NULL;
}
