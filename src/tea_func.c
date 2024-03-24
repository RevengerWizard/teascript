/*
** tea_func.c
** Function handling (prototypes, functions and upvalues)
*/

#define tea_func_c
#define TEA_CORE

#include "tea_state.h"
#include "tea_vm.h"
#include "tea_gc.h"

/* -- Prototypes -------------------------------------------------- */

GCproto* tea_func_newproto(tea_State* T, ProtoType type, GCmodule* module, int max_slots)
{
    GCproto* pt = tea_obj_new(T, GCproto, TEA_TPROTO);
    pt->arity = 0;
    pt->arity_optional = 0;
    pt->variadic = 0;
    pt->upvalue_count = 0;
    pt->max_slots = max_slots;
    pt->type = type;
    pt->name = NULL;
    pt->module = module;
    pt->bc_count = 0;
    pt->bc_size = 0;
    pt->bc = NULL;
    pt->line_count = 0;
    pt->line_size = 0;
    pt->lines = NULL;
    pt->k = NULL;
    pt->k_size = 0;
    pt->k_count = 0;
    return pt;
}

int tea_func_getline(GCproto* f, int instruction)
{
    int start = 0;
    int end = f->line_count - 1;

    while(true)
    {
        int mid = (start + end) / 2;
        LineStart* line = &f->lines[mid];
        if(instruction < line->offset)
        {
            end = mid - 1;
        }
        else if(mid == f->line_count - 1 || instruction < f->lines[mid + 1].offset)
        {
            return line->line;
        }
        else
        {
            start = mid + 1;
        }
    }
}

/* -- Upvalues -------------------------------------------------- */

/* Create an empty and closed upvalue */
static GCupvalue* func_newuv(tea_State* T, TValue* slot)
{
    GCupvalue* uv = tea_obj_new(T, GCupvalue, TEA_TUPVALUE);
    setnullV(&uv->closed);
    uv->location = slot;
    uv->next = NULL;
    return uv;
}

/* Find existing open upvalue for a stack slot or create a new one */
GCupvalue* tea_func_capture(tea_State* T, TValue* local)
{
    GCupvalue* prev_upvalue = NULL;
    GCupvalue* upvalue = T->open_upvalues;
    while(upvalue != NULL && upvalue->location > local)
    {
        prev_upvalue = upvalue;
        upvalue = upvalue->next;
    }

    if(upvalue != NULL && upvalue->location == local)
    {
        return upvalue;
    }

    GCupvalue* created_upvalue = func_newuv(T, local);
    created_upvalue->next = upvalue;

    if(prev_upvalue == NULL)
    {
        T->open_upvalues = created_upvalue;
    }
    else
    {
        prev_upvalue->next = created_upvalue;
    }

    return created_upvalue;
}

/* Close all open upvalues pointing to some stack level or above */
void tea_func_close(tea_State* T, TValue* last)
{
    while(T->open_upvalues != NULL && T->open_upvalues->location >= last)
    {
        GCupvalue* upvalue = T->open_upvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        T->open_upvalues = upvalue->next;
    }
}

/* -- Functions (closures) -------------------------------------------------- */

GCfunc* tea_func_newC(tea_State* T, CFuncType type, tea_CFunction fn, int nargs, int nupvalues)
{
    GCfunc* func = (GCfunc*)tea_obj_alloc(T, sizeCfunc(nupvalues), TEA_TFUNC);
    func->c.ffid = FF_C;
    func->c.upvalue_count = nupvalues;
    func->c.type = type;
    func->c.fn = fn;
    func->c.nargs = nargs;
    return func;
}

/* Create a new Teascript function with empty upvalues */
GCfunc* tea_func_newT(tea_State* T, GCproto* proto)
{
    GCupvalue** upvalues = tea_mem_new(T, GCupvalue*, proto->upvalue_count);
    for(int i = 0; i < proto->upvalue_count; i++)
    {
        upvalues[i] = NULL;
    }
    GCfunc* func = (GCfunc*)tea_obj_alloc(T, sizeof(GCfuncT), TEA_TFUNC);
    func->t.ffid = FF_TEA;
    func->t.upvalue_count = proto->upvalue_count;
    func->t.proto = proto;
    func->t.upvalues = upvalues;
    return func;
}