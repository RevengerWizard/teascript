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
    setintfield(T, m, "upvalues", pt->upvalue_count);
    setintfield(T, m, "kconsts", pt->k_count);
    setintfield(T, m, "bytecodes", pt->bc_count);
    setstrV(T, T->top++, pt->name);
    tea_set_key(T, -2, "name");
    setprotoV(T, tea_map_setstr(T, m, tea_str_newlit(T, "proto")), pt);
}

static void debug_funck(tea_State* T)
{
    GCproto* pt = tea_lib_checkTproto(T, 0, false);
    int32_t idx = tea_lib_checkint(T, 1);
    copyTV(T, T->top - 1, proto_kgc(pt, idx));
}

static void debug_funcbc(tea_State* T)
{
    GCproto* pt = tea_lib_checkTproto(T, 0, false);
    int32_t idx = tea_lib_checkint(T, 1);
    setnumV(T->top - 1, proto_bc(pt, idx));
}

static void debug_funcline(tea_State* T)
{
    GCproto* pt = tea_lib_checkTproto(T, 0, false);
    int32_t ofs = tea_lib_checkint(T, 1);
    setnumV(T->top++, tea_debug_line(pt, ofs));
}

/* ------------------------------------------------------------------------ */

static const tea_Reg debug_module[] = {
    { "funcinfo", debug_funcinfo, 1, 0 },
    { "funck", debug_funck, 2, 0 },
    { "funcbc", debug_funcbc, 2, 0 },
    { "funcline", debug_funcline, 2, 0 },
    { NULL, NULL }
};

TEAMOD_API void tea_import_debug(tea_State* T)
{
    tea_create_module(T, TEA_MODULE_DEBUG, debug_module);
}