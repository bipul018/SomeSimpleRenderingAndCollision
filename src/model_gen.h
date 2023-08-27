#pragma once
#include "misc_tools.h"
#include <stdbool.h>

typedef struct {
    size_t vertex_count;
    size_t index_count;
    VertexInput *vertices;
    IndexInput *indices;
} GenerateModelOutput;
GenerateModelOutput load_sphere_uv(StackAllocator *stk_allocr,
                                   size_t stk_offset,
                                   size_t num_parts, float radius);


GenerateModelOutput load_cuboid_aa(StackAllocator *stk_allocr,
                                   size_t stk_offset, Vec3 dims);

GenerateModelOutput load_tube_solid(StackAllocator *stk_allocr, size_t stk_offset, int face_sides, int divisions);

static inline Vec3 lerp_pos_func(Vec3 vecs[2], float t) {
    float s = 1 - t;
        return vec3_add(vec3_scale_fl(vecs[0], s),
                        vec3_scale_fl(vecs[1], t));

}

static inline Vec3 lerp_grad_func(Vec3 vecs[2], float t) {
        return vec3_add(vec3_scale_fl(vecs[0], -1.f), vecs[1]);
}

static inline Vec3 lerp_acc_func(Vec3 vecs[2], float t){
    return (Vec3){0.f,0.f,0.f};
}

typedef Vec3 (Vec3ParaFunc)(void * user_data, float t);

bool remodel_verts_tube(struct Model model, int sides, int divs, float radius, Vec3ParaFunc pos_func , Vec3ParaFunc grad_func , Vec3ParaFunc accn_func, void * user_data);

static inline Vec3 quad_bezier_pos_func(Vec3 vecs[3], float t) {
    float s = 1 - t;

    return vec3_add_3(vec3_scale_fl(vecs[0], s * s ),
                      vec3_scale_fl(vecs[1], 2 * s * t),
                      vec3_scale_fl(vecs[2], t * t));
}


static inline Vec3 quad_bezier_grad_func(Vec3 vecs[3], float t) {
    float s = 1 - t;

    return vec3_add_3(vec3_scale_fl(vecs[0], -2 * s),
                      vec3_scale_fl(vecs[1], 2 * (s - t)),
                      vec3_scale_fl(vecs[2], 2 * t));
}

static inline Vec3  quad_bezier_acc_func(Vec3  vecs[3], float t){
    float s = 1 - t;
    return vec3_add_3(vec3_scale_fl(vecs[0], 2.f),
                      vec3_scale_fl(vecs[1], -4.f),
                      vec3_scale_fl(vecs[2], 2.f));
}

static inline Vec3 cubic_bezier_pos_func(Vec3 vecs[4], float t) {
    float s = 1 - t;

    return vec3_add_4(vec3_scale_fl(vecs[0], s * s * s),
                      vec3_scale_fl(vecs[1], 3 * s * s * t),
                      vec3_scale_fl(vecs[2], 3 * s * t * t),
                      vec3_scale_fl(vecs[3], t * t * t));
}


static inline Vec3 cubic_bezier_grad_func(Vec3 vecs[4], float t) {
    float s = 1 - t;

    return vec3_add_4(vec3_scale_fl(vecs[0], -3.f * s * s),
                      vec3_scale_fl(vecs[1], 3 * (s * s  - 2 * s * t)),
                      vec3_scale_fl(vecs[2], 3 * (2 * s * t - t * t)),
                      vec3_scale_fl(vecs[3], 3.f * t * t));
}

static inline Vec3  cubic_bezier_acc_func(Vec3  vecs[4], float t){
    float s = 1 - t;
    return vec3_add_4(vec3_scale_fl(vecs[0], 6 * s),
                      vec3_scale_fl(vecs[1], 6 * (t - 2 * s)),
                      vec3_scale_fl(vecs[2], 6 * (s - 2 * t)),
                      vec3_scale_fl(vecs[3], 6 * t));
}

static inline Vec3  cubic_bezier_jerk_func(Vec3  vecs[4], float t){
    return vec3_add_4(vec3_scale_fl(vecs[0], -6),
                      vec3_scale_fl(vecs[1], 18),
                      vec3_scale_fl(vecs[2], -18),
                      vec3_scale_fl(vecs[3], 6));
}



static inline Vec3  ellipse_pos_func(Vec3  vecs[3], float t){
    Vec3  right = vec3_sub(vecs[1] , vecs[0]);
    Vec3  up = vec3_sub(vecs[2],vecs[0]);
    return vec3_add(vecs[0], vec3_add(
            vec3_scale_fl(right, cosf(t * 2 * M_PI)),
            vec3_scale_fl(up, sinf(t * 2 * M_PI))
            ));
}


static inline Vec3  ellipse_grad_func(Vec3  vecs[3], float t){
    Vec3  right = vec3_sub(vecs[1] , vecs[0]);
    Vec3  up = vec3_sub(vecs[2],vecs[0]);
    return vec3_add(
            vec3_scale_fl(right, -2*M_PI *sinf(t * 2 * M_PI)),
            vec3_scale_fl(up, 2 * M_PI * cosf(t * 2 * M_PI))
            );
}


static inline Vec3  ellipse_acc_func(Vec3  vecs[3], float t){
    Vec3  right = vec3_sub(vecs[1] , vecs[0]);
    Vec3  up = vec3_sub(vecs[2],vecs[0]);
    return vec3_add(
            vec3_scale_fl(right, - 4 * M_PI * M_PI * cosf(t * 2 * M_PI)),
            vec3_scale_fl(up, - 4 * M_PI * M_PI * sinf(t * 2 * M_PI))
            );
}



GenerateModelOutput load_text_character(StackAllocator *stk_allocr,size_t stk_offset,int codepoint, Vec3 extrude);
