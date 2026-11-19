#ifndef SWIETA_TOUCH_H
#define SWIETA_TOUCH_H

#include <stdbool.h>
#include <stdint.h>

#define TOUCH_MAX_FINGERS 8

typedef enum {
    TOUCH_BTN_FIRE   = 1u << 0,
    TOUCH_BTN_ALT    = 1u << 1,
    TOUCH_BTN_JUMP   = 1u << 2,
    TOUCH_BTN_USE    = 1u << 3,
    TOUCH_BTN_WEAPON = 1u << 4,
    TOUCH_BTN_HOLO   = 1u << 5
} TouchButtonBits;

typedef struct {
    float cx;
    float cy;
    float radius;
    uint32_t bit;
    const char *label;
} TouchButtonRect;

typedef struct {
    bool active;
    int64_t id;
    float start_x;
    float start_y;
    float x;
    float y;
    int role; /* 0 = none, 1 = move stick, 2 = look pad, 3 = action button */
    uint32_t button_bit;
} TouchFinger;

typedef struct {
    bool enabled;
    TouchFinger fingers[TOUCH_MAX_FINGERS];
    float move_x;
    float move_y;
    bool sprint;
    float look_dx;
    float look_dy;
    uint32_t buttons;
    float stick_origin_x;
    float stick_origin_y;
    bool stick_active;
} TouchController;

void touch_init(TouchController *tc);
void touch_set_enabled(TouchController *tc, bool enabled);
void touch_on_down(TouchController *tc, int64_t finger_id, float nx, float ny);
void touch_on_move(TouchController *tc, int64_t finger_id, float nx, float ny);
void touch_on_up(TouchController *tc, int64_t finger_id);
void touch_consume_look(TouchController *tc, float *out_dx, float *out_dy);

const TouchButtonRect *touch_get_layout(int *out_count);

#endif /* SWIETA_TOUCH_H */
