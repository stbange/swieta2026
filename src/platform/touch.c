#include "platform/touch.h"

#include "core/math3d.h"

#include <math.h>
#include <string.h>

static const TouchButtonRect kTouchButtons[] = {
    { 0.87f, 0.74f, 0.085f, TOUCH_BTN_FIRE,   "FIRE" },
    { 0.75f, 0.80f, 0.068f, TOUCH_BTN_ALT,    "DET"  },
    { 0.88f, 0.54f, 0.068f, TOUCH_BTN_JUMP,   "JUMP" },
    { 0.76f, 0.62f, 0.062f, TOUCH_BTN_USE,    "USE"  },
    { 0.66f, 0.84f, 0.056f, TOUCH_BTN_WEAPON, "WEAP" },
    { 0.66f, 0.68f, 0.056f, TOUCH_BTN_HOLO,   "HOLO" }
};

const TouchButtonRect *touch_get_layout(int *out_count) {
    if (out_count) {
        *out_count = (int)(sizeof(kTouchButtons) / sizeof(kTouchButtons[0]));
    }
    return kTouchButtons;
}

void touch_init(TouchController *tc) {
    if (!tc) return;
    memset(tc, 0, sizeof(*tc));
    tc->stick_origin_x = 0.16f;
    tc->stick_origin_y = 0.76f;
}

void touch_set_enabled(TouchController *tc, bool enabled) {
    if (!tc) return;
    tc->enabled = enabled;
    if (!enabled) {
        memset(tc->fingers, 0, sizeof(tc->fingers));
        tc->move_x = 0.0f;
        tc->move_y = 0.0f;
        tc->sprint = false;
        tc->look_dx = 0.0f;
        tc->look_dy = 0.0f;
        tc->buttons = 0u;
        tc->stick_active = false;
    }
}

static uint32_t hit_test_button(float nx, float ny) {
    int count = 0;
    const TouchButtonRect *btns = touch_get_layout(&count);
    for (int i = 0; i < count; ++i) {
        float dx = nx - btns[i].cx;
        float dy = (ny - btns[i].cy) * 0.65f; /* gentle aspect compensation */
        if (dx * dx + dy * dy <= btns[i].radius * btns[i].radius) {
            return btns[i].bit;
        }
    }
    return 0u;
}

static void recompute_buttons_and_stick(TouchController *tc) {
    tc->buttons = 0u;
    tc->stick_active = false;
    tc->move_x = 0.0f;
    tc->move_y = 0.0f;
    tc->sprint = false;

    for (int i = 0; i < TOUCH_MAX_FINGERS; ++i) {
        const TouchFinger *f = &tc->fingers[i];
        if (!f->active) continue;
        if (f->role == 3) {
            tc->buttons |= f->button_bit;
        } else if (f->role == 1) {
            tc->stick_active = true;
            tc->stick_origin_x = f->start_x;
            tc->stick_origin_y = f->start_y;
            float dx = (f->x - f->start_x) / 0.12f;
            float dy = (f->start_y - f->y) / 0.16f; /* up = +forward */
            float len = sqrtf(dx * dx + dy * dy);
            if (len < 0.12f) {
                tc->move_x = 0.0f;
                tc->move_y = 0.0f;
            } else {
                if (len > 1.0f) {
                    dx /= len;
                    dy /= len;
                }
                tc->move_x = dx;
                tc->move_y = dy;
                tc->sprint = (len >= 0.85f);
            }
        }
    }
}

void touch_on_down(TouchController *tc, int64_t finger_id, float nx, float ny) {
    if (!tc) return;
    tc->enabled = true;
    nx = sw_clampf(nx, 0.0f, 1.0f);
    ny = sw_clampf(ny, 0.0f, 1.0f);

    /* Find existing or free slot */
    int slot = -1;
    for (int i = 0; i < TOUCH_MAX_FINGERS; ++i) {
        if (tc->fingers[i].active && tc->fingers[i].id == finger_id) {
            slot = i;
            break;
        }
        if (!tc->fingers[i].active && slot < 0) {
            slot = i;
        }
    }
    if (slot < 0) return;

    TouchFinger *f = &tc->fingers[slot];
    f->active = true;
    f->id = finger_id;
    f->start_x = nx;
    f->start_y = ny;
    f->x = nx;
    f->y = ny;
    f->button_bit = 0u;

    uint32_t btn = hit_test_button(nx, ny);
    if (btn != 0u) {
        f->role = 3;
        f->button_bit = btn;
    } else if (nx < 0.42f && !tc->stick_active) {
        f->role = 1;
    } else {
        f->role = 2;
    }
    recompute_buttons_and_stick(tc);
}

void touch_on_move(TouchController *tc, int64_t finger_id, float nx, float ny) {
    if (!tc) return;
    nx = sw_clampf(nx, 0.0f, 1.0f);
    ny = sw_clampf(ny, 0.0f, 1.0f);

    for (int i = 0; i < TOUCH_MAX_FINGERS; ++i) {
        TouchFinger *f = &tc->fingers[i];
        if (!f->active || f->id != finger_id) continue;
        if (f->role == 2) {
            tc->look_dx += (nx - f->x) * 4.2f;
            tc->look_dy += (ny - f->y) * 2.8f;
        }
        f->x = nx;
        f->y = ny;
        break;
    }
    recompute_buttons_and_stick(tc);
}

void touch_on_up(TouchController *tc, int64_t finger_id) {
    if (!tc) return;
    for (int i = 0; i < TOUCH_MAX_FINGERS; ++i) {
        if (tc->fingers[i].active && tc->fingers[i].id == finger_id) {
            tc->fingers[i].active = false;
            tc->fingers[i].role = 0;
            tc->fingers[i].button_bit = 0u;
            break;
        }
    }
    recompute_buttons_and_stick(tc);
}

void touch_consume_look(TouchController *tc, float *out_dx, float *out_dy) {
    if (!tc) return;
    if (out_dx) *out_dx = tc->look_dx;
    if (out_dy) *out_dy = tc->look_dy;
    tc->look_dx = 0.0f;
    tc->look_dy = 0.0f;
}
