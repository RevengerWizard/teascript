/*
** lib_debug.c
** Teascript debug module
*/

#define lib_debug_c
#define TEA_LIB

#include "tea.h"
#include "tealib.h"

#include "tea_obj.h"
#include "tea_map.h"
#include "tea_str.h"
#include "tea_lib.h"
#include "tea_debug.h"

/* -- Reflection API for Teascript functions ------------------------------------ */

static void setintfield(tea_State* T, GCmap* m, const char* name, int32_t val)
{
    setintV(tea_map_setstr(T, m, tea_str_newlen(T, name)), val);
}

static void debug_funcinfo(tea_State* T)
{
    GCproto* pt = tea_lib_checkTproto(T, 0, false);
    GCmap* m;
    tea_new_map(T);
    m = mapV(T->top - 1);
    setintfield(T, m, "upvalues", pt->sizeuv);
    setintfield(T, m, "kconsts", pt->sizek);
    setintfield(T, m, "bytecodes", pt->sizebc);
    tea_push_bool(T, (pt->flags & PROTO_CHILD));
    tea_set_key(T, -2, "children");
    setstrV(T, T->top++, pt->name);
    tea_set_key(T, -2, "name");
    setprotoV(T, tea_map_setstr(T, m, tea_str_newlit(T, "proto")), pt);
}

static void debug_funck(tea_State* T)
{
    GCproto* pt = tea_lib_checkTproto(T, 0, false);
    int32_t idx = tea_lib_checkint(T, 1);
    if(idx >= 0 && idx < (int32_t)pt->sizek)
    {
        copyTV(T, T->top - 1, proto_kgc(pt, idx));
        return;
    }
    setnilV(T->top - 1);
}

static void debug_funcbc(tea_State* T)
{
    GCproto* pt = tea_lib_checkTproto(T, 0, false);
    BCPos pc = (BCPos)tea_lib_checkint(T, 1);
    if(pc < pt->sizebc)
    {
        setnumV(T->top - 1, proto_bc(pt)[pc]);
        return;
    }
    setnilV(T->top - 1);
}

static void debug_funcuv(tea_State* T)
{
    GCproto* pt = tea_lib_checkTproto(T, 0, false);
    int32_t idx = tea_lib_checkint(T, 1);
    if(idx < pt->sizeuv)
    {
        setnumV(T->top - 1, pt->uv[idx]);
        return;
    }
    setnilV(T->top - 1);
}

static void debug_funcline(tea_State* T)
{
    GCproto* pt = tea_lib_checkTproto(T, 0, false);
    BCLine ofs = (BCLine)tea_lib_checkint(T, 1);
    setnumV(T->top++, tea_debug_line(pt, ofs));
}

/* ------------------------------------------------------------------------ */

static const tea_Reg debug_module[] = {
    { "funcinfo", debug_funcinfo, 1, 0 },
    { "funck", debug_funck, 2, 0 },
    { "funcbc", debug_funcbc, 2, 0 },
    { "funcuv", debug_funcuv, 2, 0 },
    { "funcline", debug_funcline, 2, 0 },
    { NULL, NULL }
};

TEAMOD_API void tea_import_debug(tea_State* T)
{
    tea_create_module(T, TEA_MODULE_DEBUG, debug_module);
}