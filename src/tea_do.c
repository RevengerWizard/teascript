/*
** tea_do.c
** Stack and Call structure of Teascript
*/

#include <setjmp.h>
#include <stdlib.h>

#define tea_do_c
#define TEA_CORE

#include "tea_def.h"
#include "tea_do.h"
#include "tea_func.h"
#include "tea_vm.h"
#include "tea_parser.h"

struct tea_longjmp
{
    struct tea_longjmp* previous;
    jmp_buf buf;
    volatile int status;
};

void tea_do_realloc_ci(TeaState* T, int new_size)
{
    TeaCallInfo* old_ci = T->base_ci;
    T->base_ci = TEA_GROW_ARRAY(T, TeaCallInfo, T->base_ci, T->ci_size, new_size);
    T->ci_size = new_size;
    T->ci = (T->ci - old_ci) + T->base_ci;
    T->end_ci = T->base_ci + T->ci_size;
}

static void grow_ci(TeaState* T)
{
    if(T->ci + 1 == T->end_ci)
    {
        tea_do_realloc_ci(T, T->ci_size * 2);
    }
    if(T->ci_size > TEA_MAX_CALLS)
    {
        tea_vm_error(T, "Stack overflow");
    }
}

static void correct_stack(TeaState* T, TeaValue* old_stack)
{
    T->top = (T->top - old_stack) + T->stack;

    for(TeaOUpvalue* upvalue = T->open_upvalues; upvalue != NULL; upvalue = upvalue->next)
    {
        upvalue->location = (upvalue->location - old_stack) + T->stack;
    }

    for(TeaCallInfo* ci = T->base_ci; ci <= T->ci; ci++)
    {
        ci->base = (ci->base - old_stack) + T->stack;
    }

    T->base = (T->base - old_stack) + T->stack;
}

void tea_do_realloc_stack(TeaState* T, int new_size)
{
	TeaValue* old_stack = T->stack;
	T->stack = TEA_GROW_ARRAY(T, TeaValue, T->stack, T->stack_size, new_size);
	T->stack_size = new_size;
    T->stack_last = T->stack + new_size - 1 - EXTRA_STACK;

    if(old_stack != T->stack)
    {
        correct_stack(T, old_stack);
    }
}

void tea_do_grow_stack(TeaState* T, int needed)
{
	if(needed <= T->stack_size)
        tea_do_realloc_stack(T, 2 * T->stack_size);
    else
        tea_do_realloc_stack(T, T->stack_size + needed + EXTRA_STACK);
}

static bool callt(TeaState* T, TeaOClosure* closure, int arg_count)
{
    if(arg_count < closure->function->arity)
    {
        if((arg_count + closure->function->variadic) == closure->function->arity)
        {
            /* add missing variadic param ([]) */
            TeaOList* list = tea_obj_new_list(T);
            tea_vm_push(T, OBJECT_VAL(list));
            arg_count++;
        }
        else
        {
            tea_vm_error(T, "Expected %d arguments, but got %d", closure->function->arity, arg_count);
        }
    }
    else if(arg_count > closure->function->arity + closure->function->arity_optional)
    {
        if(closure->function->variadic)
        {
            int arity = closure->function->arity + closure->function->arity_optional;
            /* +1 for the variadic param itself */
            int varargs = arg_count - arity + 1;
            TeaOList* list = tea_obj_new_list(T);
            tea_vm_push(T, OBJECT_VAL(list));
            for(int i = varargs; i > 0; i--)
            {
                tea_write_value_array(T, &list->items, tea_vm_peek(T, i));
            }
            /* +1 for the list pushed earlier on the stack */
            T->top -= varargs + 1;
            tea_vm_push(T, OBJECT_VAL(list));
            arg_count = arity;
        }
        else
        {
            tea_vm_error(T, "Expected %d arguments, but got %d", closure->function->arity + closure->function->arity_optional, arg_count);
        }
    }
    else if(closure->function->variadic)
    {
        /* last argument is the variadic arg */
        TeaOList* list = tea_obj_new_list(T);
        tea_vm_push(T, OBJECT_VAL(list));
        tea_write_value_array(T, &list->items, tea_vm_peek(T, 1));
        T->top -= 2;
        tea_vm_push(T, OBJECT_VAL(list));
    }
    
    grow_ci(T);
    tea_do_checkstack(T, closure->function->max_slots);

    TeaCallInfo* ci = ++T->ci;
    ci->closure = closure;
    ci->native = NULL;
    ci->ip = closure->function->chunk.code;
    ci->state = CIST_TEA;
    ci->base = T->top - arg_count - 1;

    return true;
}

static bool callc(TeaState* T, TeaONative* native, int arg_count)
{
    grow_ci(T);
    tea_do_checkstack(T, BASE_STACK_SIZE);

    TeaCallInfo* ci = ++T->ci;
    ci->closure = NULL;
    ci->native = native;
    ci->ip = NULL;
    ci->state = CIST_C;
    ci->base = T->top - arg_count - 1;

    if(native->type > NATIVE_FUNCTION)
        T->base = T->top - arg_count - 1;
    else 
        T->base = T->top - arg_count;

    native->fn(T);
    
    TeaValue res = T->top[-1];

    ci = T->ci--;

    T->base = ci->base;
    T->top = ci->base;
    tea_vm_push(T, res);

    return false;
}

bool tea_do_precall(TeaState* T, TeaValue callee, uint8_t arg_count)
{
    if(IS_OBJECT(callee))
    {
        switch(OBJECT_TYPE(callee))
        {
            case OBJ_BOUND_METHOD:
            {
                TeaOBoundMethod* bound = AS_BOUND_METHOD(callee);
                T->top[-arg_count - 1] = bound->receiver;
                return tea_do_precall(T, bound->method, arg_count);
            }
            case OBJ_CLASS:
            {
                TeaOClass* klass = AS_CLASS(callee);
                T->top[-arg_count - 1] = OBJECT_VAL(tea_obj_new_instance(T, klass));
                if(!IS_NULL(klass->constructor)) 
                {
                    return tea_do_precall(T, klass->constructor, arg_count);
                }
                else if(arg_count != 0)
                {
                    tea_vm_error(T, "Expected 0 arguments but got %d", arg_count);
                }
                return false;
            }
            case OBJ_CLOSURE:
                return callt(T, AS_CLOSURE(callee), arg_count);
            case OBJ_NATIVE:
                return callc(T, AS_NATIVE(callee), arg_count);
            default:
                break; /* Non-callable object type */
        }
    }

    tea_vm_error(T, "'%s' is not callable", tea_val_type(callee));
    return false;
}

struct PCall
{
    TeaValue func;
    int arg_count;
};

static void f_call(TeaState* T, void* ud)
{
    struct PCall* c = (struct PCall*)ud;
    tea_do_call(T, c->func, c->arg_count);
}

void tea_do_call(TeaState* T, TeaValue func, int arg_count)
{
    if(++T->nccalls >= TEA_MAX_CCALLS)
    {
        tea_vm_error(T, "C stack overflow");
    }

    if(tea_do_precall(T, func, arg_count))
    {
        tea_vm_run(T);
    }
    T->nccalls--;
}

static void restore_stack_limit(TeaState* T)
{
    T->stack_last = T->stack + T->stack_size - 1;
    if(T->ci_size > TEA_MAX_CALLS)
    {
        int inuse = (T->ci - T->base_ci);
        if(inuse + 1 < TEA_MAX_CALLS)
        {
            tea_do_realloc_ci(T, TEA_MAX_CALLS);
        }
    }
}

int tea_do_pcall(TeaState* T, ptrdiff_t old_top, TeaValue func, int arg_count)
{
    struct PCall c;
    c.func = func;
    c.arg_count = arg_count;
    
    int oldnccalls = T->nccalls;
    ptrdiff_t old_ci = saveci(T, T->ci);
    int status = tea_do_runprotected(T, f_call, &c);
    if(status != TEA_OK)    /* An error occurred? */
    {
        TeaValue* old = restorestack(T, old_top);
        T->open_upvalues = NULL;
        T->nccalls = oldnccalls;
        T->ci = restoreci(T, old_ci);
        T->base = T->ci->base;
        T->top = old;
        tea_vm_push(T, NULL_VAL);
        restore_stack_limit(T);
    }
    return status;
}

void tea_do_throw(TeaState* T, int code)
{
    if(T->error_jump)
    {
        T->error_jump->status = code;
        TEA_THROW(T);
    }
    else
    {
        T->panic(T);
        exit(EXIT_FAILURE);
    }
}

int tea_do_runprotected(TeaState* T, TeaPFunction f, void* ud)
{
    int old_nccalls = T->nccalls;
    struct tea_longjmp tj;
    tj.status = TEA_OK;
    tj.previous = T->error_jump;
    T->error_jump = &tj;
    TEA_TRY(T, &tj,
        (*f)(T, ud);
    );
    T->error_jump = tj.previous;
    T->nccalls = old_nccalls;
    return tj.status;
}

struct PParser
{
    TeaOModule* module;
    const char* source;
};

static void f_parser(TeaState* T, void* ud)
{
    struct PParser* c;
    TeaOFunction* function;
    TeaOClosure* closure;

    c = (struct PParser*)(ud);

    function = tea_parse(T, c->module, c->source);
    closure = tea_func_new_closure(T, function);
    tea_vm_push(T, OBJECT_VAL(closure));
}

int tea_do_protectedparser(TeaState* T, TeaOModule* module, const char* source)
{
    struct PParser c;
    int status;
    c.module = module;
    c.source = source;
    status = tea_do_runprotected(T, f_parser, &c);
    return status;
}