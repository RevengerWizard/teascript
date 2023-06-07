/*
** tea_func.c
** Teascript closures, functions and upvalues
*/

#define tea_func_c
#define TEA_CORE

#include "tea_state.h"
#include "tea_vm.h"
#include "tea_chunk.h"

TeaObjectNative* tea_func_new_native(TeaState* T, TeaNativeType type, TeaCFunction fn)
{
    TeaObjectNative* native = ALLOCATE_OBJECT(T, TeaObjectNative, OBJ_NATIVE);
    native->type = type;
    native->fn = fn;

    return native;
}

TeaObjectClosure* tea_func_new_closure(TeaState* T, TeaObjectFunction* function)
{
    TeaObjectUpvalue** upvalues = TEA_ALLOCATE(T, TeaObjectUpvalue*, function->upvalue_count);
    for(int i = 0; i < function->upvalue_count; i++)
    {
        upvalues[i] = NULL;
    }

    TeaObjectClosure* closure = ALLOCATE_OBJECT(T, TeaObjectClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalue_count = function->upvalue_count;

    return closure;
}

TeaObjectFunction* tea_func_new_function(TeaState* T, TeaFunctionType type, TeaObjectModule* module, int max_slots)
{
    TeaObjectFunction* function = ALLOCATE_OBJECT(T, TeaObjectFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->arity_optional = 0;
    function->variadic = 0;
    function->upvalue_count = 0;
    function->max_slots = max_slots;
    function->type = type;
    function->name = NULL;
    function->module = module;
    tea_chunk_init(&function->chunk);

    return function;
}

TeaObjectUpvalue* tea_func_new_upvalue(TeaState* T, TeaValue* slot)
{
    TeaObjectUpvalue* upvalue = ALLOCATE_OBJECT(T, TeaObjectUpvalue, OBJ_UPVALUE);
    upvalue->closed = NULL_VAL;
    upvalue->location = slot;
    upvalue->next = NULL;

    return upvalue;
}

TeaObjectUpvalue* tea_func_capture_upvalue(TeaState* T, TeaValue* local)
{
    TeaObjectUpvalue* prev_upvalue = NULL;
    TeaObjectUpvalue* upvalue = T->open_upvalues;
    while(upvalue != NULL && upvalue->location > local)
    {
        prev_upvalue = upvalue;
        upvalue = upvalue->next;
    }

    if(upvalue != NULL && upvalue->location == local)
    {
        return upvalue;
    }

    TeaObjectUpvalue* created_upvalue = tea_func_new_upvalue(T, local);
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

void tea_func_close_upvalues(TeaState* T, TeaValue* last)
{
    while(T->open_upvalues != NULL && T->open_upvalues->location >= last)
    {
        TeaObjectUpvalue* upvalue = T->open_upvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        T->open_upvalues = upvalue->next;
    }
}