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

void TEA_FASTCALL tea_func_freeproto(tea_State* T, GCproto* pt)
{
    tea_mem_free(T, pt, pt->sizept);
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

void TEA_FASTCALL tea_func_freeuv(tea_State* T, GCupval* uv)
{
    tea_mem_freet(T, uv);
}

/* -- Functions (closures) -------------------------------------------------- */

GCfunc* tea_func_newC(tea_State* T, CFuncType type, tea_CFunction fn, int nupvalues, int nargs, int nopts)
{
    GCfunc* func = (GCfunc*)tea_mem_newgco(T, sizeCfunc(nupvalues), TEA_TFUNC);
    func->c.ffid = FF_C;
    func->c.upvalue_count = (uint8_t)nupvalues;
    func->c.module = NULL;
    func->c.type = type;
    func->c.fn = fn;
    func->c.nargs = nargs;
    func->c.nopts = nopts;
    return func;
}

/* Create a new Teascript function with empty upvalues */
GCfunc* tea_func_newT(tea_State* T, GCproto* pt, GCmodule* module)
{
    GCfunc* func = (GCfunc*)tea_mem_newgco(T, sizeTfunc(pt->sizeuv), TEA_TFUNC);
    func->t.ffid = FF_TEA;
    func->t.upvalue_count = pt->sizeuv;
    func->t.module = module;
    func->t.pt = pt;
    for(int i = 0; i < pt->sizeuv; i++)
    {
        func->t.upvalues[i] = NULL;
    }
    return func;
}

void TEA_FASTCALL tea_func_free(tea_State* T, GCfunc* fn)
{
    size_t size = isteafunc(fn) ? sizeTfunc(fn->t.upvalue_count) :
                sizeCfunc(fn->c.upvalue_count);
    tea_mem_free(T, fn, size);
}