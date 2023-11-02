
#include "collides.h"


#include "vectors.h"
#include <stdlib.h>


#define MAX_POLY_TERMS 5

//Minheap copied

ty_struct(HeapId){
    int obj1;
    int obj2;
    //Order of objs doesn't change result, so
    //keep obj1 < obj2
};

HeapId make_id(int inx1, int inx2){
    if (inx1 < inx2){
        return (HeapId){.obj1 = inx1, .obj2 = inx2};
    }
    else{
        return (HeapId){.obj1 = inx2, .obj2 = inx1};
    }
}

bool are_same_id(HeapId a, HeapId b){
    return (a.obj1 == b.obj1) && (a.obj2 == b.obj2);
}

ty_struct(HeapNode){
    float wt;
    HeapId id;
};



ty_struct(OptHeapNode){
    HeapNode data;
    bool avail;
};


//This heapifies from bottom to top
void heapify_up(HeapNode* heap, int inx, int size){

    while(inx > 0){
        int root = (inx - 1)>>1;
        if (heap[root].wt < heap[inx].wt)
            break;
        HeapNode hold = heap[root];
        heap[root] = heap[inx];
        heap[inx] = hold;
        inx = root;

    }

}

//This heapifies from top to bottom
void heapify_down(HeapNode* heap, int inx, int size){

    while(inx < size){
        if((2*inx + 1) >= size)
            goto end_of_fxn;
        int ch1 = 2 * inx + 1;
        int ch2 = 2 * inx + 2;


        if(heap[inx].wt < _min(heap[ch1].wt, heap[ch2].wt))
            goto end_of_fxn;
        int new_inx = (heap[ch1].wt < heap[ch2].wt) ? ch1: ch2;
        HeapNode hold = heap[inx];
        heap[inx] = heap[new_inx];
        heap[new_inx] = hold;
        inx = new_inx;


    }
    end_of_fxn:
}


OptHeapNode minheap_insert(HeapNode* heap, int count, int max_count, HeapNode node){
    OptHeapNode out = {0};

    if(count == max_count){
        out.avail = true;
        out.data = heap[count-1];
        heap[count-1] = node;
        heapify_up(heap, count - 1,count);
    }
    else{
        heap[count] = node;
        heapify_up(heap, count,count + 1);
    }

    return out;
}

int heap_find(HeapNode* heap, int count, HeapId id){
    for_range(i,0,count){
        if(are_same_id(id,heap[i].id))
            return i;
    }
    return -1;
}

bool minheap_replace(HeapNode* heap, int count, int inx, HeapNode new){

    if((inx < 0) || (inx >= count))
        return false;

    heap[inx] = new;

    if((inx == 0) || (heap[inx].wt > heap[(inx - 1)>>1].wt)){
        heapify_down(heap,inx,count);
    }
    else{
        heapify_up(heap,inx,count);
    }

    return true;

}

bool minheap_delete(HeapNode* heap, int count, int inx){
    HeapNode last = heap[count-1];
    if(minheap_replace(heap,count-1,inx,last)){
        heap[count-1]  = (HeapNode){0};
        return true;
    }
    else{
        return false;
    }
}

bool
euler_collision(const Collidable *obj1, const Collidable *obj2, float time1, float time2, float delt, float *out_t) {//Make relative , origin is at ith object
    float t0 = _max(time1, time2);


    Vec3 r = vec3_sub(obj2[0].pos,obj1[0].pos);
    Vec3 v = vec3_sub(obj2[0].vel, obj1[0].vel);

    if(time1 < time2){
        r = vec3_add(r, vec3_scale_fl(obj1->vel, time1 - time2));
    }
    else{
        r = vec3_add(r, vec3_scale_fl(obj2->vel,time1 - time2));
    }


    if((obj1->shape_type == COLL_SHAPE_SPHERE) && (obj2->shape_type == COLL_SHAPE_SPHERE)){
        // | r + v t| ^2  = |D| ^2
        //  t =    - r.v +- sqrt [(r.v)^2 - v^2 (r^2 - d^2) ]
        //      -------------------------------------------
        //                          v^2

        float rv = vec3_dot(v, r);
        float vsq = vec3_dot(v,v);
        float rsq = vec3_dot(r,r);
        float dsq = obj1->shape.sphere_radius + obj2->shape.sphere_radius;
        dsq = dsq * dsq;

        float des = rv * rv - vsq * (rsq - dsq);

        if(des > 0.f){
            des = sqrtf(des) / vsq;
            float t1 = - rv / vsq - des;
            float t2 = - rv / vsq + des;

            if(((t2 > 0.f) && (t2 <= (delt-t0))) || ((t1 > 0.f) && (t1 <= (delt-t0)))){
                float t = (t1 > 0.f) ? t1 : t2;
                *out_t = t + t0;
                return true;
            }

        }



    }
    else if((obj1->shape_type == COLL_SHAPE_PLANE ) && (obj2->shape_type == COLL_SHAPE_PLANE)){
        //Don't handle this one, not possible collision
    }
    else{
        // R = R0 - D * N
        // [R + Vt].N = 0
        //t = - (R.N) / (V.N) = - (R0.N - D) / (V.N)
        // plane must pass thru origin
        Vec3 n;
        float d;
        if(obj1->shape_type == COLL_SHAPE_SPHERE){
            v = vec3_neg(v);
            r = vec3_neg(r);
            n = obj2->shape.plane_normal;
            d = obj1->shape.sphere_radius;
        }
        else{
            n = obj1->shape.plane_normal;
            d = obj2->shape.sphere_radius;
        }

        float rn = vec3_dot(r,n);
        float vn = vec3_dot(v,n);
        float t = (d - rn)/vn;
        if((t > 0.f) && (t <= (delt-t0))){
            *out_t = t+t0;
            return true;
        }

    }
    return false;
}

//Given u1 & u2 being initial velocities, and v1 & v2 being final velocities,
//Returns if collsion happens again after this
bool test_coll_res_validity(float u1, float u2, float v1, float v2, float epsilon){

    //If u1 ≃ 0
    if( u1 >= -epsilon && u1 <= epsilon ){
    //Now if also u2 ≃ 0
        if(u2 >= -epsilon && u2 <= epsilon){
            return (v1-v2) >= -epsilon && (v1-v2) <= epsilon;
        }
        _swap(u1,u2);
        _swap(v1,v2);
    }

    //Make it so that u1 > u2 and u1 >= 0

    //If u1 < 0
    if(( u1 < -epsilon) && (u2 < -epsilon)){
        u1 = -u1;
        u2 = -u2;
        v1 = -v1;
        v2 = -v2;
    }

    if((u1 - u2) < epsilon){
        _swap(u1,u2);
        _swap(v1,v2);
    }


    //First post condition
    if((v2 - v1) < 0.f)
        return false;

    if((u2 >= epsilon) && (v1 < -epsilon) && (v2 < -epsilon))
        return false;

    return true;

    //I don't think all cases have been resolved



}

//Adds optionally momentum "gained" due to infinite mass objects and similar approximations
void resolve_collision(Collidable* og_data, float delt, int obj_count, double * gained_momentums){

    int max_nodes = obj_count * (obj_count - 1) / 2;

    HeapNode* heap_tree = malloc(sizeof(*heap_tree) * max_nodes);
    if(!heap_tree)
        return;
    memset(heap_tree, 0, sizeof(*heap_tree) * max_nodes);

    float* times = malloc(obj_count * sizeof * times);
    if(!times){
        free(heap_tree);
        return;
    }
    memset(times, 0, obj_count * sizeof * times);

    int node_count = 0;

    for_range(i, 0, obj_count){
        for_range(j,i+1, obj_count){

            float t_out = 0.f;
            if( euler_collision(og_data + i, og_data + j, times[i], times[j], delt, &t_out)) {
                HeapNode add;
                add.wt = t_out;
                add.id = make_id(i, j);
                minheap_insert(heap_tree, node_count, max_nodes, add);
                node_count++;
            }


        }
    }
    const float epsilon = 0.0005f;

    while(1) {
        //All pairs of objects have been evaluated
        if (node_count <= 0) {
            break;
        }

        HeapNode top = heap_tree[0];

        //Handle collision here
        bool v1changed = true;
        bool v2changed = true;
        {
            //Update the values to collision time first
            //only position updates, not velocity

            og_data[top.id.obj1].pos = vec3_add(og_data[top.id.obj1].pos,
                                                vec3_scale_fl(og_data[top.id.obj1].vel, top.wt - times[top.id.obj1]));
            og_data[top.id.obj2].pos = vec3_add(og_data[top.id.obj2].pos,
                                                vec3_scale_fl(og_data[top.id.obj2].vel, top.wt - times[top.id.obj2]));
            times[top.id.obj1] = times[top.id.obj2] = top.wt;

            //Direction of collision
            Vec3 norm = {0};
            Collidable *obj1 = og_data + top.id.obj1;
            Collidable *obj2 = og_data + top.id.obj2;

            if (obj1->shape_type == COLL_SHAPE_SPHERE && obj2->shape_type == COLL_SHAPE_SPHERE) {
                norm = vec3_sub(obj1->pos, obj2->pos);
                norm = vec3_normalize(norm);
            }
            if (obj1->shape_type == COLL_SHAPE_SPHERE && obj2->shape_type == COLL_SHAPE_PLANE) {
                norm = obj2->shape.plane_normal;
            }
            if (obj1->shape_type == COLL_SHAPE_PLANE && obj2->shape_type == COLL_SHAPE_SPHERE) {
                norm = obj1->shape.plane_normal;
            }
            //For precision issues, make normal 0 if nearly zero
            for_range(i, 0, 3){
                if((norm.comps[i] >= -epsilon / 20.f) && (norm.comps[i] <= epsilon / 20.f)){
                    norm.comps[i] = 0;
                }
            }

            //Velocities in direction of normal, away from normal
            float un1 = vec3_dot(obj1->vel, norm);
            Vec3 vn1 = vec3_scale_fl(norm, un1);
            float un2 = vec3_dot(obj2->vel, norm);
            Vec3 vn2 = vec3_scale_fl(norm, un2);
            obj1->vel = vec3_sub(obj1->vel, vn1);
            obj2->vel = vec3_sub(obj2->vel, vn2);



            //Special cases then normal case


            //Both of almost equal masses
            if ((obj1->mass < 0.f && obj2->mass < 0.f) ||
                ((obj1->mass - obj2->mass) < epsilon && (obj1->mass - obj2->mass) > -epsilon)) {
                _swap(vn1, vn2);
            }
                //Obj1 has too low mass
            else if ((obj2->mass < 0.f) || (fabsf(obj1->mass) < epsilon) || (fabsf(obj1->mass / obj2->mass) < epsilon)) {
                vn1 = vec3_neg(vn1);
                if(gained_momentums) {
                    Vec3 gained_p = vec3_scale_fl(vn1, -2.f * obj1->mass);
                    gained_momentums[0] += gained_p.x;
                    gained_momentums[1] += gained_p.y;
                    gained_momentums[2] += gained_p.z;
                }
                v2changed = false;
            }
                //Obj2 has too low mass
            else if ((obj1->mass < 0.f) || (fabsf(obj2->mass) < epsilon) || (fabsf(obj2->mass / obj1->mass) < epsilon)) {
                vn2 = vec3_neg(vn2);
                if(gained_momentums) {
                    Vec3 gained_p = vec3_scale_fl(vn2, -2.f * obj2->mass);
                    gained_momentums[0] += gained_p.x;
                    gained_momentums[1] += gained_p.y;
                    gained_momentums[2] += gained_p.z;
                }
                v1changed = false;
            } else {
                float P = un1 * obj1->mass + un2 * obj2->mass;
                float E2 = un1 * un1 * obj1->mass + un2 * un2 * obj2->mass;

                //v1 = P/M +- sqrt of (m2/m1M) * ( 2E - P^2 / M )
                //v2 = P/M -+ sqrt of (m1/m2M) * ( 2E - P^2 / M )

                // = A +- sqrtof(m1/m2 or m2/m1) * B

                float Afac = P / (obj1->mass + obj2->mass);
                float Bfac = sqrt((E2 - P * P / (obj1->mass + obj2->mass)) / (obj1->mass + obj2->mass));

                float if1 = sqrtf(obj2->mass / obj1->mass);
                float if2 = sqrtf(obj1->mass / obj2->mass);

                if (!test_coll_res_validity(un1, un2, Afac + if1 * Bfac, Afac - if2 * Bfac, epsilon)) {
                    vn1 = vec3_scale_fl(norm, Afac - if1 * Bfac);
                    vn2 = vec3_scale_fl(norm, Afac + if2 * Bfac);
                } else {
                    vn1 = vec3_scale_fl(norm, Afac + if1 * Bfac);
                    vn2 = vec3_scale_fl(norm, Afac - if2 * Bfac);
                }

            }


            obj1->vel = vec3_add(obj1->vel, vn1);
            obj2->vel = vec3_add(obj2->vel, vn2);

        }


        minheap_delete(heap_tree, node_count, 0);
        node_count--;
        for_range(i, 0, obj_count) {
            if ((i == top.id.obj1) || (i == top.id.obj2))
                continue;
            int inx;
            if (v1changed) {
                inx = heap_find(heap_tree, node_count, make_id(i, top.id.obj1));
                if (inx >= 0) {
                    minheap_delete(heap_tree, node_count, inx);
                    node_count--;
                }
            }
            if (v2changed) {
                inx = heap_find(heap_tree, node_count, make_id(i, top.id.obj2));
                if (inx >= 0) {
                    minheap_delete(heap_tree, node_count, inx);
                    node_count--;
                }
            }
        }

        for_range(i, 0, obj_count) {
            if ((i == top.id.obj1) || (i == top.id.obj2))
                continue;
            float t_out = 0.f;
            if(v1changed) {
                if (euler_collision(og_data + i, og_data + top.id.obj1, times[i], times[top.id.obj1], delt, &t_out)) {
                    HeapNode add;
                    add.wt = t_out;
                    add.id = make_id(i, top.id.obj1);
                    minheap_insert(heap_tree, node_count, max_nodes, add);
                    node_count++;
                }
            }
            if(v2changed) {
                if (euler_collision(og_data + i, og_data + top.id.obj2, times[i], times[top.id.obj2], delt, &t_out)) {
                    HeapNode add;
                    add.wt = t_out;
                    add.id = make_id(i, top.id.obj2);
                    minheap_insert(heap_tree, node_count, max_nodes, add);
                    node_count++;
                }
            }
        }


    }

    //Now update og_data to delt
    for_range(i, 0, obj_count){
        og_data[i].pos = vec3_add(og_data[i].pos, vec3_scale_fl(og_data[i].vel,delt - times[i]));
        if(og_data[i].mass > epsilon)
            og_data[i].vel = vec3_add(og_data[i].vel, vec3_scale_fl(og_data[i].force, delt / og_data[i].mass));

    }

    free(times);
    free(heap_tree);

}
