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

GCproto* tea_func_newproto(tea_State* T, int max_slots)
{
    GCproto* pt = tea_mem_newobj(T, GCproto, TEA_TPROTO);
    pt->numparams = 0;
    pt->numopts = 0;
    pt->variadic = 0;
    pt->upvalue_count = 0;
    pt->max_slots = max_slots;
    pt->name = NULL;
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
        if(instruction < line->ofs)
        {
            end = mid - 1;
        }
        else if(mid == f->line_count - 1 || instruction < f->lines[mid + 1].ofs)
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
static GCupval* func_newuv(tea_State* T, TValue* slot)
{
    GCupval* uv = tea_mem_newobj(T, GCupval, TEA_TUPVAL);
    setnilV(&uv->closed);
    uv->location = slot;
    uv->next = NULL;
    return uv;
}

/* Find existing open upvalue for a stack slot or create a new one */
GCupval* tea_func_finduv(tea_State* T, TValue* local)
{
    GCupval* prev_uv = NULL;
    GCupval* uv = T->open_upvalues;
    while(uv != NULL && uv->location > local)
    {
        prev_uv = uv;
        uv = uv->next;
    }

    if(uv != NULL && uv->location == local)
    {
        return uv;
    }

    GCupval* new_uv = func_newuv(T, local);
    new_uv->next = uv;

    if(prev_uv == NULL)
    {
        T->open_upvalues = new_uv;
    }
    else
    {
        prev_uv->next = new_uv;
    }
    return new_uv;
}

/* Close all open upvalues pointing to some stack level or above */
void tea_func_closeuv(tea_State* T, TValue* last)
{
    while(T->open_upvalues != NULL && T->open_upvalues->location >= last)
    {
        GCupval* uv = T->open_upvalues;
        uv->closed = *uv->location;
        uv->location = &uv->closed;
        T->open_upvalues = uv->next;
    }
}

/* -- Functions (closures) -------------------------------------------------- */

GCfunc* tea_func_newC(tea_State* T, CFuncType type, tea_CFunction fn, int nargs, int nupvalues)
{
    GCfunc* func = (GCfunc*)tea_mem_newgco(T, sizeCfunc(nupvalues), TEA_TFUNC);
    func->c.ffid = FF_C;
    func->c.upvalue_count = (uint8_t)nupvalues;
    func->c.module = NULL;
    func->c.type = type;
    func->c.fn = fn;
    func->c.nargs = nargs;
    return func;
}

/* Create a new Teascript function with empty upvalues */
GCfunc* tea_func_newT(tea_State* T, GCproto* proto, GCmodule* module)
{
    GCfunc* func = (GCfunc*)tea_mem_newgco(T, sizeTfunc(proto->upvalue_count), TEA_TFUNC);
    func->t.ffid = FF_TEA;
    func->t.upvalue_count = proto->upvalue_count;
    func->t.module = module;
    func->t.proto = proto;
    for(int i = 0; i < proto->upvalue_count; i++)
    {
        func->t.upvalues[i] = NULL;
    }
    return func;
}

void tea_func_free(tea_State* T, GCfunc* fn)
{
    size_t size = isteafunc(fn) ? sizeTfunc(fn->t.upvalue_count) :
                sizeCfunc(fn->c.upvalue_count);
    tea_mem_free(T, fn, size);
}