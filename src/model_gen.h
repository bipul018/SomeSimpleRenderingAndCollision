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

inline Vec3 lerp_pos_func(Vec3 vecs[3], float t) {
    float s = 1 - t;
    if (t <= 0.5f)
        return vec3_add(vec3_scale_fl(vecs[0], s),
                        vec3_scale_fl(vecs[1], t));
    else
        return vec3_add(vec3_scale_fl(vecs[1], s),
                        vec3_scale_fl(vecs[2], t));

}

inline Vec3 lerp_grad_func(Vec3 vecs[3], float t) {
    if (t <= 0.5f)
        return vec3_add(vec3_scale_fl(vecs[0], -1.f), vecs[1]);
    else
        return vec3_add(vec3_scale_fl(vecs[1], -1.f), vecs[2]);
}

typedef Vec3 (*Vec3ParaFunc)(void * user_data, float t);

bool remodel_verts_tube(struct Model model, int sides, int divs, float radius, typeof(Vec3(void*,float)) pos_func , Vec3 grad_func(void*,float) , void * user_data);

inline Vec3 bezier_pos_func(Vec3 vecs[4], float t) {
    float s = 1 - t;

    return vec3_add_4(vec3_scale_fl(vecs[0], s * s * s),
                      vec3_scale_fl(vecs[1], 3 * s * s * t),
                      vec3_scale_fl(vecs[2], 3 * s * t * t),
                      vec3_scale_fl(vecs[3], t * t * t));
}


inline Vec3 bezier_grad_func(Vec3 vecs[4], float t) {
    float s = 1 - t;

    return vec3_add_4(vec3_scale_fl(vecs[0], -3.f * s * s),
                      vec3_scale_fl(vecs[1], 3 * (s * s  + -2.f * s)),
                      vec3_scale_fl(vecs[2], 3 * (s *2* t - t * t)),
                      vec3_scale_fl(vecs[3], 3.f * t * t));
}



GenerateModelOutput load_text_character(StackAllocator *stk_allocr,size_t stk_offset,int codepoint, Vec3 extrude);
