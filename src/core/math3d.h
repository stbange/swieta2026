#ifndef SWIETA_MATH3D_H
#define SWIETA_MATH3D_H

#include <stdbool.h>

#define SW_PI 3.14159265358979323846f

typedef struct {
    float x;
    float y;
} Vec2;

typedef struct {
    float x;
    float y;
    float z;
} Vec3;

typedef struct {
    float x;
    float y;
    float z;
    float w;
} Vec4;

/* Column-major 4x4 matrix compatible with OpenGL */
typedef struct {
    float m[16];
} Mat4;

typedef struct {
    Vec3 min;
    Vec3 max;
} AABB;

float sw_clampf(float v, float lo, float hi);
float sw_lerpf(float a, float b, float t);
float sw_wrap_angle(float rad);

Vec3 vec3_make(float x, float y, float z);
Vec3 vec3_add(Vec3 a, Vec3 b);
Vec3 vec3_sub(Vec3 a, Vec3 b);
Vec3 vec3_scale(Vec3 v, float s);
float vec3_dot(Vec3 a, Vec3 b);
Vec3 vec3_cross(Vec3 a, Vec3 b);
float vec3_len_sq(Vec3 v);
float vec3_len(Vec3 v);
Vec3 vec3_norm(Vec3 v);
float vec3_dist(Vec3 a, Vec3 b);
Vec3 vec3_lerp(Vec3 a, Vec3 b, float t);

Mat4 mat4_identity(void);
Mat4 mat4_mul(Mat4 a, Mat4 b);
Vec4 mat4_mul_vec4(Mat4 m, Vec4 v);
Mat4 mat4_translate(Vec3 t);
Mat4 mat4_scale(Vec3 s);
Mat4 mat4_rotate_x(float rad);
Mat4 mat4_rotate_y(float rad);
Mat4 mat4_perspective(float fovy_rad, float aspect, float znear, float zfar);
Mat4 mat4_ortho(float left, float right, float bottom, float top, float znear, float zfar);
Mat4 mat4_look_at(Vec3 eye, Vec3 center, Vec3 up);

bool aabb_overlap(AABB a, AABB b);
bool sphere_aabb_overlap(Vec3 center, float radius, AABB box);
bool ray_aabb_hit(Vec3 origin, Vec3 dir, AABB box, float max_dist, float *out_t);

#endif /* SWIETA_MATH3D_H */
