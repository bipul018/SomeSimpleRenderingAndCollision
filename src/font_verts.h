#pragma once

#include "common-stuff.h"

#include "tryout.h"
#include <stdlib.h>


void init_font_file(const char *file_name);

void clear_font_file();



struct AddCurveOutput {
    size_t curve_count;
    size_t total_points;
    CurveNode **curves;
    //First curve node of curves is the overall first curve point
    union Point *first_point;
    IndexInput *indices;
    //Offset of stack allocator after this function is used up
    size_t stk_offset;

};

struct AddCurveOutput add_font_verts(StackAllocator *stk_allocr,
    size_t stk_offset,
    int codepoint);

