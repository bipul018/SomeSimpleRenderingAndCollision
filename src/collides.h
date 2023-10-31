//
// Created by bipul on 10/6/2023.
//

#ifndef CLIONTEST_COLLIDES_H
#define CLIONTEST_COLLIDES_H
#include <extra_c_macros.h>
#include "vectors.h"
ty_struct(Collidable){
        Vec3 pos;
        Vec3 vel;
        Vec3 force;
        float mass; //-ve mass = infinite mass
        enum{
            COLL_SHAPE_SPHERE,
                    COLL_SHAPE_PLANE,
        } shape_type;
        union{
            float sphere_radius;
            Vec3 plane_normal;
        }shape;
};


//Adds optionally momentum "gained" due to infinite mass objects and similar approximations
void resolve_collision(Collidable* og_data, float delt, int obj_count, double * gained_momentums);


#endif //CLIONTEST_COLLIDES_H
