#include "core/math3d.h"

#include <math.h>
#include <string.h>

float sw_clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

float sw_lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

float sw_wrap_angle(float rad) {
    float two_pi = 2.0f * SW_PI;
    rad = fmodf(rad + SW_PI, two_pi);
    if (rad < 0.0f) {
        rad += two_pi;
    }
    return rad - SW_PI;
}

Vec3 vec3_make(float x, float y, float z) {
    Vec3 v = {x, y, z};
    return v;
}

Vec3 vec3_add(Vec3 a, Vec3 b) {
    return vec3_make(a.x + b.x, a.y + b.y, a.z + b.z);
}

Vec3 vec3_sub(Vec3 a, Vec3 b) {
    return vec3_make(a.x - b.x, a.y - b.y, a.z - b.z);
}

Vec3 vec3_scale(Vec3 v, float s) {
    return vec3_make(v.x * s, v.y * s, v.z * s);
}

float vec3_dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 vec3_cross(Vec3 a, Vec3 b) {
    return vec3_make(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

float vec3_len_sq(Vec3 v) {
    return vec3_dot(v, v);
}

float vec3_len(Vec3 v) {
    return sqrtf(vec3_len_sq(v));
}

Vec3 vec3_norm(Vec3 v) {
    float len = vec3_len(v);
    if (len <= 1e-6f) {
        return vec3_make(0.0f, 0.0f, 0.0f);
    }
    return vec3_scale(v, 1.0f / len);
}

float vec3_dist(Vec3 a, Vec3 b) {
    return vec3_len(vec3_sub(a, b));
}

Vec3 vec3_lerp(Vec3 a, Vec3 b, float t) {
    return vec3_make(
        sw_lerpf(a.x, b.x, t),
        sw_lerpf(a.y, b.y, t),
        sw_lerpf(a.z, b.z, t)
    );
}

Mat4 mat4_identity(void) {
    Mat4 r;
    memset(&r, 0, sizeof(r));
    r.m[0] = 1.0f;
    r.m[5] = 1.0f;
    r.m[10] = 1.0f;
    r.m[15] = 1.0f;
    return r;
}

Mat4 mat4_mul(Mat4 a, Mat4 b) {
    Mat4 r;
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            r.m[col * 4 + row] =
                a.m[0 * 4 + row] * b.m[col * 4 + 0] +
                a.m[1 * 4 + row] * b.m[col * 4 + 1] +
                a.m[2 * 4 + row] * b.m[col * 4 + 2] +
                a.m[3 * 4 + row] * b.m[col * 4 + 3];
        }
    }
    return r;
}

Vec4 mat4_mul_vec4(Mat4 m, Vec4 v) {
    Vec4 r;
    r.x = m.m[0] * v.x + m.m[4] * v.y + m.m[8]  * v.z + m.m[12] * v.w;
    r.y = m.m[1] * v.x + m.m[5] * v.y + m.m[9]  * v.z + m.m[13] * v.w;
    r.z = m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z + m.m[14] * v.w;
    r.w = m.m[3] * v.x + m.m[7] * v.y + m.m[11] * v.z + m.m[15] * v.w;
    return r;
}

Mat4 mat4_translate(Vec3 t) {
    Mat4 r = mat4_identity();
    r.m[12] = t.x;
    r.m[13] = t.y;
    r.m[14] = t.z;
    return r;
}

Mat4 mat4_scale(Vec3 s) {
    Mat4 r = mat4_identity();
    r.m[0] = s.x;
    r.m[5] = s.y;
    r.m[10] = s.z;
    return r;
}

Mat4 mat4_rotate_x(float rad) {
    Mat4 r = mat4_identity();
    float c = cosf(rad);
    float s = sinf(rad);
    r.m[5] = c;
    r.m[6] = s;
    r.m[9] = -s;
    r.m[10] = c;
    return r;
}

Mat4 mat4_rotate_y(float rad) {
    Mat4 r = mat4_identity();
    float c = cosf(rad);
    float s = sinf(rad);
    r.m[0] = c;
    r.m[2] = -s;
    r.m[8] = s;
    r.m[10] = c;
    return r;
}

Mat4 mat4_perspective(float fovy_rad, float aspect, float znear, float zfar) {
    Mat4 r;
    memset(&r, 0, sizeof(r));
    float f = 1.0f / tanf(fovy_rad * 0.5f);
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (zfar + znear) / (znear - zfar);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * zfar * znear) / (znear - zfar);
    return r;
}

Mat4 mat4_ortho(float left, float right, float bottom, float top, float znear, float zfar) {
    Mat4 r = mat4_identity();
    r.m[0] = 2.0f / (right - left);
    r.m[5] = 2.0f / (top - bottom);
    r.m[10] = -2.0f / (zfar - znear);
    r.m[12] = -(right + left) / (right - left);
    r.m[13] = -(top + bottom) / (top - bottom);
    r.m[14] = -(zfar + znear) / (zfar - znear);
    return r;
}

Mat4 mat4_look_at(Vec3 eye, Vec3 center, Vec3 up) {
    Vec3 f = vec3_norm(vec3_sub(center, eye));
    Vec3 s = vec3_norm(vec3_cross(f, up));
    Vec3 u = vec3_cross(s, f);

    Mat4 r = mat4_identity();
    r.m[0] = s.x;  r.m[4] = s.y;  r.m[8]  = s.z;
    r.m[1] = u.x;  r.m[5] = u.y;  r.m[9]  = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -vec3_dot(s, eye);
    r.m[13] = -vec3_dot(u, eye);
    r.m[14] = vec3_dot(f, eye);
    return r;
}

bool aabb_overlap(AABB a, AABB b) {
    return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
           (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
           (a.min.z <= b.max.z && a.max.z >= b.min.z);
}

bool sphere_aabb_overlap(Vec3 center, float radius, AABB box) {
    float cx = sw_clampf(center.x, box.min.x, box.max.x);
    float cy = sw_clampf(center.y, box.min.y, box.max.y);
    float cz = sw_clampf(center.z, box.min.z, box.max.z);
    float dx = center.x - cx;
    float dy = center.y - cy;
    float dz = center.z - cz;
    return (dx * dx + dy * dy + dz * dz) <= (radius * radius);
}

bool ray_aabb_hit(Vec3 origin, Vec3 dir, AABB box, float max_dist, float *out_t) {
    float tmin = 0.0f;
    float tmax = max_dist;
    const float o[3] = {origin.x, origin.y, origin.z};
    const float d[3] = {dir.x, dir.y, dir.z};
    const float bmin[3] = {box.min.x, box.min.y, box.min.z};
    const float bmax[3] = {box.max.x, box.max.y, box.max.z};

    for (int i = 0; i < 3; ++i) {
        if (fabsf(d[i]) < 1e-6f) {
            if (o[i] < bmin[i] || o[i] > bmax[i]) {
                return false;
            }
        } else {
            float inv = 1.0f / d[i];
            float t1 = (bmin[i] - o[i]) * inv;
            float t2 = (bmax[i] - o[i]) * inv;
            if (t1 > t2) {
                float tmp = t1;
                t1 = t2;
                t2 = tmp;
            }
            if (t1 > tmin) tmin = t1;
            if (t2 < tmax) tmax = t2;
            if (tmin > tmax) {
                return false;
            }
        }
    }
    if (out_t) {
        *out_t = tmin;
    }
    return true;
}
