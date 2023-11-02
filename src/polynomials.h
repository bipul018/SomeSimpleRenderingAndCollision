//
// Created by bipul on 10/9/2023.
//

#ifndef CLIONTEST_POLYNOMIALS_H
#define CLIONTEST_POLYNOMIALS_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>


//Here will try to do polynomials
#ifndef MAX_POLY_TERMS
#define MAX_POLY_TERMS 15
#endif
typedef double PolyCoeff;
//Will always be a "floating" type
#define POLY_COEFF_STR "lf"
typedef double Polynomial[MAX_POLY_TERMS];
#ifndef POLY_EPSILON
#define POLY_EPSILON 0.000001
#endif
bool poly_almost_zero(PolyCoeff coeff){
    return (coeff < POLY_EPSILON) && (coeff > -POLY_EPSILON);
}

bool poly_greater_than(PolyCoeff v1, PolyCoeff v2){
    return (v1 - v2) > POLY_EPSILON;
}

bool poly_less_than(PolyCoeff v1, PolyCoeff v2){
    return (v2 - v1) > POLY_EPSILON;
}

bool poly_in_range_inx(int inx){
    return (inx >= 0) && (inx < MAX_POLY_TERMS);
}

#define poly_for_each_begin(inx_var, poly) \
    for(int inx_var = 0; inx_var < MAX_POLY_TERMS; ++inx_var){\
        if(poly_almost_zero((poly)[inx_var]))                 \
            continue;


#define poly_for_each_begin_rev(inx_var, poly) \
    for(int inx_var = MAX_POLY_TERMS-1; inx_var >= 0; --inx_var){\
        if(poly_almost_zero((poly)[inx_var]))                 \
            continue;



#define poly_for_each_end() }

#define poly_for_each(inx_var) \
    for(int inx_var = 0; inx_var < MAX_POLY_TERMS; ++inx_var)

//If write_dst = dst_str_len = 0, then write to stdout
//If dst_str_len = 0 only, then write_dst is a FILE*
//Else, write_dst is a char *
void poly_write(Polynomial poly, void * write_dst, size_t dst_str_len){

    poly_for_each_begin(i,poly)
        if(!write_dst && !dst_str_len){
            printf("+ %.3"POLY_COEFF_STR" x^%d",(float)poly[i], (int)i);
        }
        else if(!dst_str_len){
            fprintf((FILE*)write_dst,"+ %.3"POLY_COEFF_STR" x^%d",(float)poly[i], (int)i);
        }
        else{
            int kk = sprintf_s((char*)write_dst,dst_str_len,"+ %.3"POLY_COEFF_STR" x^%d",(float)poly[i], (int)i);
            write_dst = (char*)write_dst + kk;
            dst_str_len -= kk;
        }
    poly_for_each_end()

}

void poly_copy(Polynomial dst, const Polynomial src){
    memcpy(dst,src,sizeof(src[0]) * MAX_POLY_TERMS);
}

void poly_zero(Polynomial dst){
    memset(dst, 0, sizeof(dst[0])* MAX_POLY_TERMS);
}

void poly_add(Polynomial dst,const Polynomial src1,const Polynomial src2){
    poly_for_each(i){
        dst[i] = src1[i] + src2[i];
    }
}

void poly_sub(Polynomial dst,const Polynomial src1,const Polynomial src2){
    poly_for_each(i){
        dst[i] = src1[i] - src2[i];
    }
}

void poly_scale(Polynomial dst,const Polynomial src, PolyCoeff scale){
    poly_for_each_begin(i,src)
        dst[i] = src[i] * scale;
    poly_for_each_end()
}

void poly_neg(Polynomial dst, const Polynomial src){
    poly_for_each(i){
        dst[i] = -src[i];
    }
}

//src cannot be same as dst
void poly_shift(Polynomial dst, const Polynomial src, int shift_amt){
    poly_for_each(i){
        if(poly_in_range_inx(i - shift_amt))
            dst[i] = src[i - shift_amt];
        else
            dst[i] = 0;
    }
}

//src1 and src2 must be different from dst, dst must be zero initialized
void poly_prod(Polynomial dst,const Polynomial src1,const Polynomial src2){
    poly_for_each_begin(i,src2)
        Polynomial part = {0};
        poly_shift(part,src1,i);
        poly_scale(part,part,src2[i]);
        poly_add(dst,part,dst);
    poly_for_each_end()
}

int poly_deg(const Polynomial poly){
    poly_for_each_begin_rev(i,poly)
        return i;
    poly_for_each_end()
    return -1;
}

//No need to preinitialize quo and rem
void poly_div(Polynomial quo, Polynomial rem, const Polynomial src, const Polynomial div){

    if(!quo && !rem)
        return;

    Polynomial dummy_rem = {0};

    if(!rem)
        rem = dummy_rem;

    int divi_deg = poly_deg(div);
    //Follow restoring division method
    Polynomial dumdum = {0};
    if(quo)
        poly_zero(quo);
    poly_copy(rem,src);
    if(divi_deg == -1)
        return;
    poly_for_each_begin_rev(i,rem)
        if(i < divi_deg)
            break;
        poly_shift(dumdum,div,i-divi_deg);
        poly_scale(dumdum,dumdum,rem[i]);
        poly_scale(dumdum,dumdum,1.0f/div[divi_deg]);
        if(quo)
            quo[i-divi_deg] = rem[i] / div[divi_deg];
        poly_sub(rem,rem,dumdum);
    poly_for_each_end()
}

void poly_diff(Polynomial dst,const Polynomial src){
    poly_for_each_begin(i,src)
        if(poly_in_range_inx(i-1))
            dst[i-1] = src[i] * (PolyCoeff)i;
    poly_for_each_end()
}

PolyCoeff poly_eval(const Polynomial poly, PolyCoeff x){

    //Trying to numerically make more stable using Horner's method
    //P(x) = a0 + x(a1 + x(a2 + xa3))
    PolyCoeff out = 0;
    int prev_degg = -1;
    poly_for_each_begin_rev(i,poly)
        int scale_deg = prev_degg - i;
        if(prev_degg == -1)
            scale_deg = 1;
        prev_degg = i;
        out = poly[i] + pow(x,scale_deg) * out;
        //out += poly[i] * pow(x,i);
    poly_for_each_end()
    return out;
}

int poly_strum_chain(Polynomial* out_polys, const Polynomial in_poly){
    Polynomial workshop={0};
    int degg = poly_deg(in_poly);
    if(degg < 0)
        return 0;
    poly_copy(out_polys[0],in_poly);
    if(degg == 0)
        return 1;
    poly_diff(workshop,out_polys[0]);
    for(int i = 1; i <= degg; ++i){
        int degg = poly_deg(workshop);
        if(degg < 0)
            return i;
        poly_copy(out_polys[i],workshop);
        if(poly_deg(workshop) == 0){
            return i+1;
        }
        poly_div(NULL,workshop,out_polys[i-1],out_polys[i]);
        poly_neg(workshop,workshop);
    }
    return degg+1;
}

int poly_strum_sign_count(const Polynomial* in_chain,int poly_count, PolyCoeff x){
    int changes = 0;
    PolyCoeff prev_sign = 0;
    for(int i = 0; i < poly_count; ++i){
        PolyCoeff eval = poly_eval(in_chain[i],x);
        if(poly_almost_zero(prev_sign))
            prev_sign = eval;
        else if(!poly_almost_zero(eval)) {
            if (prev_sign * eval < 0)
                changes++;
            prev_sign = eval;
        }
    }
    return changes;
}

//src can't be quo: divides by (x - factor)
PolyCoeff poly_reduce_div(Polynomial quo, const Polynomial src, PolyCoeff factor){
    poly_for_each(inx){
        int n = MAX_POLY_TERMS - inx - 1;
        PolyCoeff an1 = poly_in_range_inx(n+1)?src[n+1] : 0;
        an1 = poly_almost_zero(an1)?0:an1;
        quo[n] = an1 + factor * (poly_in_range_inx(n+1)?quo[n+1]:0);
    }
    return src[0] + factor * quo[0];
}


void collision_poly(Polynomial out, Collidable o1, Collidable o2){
    poly_zero(out);
    //Relative motions
    Vec3 xr = vec3_sub(o2.pos,o1.pos);
    Vec3 vr = vec3_sub(o2.vel, o1.vel);
    Vec3 ar = vec3_sub(o2.force, o1.force);
    float sum_r = o1.shape.sphere_radius + o2.shape.sphere_radius;


    //Handle only sphere to sphere collision now
    if(o1.shape_type == COLL_SHAPE_SPHERE && o2.shape_type == COLL_SHAPE_SPHERE){
        out[4] = 0.25 * vec3_dot(ar,ar);
        out[3] = vec3_dot(vr,ar);
        out[2] = vec3_dot(vr,vr) + vec3_dot(xr,ar);
        out[1] = 2 * vec3_dot(xr,vr);
        out[0] = vec3_dot(xr,xr) - sum_r * sum_r;
    }
}

//Given a, b find first root in (a,b], if not found return a -ve value
//fx and dfx may be modified if necessary
float find_first_collision(Polynomial fx,Polynomial dfx, float ta, float tb){
    Polynomial chain[MAX_POLY_TERMS] = {0};
    PolyCoeff a = ta;
    PolyCoeff b = tb;
    //fa and fb
    PolyCoeff fa = poly_eval(fx,a);
    PolyCoeff fb = poly_eval(fx,b);

    //f'a and f'b
    PolyCoeff dfa = poly_eval(dfx, a);
    PolyCoeff dfb = poly_eval(dfx, b);

    //If fa = dfa = 0, or fb = dfb = 0, then we have a problem

    //Possible optimization : maybe when there is multiple roots, the collision is just "grazing"
    //so maybe no need to process that

    //Assumption, at most 2 roots occur at one place, maybe valid when relative motion ?
    Polynomial dumm = {0};
    bool changed = false;
    if(poly_almost_zero(fa) && poly_almost_zero(dfa)) {
        poly_reduce_div(dumm, fx, fa);
        poly_copy(fx, dumm);
        poly_zero(dumm);
        changed = true;
    }

    if(poly_almost_zero(fb) && poly_almost_zero(dfb)){
        poly_reduce_div(dumm, fx, fb);
        poly_copy(fx, dumm);
        poly_zero(dumm);
        changed = true;
    }
    if(changed) {
        poly_diff(dfx, fx);
        fa = poly_eval(fx, a);
        fb = poly_eval(fx, b);
    }


    int chain_n = poly_strum_chain(chain,fx);


    int sa, sb;
    sa = poly_strum_sign_count(chain,chain_n,a);
    sb = poly_strum_sign_count(chain,chain_n, b);

    if((sa - sb) <= 0)
        return -4.f;



    //There exists some solution, so bisection search for lower interval till one solution is achieved only
    while((sa-sb)>1){
        PolyCoeff m = b;
        PolyCoeff fm  ;
        PolyCoeff dfm ;
        do{
            m = (a+m)/2;
            fm = poly_eval(fx,m);
            dfm = poly_eval(dfx,m);
        }
        while(poly_almost_zero(fm) && poly_almost_zero(dfm));

        int sm = poly_strum_sign_count(chain,chain_n,m);
        //a,m if >= 1 roots
        if((sa - sm) >= 1){
            sb = sm;
            b = m;
            fb = fm;
            dfb = dfm;
        }
        else{
            sa = sm;
            a = m;
            fa = fm;
            dfa = dfm;
        }

    }

    //Use bisection method only for now
    while(true){
        if(!poly_almost_zero(fa) && poly_almost_zero((b-a) ))
            break;
        if(poly_almost_zero(fb))
            return b;
        PolyCoeff m = (a+b)/2;
        PolyCoeff fm = poly_eval(fx,m);
        if(poly_almost_zero(fm))
            return m;

        if(fm * fb > 0){
            b = m;
            fb = fm;
        }
        else{
            a = m;
            fa = fm;
        }


    }
    return (a+b)/2;

}


#endif //CLIONTEST_POLYNOMIALS_H
