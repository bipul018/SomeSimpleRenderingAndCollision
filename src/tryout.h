#pragma once
#include "misc_tools.h"
#include <Windows.h>

// Actual point union, just hope sizeof vert is larger
union Point {
    VertexInput vert;
    struct {
        POINT point;
        Vec2 normal; // Normal in the xy plane, targeted for beizer
                     // curves
    };
};


//A linked list ds, where node is index to vertex
struct CurveNode {
    struct CurveNode *prev;
    struct CurveNode *next;
    union Point *point;
    UINT flag;

};
typedef struct CurveNode CurveNode;

enum CurveNodeFlag {
    CurveVisit,
    CurveUsed,
    CurveActive,
    CurveSmooth,
};

static inline int get_flag( enum CurveNodeFlag flag ) {
    return 1 << flag;
}

struct Triangle {
    IndexInput a, b, c;
};

typedef struct Triangle Triangle;



//Find if a value is in between other two values(inclusive)
#define check_if_in_between(v, a, b)    \
    ((  ((a) <= (b))&&( (v) >= (a)   )&&( (v) <= (b)  )         ) ||\
     (  ((a) >= (b))&&( (v) >= (b)   )&& ((v)<= (a)) ))


#define vec2_cross_prod( v1, v2)       \
    ( ( (v1).x  )  * ( (v2).y  )  - ( (v1).y  ) * ( (v2).x )   )

size_t count_curve_nodes(CurveNode * head);

//A struct as a temporary container, for now, used as a linked list
typedef struct TempStruct
{
    CurveNode *top;
    CurveNode *prev;
    CurveNode *next;

    struct TempStruct *another;
}TempStruct;

//"Foolproof version" hopefully
typedef struct ProcessNode
{
    union Point *point;
    enum CurveNodeFlag flag;
    struct Child
    {
        struct ProcessNode *prev;
        struct ProcessNode *next;
        struct Child *another;
    } *children;

}ProcessNode;



//Collects the curve into the dst in sequential y order
void sort_process_curve( ProcessNode * ptr, size_t count);

//Now let's try for an array of heads, winding order pre maintained
void triangulate_curve(StackAllocator * stk_allocr, size_t stk_offset,
                            ProcessNode * nodes, size_t total_points,union Point * base_point_ptr, Triangle * triangles, size_t triangle_count);
