/*
** tea_debug.c
** Debugging and introspection
*/

#define tea_debug_c
#define TEA_CORE

#include "tea_debug.h"
#include "tea_obj.h"

/* -- Line numbers -------------------------------------------------------- */

/* Get line number for a bytecode position */
BCLine tea_debug_line(GCproto* pt, BCPos pc)
{
    const void* lineinfo = pt->lineinfo;
    if(pc <= pt->sizebc && lineinfo)
    {
        BCLine first = pt->firstline;
        if(pc == pt->sizebc)
            return first + pt->numline;
        if(pc-- == 0)
            return first;
        if(pt->numline < 256)
            return first + (BCLine)((const uint8_t*)lineinfo)[pc];
        else if(pt->numline < 65536)
            return first + (BCLine)((const uint16_t*)lineinfo)[pc];
        else
            return first + (BCLine)((const uint32_t*)lineinfo)[pc];
    }
    return 0;
}