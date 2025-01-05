/*
** tea_debug.c
** Debugging and introspection
*/

#define tea_debug_c
#define TEA_CORE

#include "tea_debug.h"
#include "tea_obj.h"
#include "tea_buf.h"
#include "tea_state.h"
#include "tea_strfmt.h"

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

void tea_debug_stacktrace(tea_State* T, GCstr* msg)
{
    SBuf* sb = &T->strbuf;
    tea_buf_reset(sb);
    tea_buf_putstr(T, sb, msg);
    tea_buf_putlit(T, sb, "\n");
    for(CallInfo* ci = T->ci; ci > T->ci_base; ci--)
    {
        /* Skip stack trace for C functions */
        if(iscfunc(ci->func)) continue;
        GCmodule* module = ci->func->t.module;
        GCproto* pt = ci->func->t.pt;
        BCPos pc = ci->ip - proto_bc(pt) - 1;
        tea_strfmt_pushf(T, "File %s [line %d] in function %s\n", 
            str_data(module->name), tea_debug_line(pt, pc), str_data(pt->name));
        msg = strV(T->top - 1);
        tea_buf_putstr(T, sb, msg);
        T->top--;
    }
    sb->w--;
    setstrV(T, T->top, tea_buf_str(T, sb));
    incr_top(T);
}