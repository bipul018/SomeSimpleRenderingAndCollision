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



#endif //CLIONTEST_POLYNOMIALS_H
