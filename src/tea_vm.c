/*
** tea_vm.c
** Teascript virtual machine
*/

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define tea_vm_c
#define TEA_CORE

#include "tea_def.h"
#include "tea_obj.h"
#include "tea_func.h"
#include "tea_map.h"
#include "tea_str.h"
#include "tea_gc.h"
#include "tea_vm.h"
#include "tea_utf.h"
#include "tea_import.h"
#include "tea_err.h"
#include "tea_bc.h"
#include "tea_tab.h"
#include "tea_list.h"
#include "tea_strfmt.h"
#include "tea_meta.h"

static bool vm_callT(tea_State* T, GCfunc* f, int arg_count)
{
    GCfuncT* func = &f->t;
    if(arg_count < func->proto->arity)
    {
        if((arg_count + func->proto->variadic) == func->proto->arity)
        {
            /* Add missing variadic param ([]) */
            GClist* list = tea_list_new(T);
            setlistV(T, T->top++, list);
            arg_count++;
        }
        else
        {
            tea_err_run(T, TEA_ERR_ARGS, func->proto->arity, arg_count);
        }
    }
    else if(arg_count > func->proto->arity + func->proto->arity_optional)
    {
        if(func->proto->variadic)
        {
            int arity = func->proto->arity + func->proto->arity_optional;
            /* +1 for the variadic param itself */
            int varargs = arg_count - arity + 1;
            GClist* list = tea_list_new(T);
            setlistV(T, T->top++, list);
            for(int i = varargs; i > 0; i--)
            {
                tea_list_add(T, list, T->top - 1 - i);
            }
            /* +1 for the list pushed earlier on the stack */
            T->top -= varargs + 1;
            setlistV(T, T->top++, list);
            arg_count = arity;
        }
        else
        {
            tea_err_run(T, TEA_ERR_ARGS, func->proto->arity + func->proto->arity_optional, arg_count);
        }
    }
    else if(func->proto->variadic)
    {
        /* Last argument is the variadic arg */
        GClist* list = tea_list_new(T);
        setlistV(T, T->top++, list);
        tea_list_add(T, list, T->top - 1);
        T->top -= 2;
        setlistV(T, T->top++, list);
    }
    
    tea_state_growci(T);
    tea_state_checkstack(T, func->proto->max_slots);

    CallInfo* ci = ++T->ci; /* Enter new function */
    ci->func = f;
    ci->ip = func->proto->bc;
    ci->state = CIST_TEA;
    ci->base = T->top - arg_count - 1;

    return true;
}

#define iscci(T) \
    (((T)->ci->func != NULL) && \
    iscfunc((T)->ci->func) && \
    (T)->ci->func->c.type == C_FUNCTION)

static bool vm_callC(tea_State* T, GCfunc* f, int arg_count)
{
    GCfuncC* cfunc = &f->c;
    int extra = cfunc->type > C_FUNCTION;
    if(cfunc->nargs != TEA_VARARGS)
    {
        if((cfunc->nargs >= 0) && ((arg_count + extra) != cfunc->nargs))
        {
            tea_err_run(T, TEA_ERR_ARGS, cfunc->nargs, arg_count + extra);
        }
        else if((cfunc->nargs < 0) && ((arg_count + extra) > (-cfunc->nargs)))
        {
            tea_err_run(T, TEA_ERR_OPTARGS, -cfunc->nargs, arg_count + extra);
        }
    }

    tea_state_growci(T);
    tea_state_checkstack(T, TEA_STACK_START);

    CallInfo* ci = ++T->ci; /* Enter new function */
    ci->func = f;
    ci->ip = NULL;
    ci->state = CIST_C;
    ci->base = T->top - arg_count - 1;

    if(extra)
        T->base = T->top - arg_count - 1;
    else 
        T->base = T->top - arg_count;

    cfunc->fn(T);   /* Do the actual call */
    
    TValue* res = T->top - 1;
    ci = T->ci--;
    T->base = T->ci->base;
    if(iscci(T))
    {
        T->base++;
    }
    T->top = ci->base;
    copyTV(T, T->top++, res);
    return false;
}

bool vm_call(tea_State* T, GCfunc* func, int arg_count)
{
    if(isteafunc(func))
        return vm_callT(T, func, arg_count);
    else
        return vm_callC(T, func, arg_count);
}

bool vm_precall(tea_State* T, TValue* callee, uint8_t arg_count)
{
    switch(itype(callee))
    {
        case TEA_TMETHOD:
        {
            GCmethod* bound = methodV(callee);
            copyTV(T, T->top - arg_count - 1, &bound->receiver);
            return vm_call(T, bound->func, arg_count);
        }
        case TEA_TCLASS:
        {
            GCclass* klass = classV(callee);
            TValue* f = &klass->init;
            if(!tvisnil(f))
            {
                if(tvisfunc(f) && !iscfunc(funcV(f)))
                {
                    setinstanceV(T, T->top - arg_count - 1, tea_instance_new(T, klass));
                }
                else
                {
                    setclassV(T, T->top - arg_count - 1, klass);
                }
                return vm_precall(T, f, arg_count);
            }
            else if(arg_count != 0)
            {
                tea_err_run(T, TEA_ERR_NOARGS, arg_count);
            }
            else
            {
                setinstanceV(T, T->top - arg_count - 1, tea_instance_new(T, klass));
            }
            return false;
        }
        case TEA_TUDATA:
        case TEA_TINSTANCE:
        {
            TValue* mo = tea_meta_lookup(T, callee, MM_CALL);
            if(!mo) break;
            copyTV(T, callee, mo);   /* Fallback */
        }
        case TEA_TFUNC:
            return vm_call(T, funcV(callee), arg_count);
        default:
            break; /* Non-callable object type */
    }
    tea_err_run(T, TEA_ERR_CALL, tea_typename(callee));
    return false;
}

static bool vm_invoke_from_class(tea_State* T, GCclass* klass, GCstr* name, int arg_count)
{
    TValue* mo = tea_tab_get(&klass->methods, name);
    if(!mo)
    {
        tea_err_run(T, TEA_ERR_METHOD, str_data(name));
    }
    return vm_call(T, funcV(mo), arg_count);
}

static bool vm_invoke(tea_State* T, TValue* receiver, GCstr* name, int arg_count)
{
    switch(itype(receiver))
    {
        case TEA_TMODULE:
        {
            GCmodule* module = moduleV(receiver);
            TValue* o = tea_tab_get(&module->vars, name);
            if(o)
            {
                return vm_precall(T, o, arg_count);
            }
            tea_err_run(T, TEA_ERR_MODVAR, str_data(name), str_data(module->name));
        }
        case TEA_TUDATA:
        case TEA_TINSTANCE:
        {
            GCinstance* instance = instanceV(receiver);
            TValue* o = tea_tab_get(&instance->attrs, name);
            if(o)
            {
                copyTV(T, T->top - arg_count - 1, o);
                return vm_precall(T, o, arg_count);
            }
            return vm_invoke_from_class(T, instance->klass, name, arg_count);
        }
        default:
        {
            GCclass* type = tea_meta_getclass(T, receiver);
            if(type)
            {
                TValue* o = tea_tab_get(&type->methods, name);
                if(o)
                {
                    return vm_precall(T, o, arg_count);
                }
            }
            tea_err_run(T, TEA_ERR_NOMETHOD, tea_typename(receiver), str_data(name));
        }
    }
    return false;
}

static bool vm_bind_method(tea_State* T, GCclass* klass, GCstr* name)
{
    TValue* mo = tea_tab_get(&klass->methods, name);
    if(!mo)
    {
        tea_err_run(T, TEA_ERR_METHOD, str_data(name));
    }
    GCmethod* bound = tea_method_new(T, T->top - 1, funcV(mo));
    T->top--;
    setmethodV(T, T->top++, bound);
    return true;
}

static void vm_extend(tea_State* T, GClist* list, TValue* obj)
{
    switch(itype(obj))
    {
        case TEA_TRANGE:
        {
            GCrange* range = rangeV(obj);

            int32_t start = range->start;
            int32_t end = range->end;
            int32_t step = range->step;

            TValue n;
            if(step > 0)
            {
                for(int i = start; i < end; i += step)
                {
                    setnumV(&n, i);
                    tea_list_add(T, list, &n);
                }
            }
            else if(step < 0)
            {
                for(int i = end + step; i >= 0; i += step)
                {
                    setnumV(&n, i);
                    tea_list_add(T, list, &n);
                }
            }
            return;
        }
        case TEA_TLIST:
        {
            GClist* l = listV(obj);
            for(int i = 0; i < l->len; i++)
            {
                tea_list_add(T, list, list_slot(l, i));
            }
            return;
        }
        case TEA_TSTR:
        {
            GCstr* str = strV(obj);
            int len = tea_utf_len(str);
            for(int i = 0; i < len; i++)
            {
                GCstr* c = tea_utf_codepoint_at(T, str, tea_utf_char_offset(str_datawr(str), i));
                TValue v;
                setstrV(T, &v, c);
                tea_list_add(T, list, &v);
            }
            return;
        }
        default:
            break;
    }
    tea_err_run(T, TEA_ERR_ITER, tea_typename(obj));
}

static void vm_get_index(tea_State* T, TValue* index_value, TValue* obj, bool assign)
{
    switch(itype(obj))
    {
        case TEA_TUDATA:
        case TEA_TINSTANCE:
        {
            GCinstance* instance = instanceV(obj);
            TValue* mo = tea_meta_lookup(T, obj, MM_INDEX);
            if(mo)
            {
                if(!assign)
                {
                    copyTV(T, T->top++, obj);
                    copyTV(T, T->top++, index_value);
                }
                setnilV(T->top++);
                tea_vm_call(T, mo, 2);
                return;
            }
            tea_err_run(T, TEA_ERR_INSTSUBSCR, instance->klass->name);
        }
        case TEA_TRANGE:
        {
            if(!tvisnum(index_value))
            {
                tea_err_run(T, TEA_ERR_NUMRANGE);
            }

            GCrange* range = rangeV(obj);
            double index = numV(index_value);

            /* Calculate the length of the range */
            double len = (range->end - range->start) / range->step;

            /* Allow negative indexes */
            if(index < 0)
            {
                index = len + index;
            }

            if(index >= 0 && index < len)
            {
                if(assign)
                {
                    T->top -= 2;
                }
                setnumV(T->top++, range->start + index * range->step);
                return;
            }
            tea_err_run(T, TEA_ERR_IDXRANGE);
        }
        case TEA_TLIST:
        {
            if(!tvisnum(index_value) && !tvisrange(index_value))
            {
                tea_err_run(T, TEA_ERR_NUMLIST);
            }

            if(tvisrange(index_value))
            {
                GClist* l = tea_list_slice(T, listV(obj), rangeV(index_value));
                if(assign)
                {
                    T->top -= 2;
                }
                setlistV(T, T->top++, l);
                return;
            }

            GClist* list = listV(obj);
            int32_t index = numV(index_value);

            /* Allow negative indexes */
            if(index < 0)
            {
                index = list->len + index;
            }

            if(index >= 0 && index < list->len)
            {
                if(assign)
                {
                    T->top -= 2;
                }
                copyTV(T, T->top++, list_slot(list, index));
                return;
            }
            tea_err_run(T, TEA_ERR_IDXLIST);
        }
        case TEA_TMAP:
        {
            GCmap* map = mapV(obj);
            cTValue* o = tea_map_get(map, index_value);
            if(o)
            {
                if(assign)
                {
                    T->top -= 2;
                }
                copyTV(T, T->top++, o);
                return;
            }
            tea_err_run(T, TEA_ERR_MAPKEY);
        }
        case TEA_TSTR:
        {
            if(!tvisnum(index_value) && !tvisrange(index_value))
            {
                tea_err_run(T, TEA_ERR_NUMSTR, tea_typename(index_value));
            }

            if(tvisrange(index_value))
            {
                GCstr* s = tea_utf_slice(T, strV(obj), rangeV(index_value));
                if(assign)
                {
                    T->top -= 2;
                }
                setstrV(T, T->top++, s);
                return;
            }

            GCstr* str = strV(obj);
            int32_t index = numV(index_value);
            int32_t real_len = tea_utf_len(str);

            /* Allow negative indexes */
            if(index < 0)
            {
                index = real_len + index;
            }

            if(index >= 0 && index < real_len)
            {
                if(assign)
                {
                    T->top -= 2;
                }
                GCstr* c = tea_utf_codepoint_at(T, str, tea_utf_char_offset(str_datawr(str), index));
                setstrV(T, T->top++, c);
                return;
            }
            tea_err_run(T, TEA_ERR_IDXSTR);
        }
        default:
            break;
    }
    tea_err_run(T, TEA_ERR_SUBSCR, tea_typename(obj));
}

static void vm_set_index(tea_State* T, TValue* item_value, TValue* index_value, TValue* obj)
{
    switch(itype(obj))
    {
        case TEA_TUDATA:
        case TEA_TINSTANCE:
        {
            GCinstance* instance = instanceV(obj);
            TValue* mo = tea_meta_lookup(T, obj, MM_INDEX);
            if(mo)
            {             
                tea_vm_call(T, mo, 2);
                return;
            }
            tea_err_run(T, TEA_ERR_SETSUBSCR, instance->klass->name);
        }
        case TEA_TLIST:
        {
            if(!tvisnum(index_value))
            {
                tea_err_run(T, TEA_ERR_NUMLIST);
            }

            GClist* list = listV(obj);
            int32_t index = numV(index_value);

            if(index < 0)
            {
                index = list->len + index;
            }

            if(index >= 0 && index < list->len)
            {
                copyTV(T, list_slot(list, index), item_value);
                T->top -= 3;
                copyTV(T, T->top++, item_value);
                return;
            }
            tea_err_run(T, TEA_ERR_IDXLIST);
        }
        case TEA_TMAP:
        {
            GCmap* map = mapV(obj);
            copyTV(T, tea_map_set(T, map, index_value), item_value);
            T->top -= 3;
            copyTV(T, T->top++, item_value);
            return;
        }
        default:
            break;
    }
    tea_err_run(T, TEA_ERR_SETSUBSCR, tea_typename(obj));
}

static void vm_get_attr(tea_State* T, TValue* obj, GCstr* name, bool dopop)
{
    switch(itype(obj))
    {
        case TEA_TUDATA:
        case TEA_TINSTANCE:
        {
            GCinstance* instance = instanceV(obj);
            cTValue* o = tea_tab_get(&instance->attrs, name);
            if(o)
            {
                if(dopop)
                {
                    T->top--; /* Instance */
                }
                copyTV(T, T->top++, o);
                return;
            }

            if(vm_bind_method(T, instance->klass, name))
                return;

            tea_err_run(T, TEA_ERR_INSTATTR, str_data(instance->klass->name), str_data(name));
        }
        case TEA_TMODULE:
        {
            GCmodule* module = moduleV(obj);
            cTValue* o = tea_tab_get(&module->vars, name);
            if(o)
            {
                if(dopop)
                {
                    T->top--; /* Module */
                }
                copyTV(T, T->top++, o);
                return;
            }
            tea_err_run(T, TEA_ERR_MODATTR, str_data(module->name), str_data(name));
        }
        case TEA_TMAP:
        {
            GCmap* map = mapV(obj);
            cTValue* o = tea_map_getstr(T, map, name);
            if(o)
            {
                if(dopop)
                {
                    T->top--;
                }
                copyTV(T, T->top++, o);
                return;
            }
            else
            {
                goto retry;
            }
            tea_err_run(T, TEA_ERR_MAPATTR, str_data(name));
        }
        default:
retry:
        {
            GCclass* klass = tea_meta_getclass(T, obj);
            if(klass)
            {
                TValue* o = tea_tab_get(&klass->methods, name);
                if(o)
                {
                    if((tvisfunc(o) && iscfunc(funcV(o))) && 
                        funcV(o)->c.type == C_PROPERTY)
                    {
                        if(!dopop)
                        {
                            copyTV(T, T->top++, obj);
                        }
                        tea_vm_call(T, o, 0);
                    }
                    else
                    {
                        vm_bind_method(T, klass, name);
                    }
                    return;
                }
            }
            break;
        }
    }
    tea_err_run(T, TEA_ERR_NOATTR, tea_typename(obj), str_data(name));
}

static void vm_set_attr(tea_State* T, GCstr* name, TValue* obj, TValue* item)
{
    switch(itype(obj))
    {
        case TEA_TUDATA:
        case TEA_TINSTANCE:
        {
            GCinstance* instance = instanceV(obj);
            copyTV(T, tea_tab_set(T, &instance->attrs, name, NULL), item);
            T->top -= 2;
            copyTV(T, T->top++, item);
            return;
        }
        case TEA_TMAP:
        {
            GCmap* map = mapV(obj);
            copyTV(T, tea_map_setstr(T, map, name), item);
            T->top -= 2;
            copyTV(T, T->top++, item);
            return;
        }
        case TEA_TMODULE:
        {
            GCmodule* module = moduleV(obj);
            copyTV(T, tea_tab_set(T, &module->vars, name, NULL), item);
            T->top -= 2;
            copyTV(T, T->top++, item);
            return;
        }
        default:
        {
            GCclass* klass = tea_meta_getclass(T, obj);
            if(klass)
            {
                TValue* o = tea_tab_get(&klass->methods, name);
                if(o)
                {
                    if((tvisfunc(o) && iscfunc(funcV(o))) && 
                        funcV(o)->c.type == C_PROPERTY)
                    {
                        tea_vm_call(T, o, 1);
                        return;
                    }
                }
            }
            break;
        }
    }
    tea_err_run(T, TEA_ERR_SETATTR, tea_typename(obj));
}

static void vm_define_method(tea_State* T, GCstr* name)
{
    TValue* mo = T->top - 1;
    GCclass* klass = classV(T->top - 2);
    copyTV(T, tea_tab_set(T, &klass->methods, name, NULL), mo);
    if(name == T->init_str) copyTV(T, &klass->init, mo);
    T->top--;
}

static void vm_arith_unary(tea_State* T, MMS mm, TValue* o)
{
    TValue* mo = tea_meta_lookup(T, o, mm);
    if(!mo)
    {
        tea_err_run(T, TEA_ERR_UNOP, mmname_str(T, mm), tea_typename(o));
    }

    TValue tv;
    copyTV(T, &tv, o);

    T->top--;
    copyTV(T, T->top++, mo);
    copyTV(T, T->top++, &tv);
    setnilV(T->top++);
    tea_vm_call(T, mo, 2);
}

static void vm_arith(tea_State* T, MMS mm, TValue* a, TValue* b)
{
    TValue* mo = tea_meta_lookup(T, a, mm);    /* try first operand */
    if(!mo)
    {
        mo = tea_meta_lookup(T, b, mm); /* try second operand */
    }
    if(!mo)
    {
        tea_err_run(T, TEA_ERR_BIOP, mmname_str(T, mm), tea_typename(a), tea_typename(b));
    }

    TValue tv1, tv2;
    copyTV(T, &tv1, a);
    copyTV(T, &tv2, b);

    T->top -= 2;
    copyTV(T, T->top++, mo);
    copyTV(T, T->top++, &tv1);
    copyTV(T, T->top++, &tv2);
    tea_vm_call(T, mo, 2);
}

static bool vm_arith_comp(tea_State* T, MMS mm, TValue* a, TValue* b)
{
    TValue* mo = tea_meta_lookup(T, a, mm);    /* try first operand */
    if(!mo)
    {
        mo = tea_meta_lookup(T, b, mm); /* try second operand */
    }
    if(!mo)
    {
        return false;
    }

    TValue tv1, tv2;
    copyTV(T, &tv1, a);
    copyTV(T, &tv2, b);

    T->top -= 2;
    copyTV(T, T->top++, mo);
    copyTV(T, T->top++, &tv1);
    copyTV(T, T->top++, &tv2);
    tea_vm_call(T, mo, 2);
    return true;
}

static bool vm_iterator_call(tea_State* T, MMS mm, TValue* o)
{
    TValue* mo = tea_meta_lookup(T, o, mm);
    if(mo)
    {
        tea_vm_call(T, mo, 1);
        return true;
    }
    return false;
}

static void vm_execute(tea_State* T)
{
#define STORE_FRAME (T->ci->ip = ip)
#define READ_FRAME() \
    do \
    { \
	    ip = T->ci->ip; \
	    base = T->ci->base; \
    } \
    while(false)

#define READ_BYTE() (*ip++)
#define READ_SHORT() (ip += 2, (uint16_t)((ip[-2] << 8) | ip[-1]))
#define READ_CONSTANT() (proto_kgc(T->ci->func->t.proto, READ_BYTE()))
#define READ_STRING() strV(READ_CONSTANT())

#define RUNTIME_ERROR(...) \
    do \
    { \
        STORE_FRAME; \
        tea_err_run(T, __VA_ARGS__); \
        READ_FRAME(); \
        DISPATCH(); \
    } \
    while(false)

#define BINARY_OP(value_type, expr, op_method, type) \
    do \
    { \
        TValue* v1 = T->top - 2; \
        TValue* v2 = T->top - 1; \
        if(tvisnum(v1) && tvisnum(v2)) \
        { \
            type b = numV(--T->top); \
            type a = numV(T->top - 1); \
            value_type(T->top - 1, expr); \
        } \
        else \
        { \
            STORE_FRAME; \
            vm_arith(T, op_method, v1, v2); \
            READ_FRAME(); \
        } \
    } \
    while(false)

#define UNARY_OP(value_type, expr, op_method, type) \
    do \
    { \
        TValue* v1 = T->top - 1; \
        if(tvisnum(v1)) \
        { \
            type v = numV(v1); \
            value_type(T->top - 1, expr); \
        } \
        else \
        { \
            STORE_FRAME; \
            vm_arith_unary(T, op_method, v1); \
            READ_FRAME(); \
        } \
    } \
    while(false)

#ifdef TEA_COMPUTED_GOTO
    static void* dispatch_table[] = {
        #define BCGOTO(name, _) &&BC_##name,
        BCDEF(BCGOTO)
        #undef BCGOTO
    };

    #define DISPATCH() \
        do \
        { \
            goto *dispatch_table[instruction = READ_BYTE()]; \
        } \
        while(false)

    #define INTERPRET_LOOP  DISPATCH();
    #define CASE_CODE(name) name
#else
    #define INTERPRET_LOOP \
        loop: \
            switch(instruction = READ_BYTE())

    #define DISPATCH() goto loop

    #define CASE_CODE(name) case name
#endif

    uint8_t* ip;
    TValue* base;

    READ_FRAME();
    (T->ci - 1)->state = CIST_REENTRY;

    /* Main interpreter loop */
    while(true)
    {
        uint8_t instruction;
        INTERPRET_LOOP
        {
            CASE_CODE(BC_CONSTANT):
            {
                TValue* o = READ_CONSTANT();
                copyTV(T, T->top++, o);
                DISPATCH();
            }
            CASE_CODE(BC_NIL):
            {
                setnilV(T->top++);
                DISPATCH();
            }
            CASE_CODE(BC_TRUE):
            {
                settrueV(T->top++);
                DISPATCH();
            }
            CASE_CODE(BC_FALSE):
            {
                setfalseV(T->top++);
                DISPATCH();
            }
            CASE_CODE(BC_POP):
            {
                T->top--;
                DISPATCH();
            }
            CASE_CODE(BC_PRINT):
            {
                TValue* o = T->top - 1;
                if(!tvisnil(o))
                {
                    copyTV(T, tea_tab_set(T, &T->globals, T->repl_str, NULL), o);
                    if(tea_get_global(T, "print"))
                    {
                        copyTV(T, T->top++, o);
                        STORE_FRAME;
                        tea_vm_call(T, T->top - 2, 1);
                        READ_FRAME();
                        T->top--;
                    }
                }
                T->top--;
                DISPATCH();
            }
            CASE_CODE(BC_GET_LOCAL):
            {
                uint8_t slot = READ_BYTE();
                copyTV(T, T->top++, base + slot);
                DISPATCH();
            }
            CASE_CODE(BC_SET_LOCAL):
            {
                uint8_t slot = READ_BYTE();
                copyTV(T, base + slot, T->top - 1);
                DISPATCH();
            }
            CASE_CODE(BC_GET_MODULE):
            {
                GCstr* name = READ_STRING();
                TValue* o = tea_tab_get(&T->ci->func->t.module->vars, name);
                if(!o)
                {
                    RUNTIME_ERROR(TEA_ERR_VAR, str_data(name));
                }
                copyTV(T, T->top++, o);
                DISPATCH();
            }
            CASE_CODE(BC_SET_MODULE):
            {
                bool b;
                GCstr* name = READ_STRING();
                TValue* o = tea_tab_set(T, &T->ci->func->t.module->vars, name, &b);
                if(b)
                {
                    tea_tab_delete(&T->ci->func->t.module->vars, name);
                    RUNTIME_ERROR(TEA_ERR_VAR, str_data(name));
                }
                copyTV(T, o, T->top - 1);
                DISPATCH();
            }
            CASE_CODE(BC_DEFINE_OPTIONAL):
            {
                uint8_t arity = READ_BYTE();
                uint8_t arity_optional = READ_BYTE();
                int arg_count = T->top - base - arity_optional - 1;

                /*
                ** Temp array while we shuffle the stack
                ** Cannot have more than 255 args to a function, so
                ** we can define this with a constant limit
                */
                TValue values[255];
                int index;

                for(index = 0; index < arity_optional + arg_count; index++)
                {
                    TValue* v = --T->top;
                    copyTV(T, values + index, v);
                }

                --index;

                for(int i = 0; i < arg_count; i++)
                {
                    copyTV(T, T->top++, values + index - i);
                }

                /* Calculate how many "default" values are required */
                int remaining = arity + arity_optional - arg_count;

                /* Push any "default" values back onto the stack */
                for(int i = remaining; i > 0; i--)
                {
                    copyTV(T, T->top++, values + i - 1);
                }
                DISPATCH();
            }
            CASE_CODE(BC_DEFINE_MODULE):
            {
                GCstr* name = READ_STRING();
                TValue* o = tea_tab_set(T, &T->ci->func->t.module->vars, name, NULL);
                copyTV(T, o, T->top - 1);
                T->top--;
                DISPATCH();
            }
            CASE_CODE(BC_GET_UPVALUE):
            {
                uint8_t slot = READ_BYTE();
                TValue* v = T->ci->func->t.upvalues[slot]->location;
                copyTV(T, T->top++, v);
                DISPATCH();
            }
            CASE_CODE(BC_SET_UPVALUE):
            {
                uint8_t slot = READ_BYTE();
                TValue* v = T->ci->func->t.upvalues[slot]->location;
                copyTV(T, v, T->top - 1);
                DISPATCH();
            }
            CASE_CODE(BC_GET_ATTR):
            CASE_CODE(BC_PUSH_ATTR):
            {
                TValue* receiver = T->top - 1;
                GCstr* name = READ_STRING();
                bool dopop = instruction == BC_GET_ATTR;
                STORE_FRAME;
                vm_get_attr(T, receiver, name, dopop);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_SET_ATTR):
            {
                GCstr* name = READ_STRING();
                TValue* receiver = T->top - 2;
                TValue* item = T->top - 1;
                STORE_FRAME;
                vm_set_attr(T, name, receiver, item);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_GET_SUPER):
            {
                GCstr* name = READ_STRING();
                GCclass* superclass = classV(--T->top);
                STORE_FRAME;
                vm_bind_method(T, superclass, name);
                DISPATCH();
            }
            CASE_CODE(BC_RANGE):
            {
                TValue* c = --T->top;
                TValue* b = --T->top;
                TValue* a = --T->top;

                if(!tvisnum(a) || !tvisnum(b) || !tvisnum(c))
                {
                    RUNTIME_ERROR(TEA_ERR_RANGE);
                }

                GCrange* r = tea_range_new(T, numV(a), numV(b), numV(c));
                setrangeV(T, T->top++, r);
                DISPATCH();
            }
            CASE_CODE(BC_LIST):
            {
                GClist* list = tea_list_new(T);
                setlistV(T, T->top++, list);
                DISPATCH();
            }
            CASE_CODE(BC_UNPACK):
            {
                uint8_t var_count = READ_BYTE();

                if(!tvislist(T->top - 1))
                {
                    RUNTIME_ERROR(TEA_ERR_UNPACK);
                }

                GClist* list = listV(--T->top);

                if(var_count != list->len)
                {
                    if(var_count < list->len)
                    {
                        RUNTIME_ERROR(TEA_ERR_MAXUNPACK);
                    }
                    else
                    {
                        RUNTIME_ERROR(TEA_ERR_MINUNPACK);
                    }
                }

                for(int i = 0; i < list->len; i++)
                {
                    copyTV(T, T->top++, list_slot(list, i));
                }

                DISPATCH();
            }
            CASE_CODE(BC_UNPACK_REST):
            {
                uint8_t var_count = READ_BYTE();
                uint8_t rest_pos = READ_BYTE();

                if(!tvislist(T->top - 1))
                {
                    RUNTIME_ERROR(TEA_ERR_UNPACK);
                }

                GClist* list = listV(--T->top);

                if(var_count > list->len)
                {
                    RUNTIME_ERROR(TEA_ERR_MINUNPACK);
                }

                for(int i = 0; i < list->len; i++)
                {
                    if(i == rest_pos)
                    {
                        GClist* rest_list = tea_list_new(T);
                        setlistV(T, T->top++, rest_list);
                        int j;
                        for(j = i; j < list->len - (var_count - rest_pos) + 1; j++)
                        {
                            tea_list_add(T, rest_list, list_slot(list, j));
                        }
                        i = j - 1;
                    }
                    else
                    {
                        copyTV(T, T->top++, list_slot(list, i));
                    }
                }

                DISPATCH();
            }
            CASE_CODE(BC_LIST_EXTEND):
            {
                GClist* list = listV(T->top - 2);
                TValue* item = T->top - 1;
                STORE_FRAME;
                vm_extend(T, list, item);
                READ_FRAME();
                T->top--;
                DISPATCH();
            }
            CASE_CODE(BC_MAP):
            {
                GCmap* map = tea_map_new(T);
                setmapV(T, T->top++, map);
                DISPATCH();
            }
            CASE_CODE(BC_GET_INDEX):
            CASE_CODE(BC_PUSH_INDEX):
            {
                TValue* obj = T->top - 2;
                TValue* index = T->top - 1;
                bool assign = instruction == BC_GET_INDEX;
                STORE_FRAME;
                vm_get_index(T, index, obj, assign);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_SET_INDEX):
            {
                TValue* obj = T->top - 3;
                TValue* index = T->top - 2;
                TValue* item = T->top - 1;
                STORE_FRAME;
                vm_set_index(T, item, index, obj);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_LIST_ITEM):
            {
                GClist* list = listV(T->top - 2);
                cTValue* item = T->top - 1;
                tea_list_add(T, list, item);
                T->top--;
                DISPATCH();
            }
            CASE_CODE(BC_MAP_FIELD):
            {
                GCmap* map = mapV(T->top - 3);
                TValue* key = T->top - 2;
                TValue* o = T->top - 1;
                copyTV(T, tea_map_set(T, map, key), o);
                T->top -= 2;
                DISPATCH();
            }
            CASE_CODE(BC_IS):
            {
                TValue* instance = T->top - 2;
                TValue* klass = T->top - 1;

                if(!tvisclass(klass))
                {
                    RUNTIME_ERROR(TEA_ERR_IS);
                }

                GCclass* instance_klass = tea_meta_getclass(T, instance);
                if(instance_klass == NULL)
                {
                    T->top -= 2; /* Drop the instance and class */
                    setfalseV(T->top++);
                    DISPATCH();
                }

                GCclass* type = classV(klass);
                bool found = false;

                while(instance_klass != NULL)
                {
                    if(instance_klass == type)
                    {
                        found = true;
                        break;
                    }

                    instance_klass = (GCclass*)instance_klass->super;
                }

                T->top -= 2; /* Drop the instance and class */
                setboolV(T->top++, found);
                DISPATCH();
            }
            CASE_CODE(BC_IN):
            {
                TValue* value = T->top - 2;
                TValue* object = T->top - 1;

                TValue v1, v2;
                copyTV(T, &v1, object);
                copyTV(T, &v2, value);

                T->top -= 2;

                copyTV(T, T->top++, &v1);
                copyTV(T, T->top++, &v2);

                STORE_FRAME;
                if(!vm_iterator_call(T, MM_CONTAINS, &v1))
                {
                    RUNTIME_ERROR(TEA_ERR_ITER, tea_typename(&v1));
                }
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_EQUAL):
            {
                TValue* a = T->top - 2;
                TValue* b = T->top - 1;
                if((tvisinstance(a) || tvisinstance(b)) &&
                    (tvisudata(a) || tvisudata(b)))
                {
                    STORE_FRAME;
                    if(!vm_arith_comp(T, MM_EQ, a, b))
                    {
                        T->top -= 2;
                        setboolV(T->top++, tea_obj_equal(a, b));
                        DISPATCH();
                    }
                    READ_FRAME();
                    DISPATCH();
                }
                else
                {
                    T->top -= 2;
                    setboolV(T->top++, tea_obj_equal(a, b));
                }
                DISPATCH();
            }
            CASE_CODE(BC_GREATER):
            {
                BINARY_OP(setboolV, (a > b), MM_GT, double);
                DISPATCH();
            }
            CASE_CODE(BC_GREATER_EQUAL):
            {
                BINARY_OP(setboolV, (a >= b), MM_GE, double);
                DISPATCH();
            }
            CASE_CODE(BC_LESS):
            {
                BINARY_OP(setboolV, (a < b), MM_LT, double);
                DISPATCH();
            }
            CASE_CODE(BC_LESS_EQUAL):
            {
                BINARY_OP(setboolV, (a <= b), MM_LE, double);
                DISPATCH();
            }
            CASE_CODE(BC_ADD):
            {
                BINARY_OP(setnumV, (a + b), MM_PLUS, double);
                DISPATCH();
            }
            CASE_CODE(BC_SUBTRACT):
            {
                BINARY_OP(setnumV, (a - b), MM_MINUS, double);
                DISPATCH();
            }
            CASE_CODE(BC_MULTIPLY):
            {
                BINARY_OP(setnumV, (a * b), MM_MULT, double);
                DISPATCH();
            }
            CASE_CODE(BC_DIVIDE):
            {
                BINARY_OP(setnumV, (a / b), MM_DIV, double);
                DISPATCH();
            }
            CASE_CODE(BC_MOD):
            {
                BINARY_OP(setnumV, (fmod(a, b)), MM_MOD, double);
                DISPATCH();
            }
            CASE_CODE(BC_POW):
            {
                BINARY_OP(setnumV, (pow(a, b)), MM_POW, double);
                DISPATCH();
            }
            CASE_CODE(BC_BAND):
            {
                BINARY_OP(setnumV, (a & b), MM_BAND, uint32_t);
                DISPATCH();
            }
            CASE_CODE(BC_BOR):
            {
                BINARY_OP(setnumV, (a | b), MM_BOR, uint32_t);
                DISPATCH();
            }
            CASE_CODE(BC_BNOT):
            {
                UNARY_OP(setnumV, (~v), MM_BNOT, uint32_t);
                DISPATCH();
            }
            CASE_CODE(BC_BXOR):
            {
                BINARY_OP(setnumV, (a ^ b), MM_BXOR, uint32_t);
                DISPATCH();
            }
            CASE_CODE(BC_LSHIFT):
            {
                BINARY_OP(setnumV, (a << b), MM_LSHIFT, uint32_t);
                DISPATCH();
            }
            CASE_CODE(BC_RSHIFT):
            {
                BINARY_OP(setnumV, (a >> b), MM_RSHIFT, uint32_t);
                DISPATCH();
            }
            CASE_CODE(BC_NOT):
            {
                bool b = tea_obj_isfalse(--T->top);
                setboolV(T->top++, b);
                DISPATCH();
            }
            CASE_CODE(BC_NEGATE):
            {
                UNARY_OP(setnumV, (-v), MM_MINUS, double);
                DISPATCH();
            }
            CASE_CODE(BC_MULTI_CASE):
            {
                uint8_t count = READ_BYTE();
                TValue* switch_value = T->top - 1 - (count + 1);
                TValue* case_value = --T->top;
                for(int i = 0; i < count; i++)
                {
                    if(tea_obj_equal(switch_value, case_value))
                    {
                        i++;
                        while(i <= count)
                        {
                            T->top--;
                            i++;
                        }
                        break;
                    }
                    case_value = --T->top;
                }
                copyTV(T, T->top++, case_value);
                DISPATCH();
            }
            CASE_CODE(BC_COMPARE_JUMP):
            {
                uint16_t offset = READ_SHORT();
                TValue* a = --T->top;
                if(!tea_obj_equal(T->top - 1, a))
                {
                    ip += offset;
                }
                else
                {
                    T->top--;
                }
                DISPATCH();
            }
            CASE_CODE(BC_JUMP):
            {
                uint16_t offset = READ_SHORT();
                ip += offset;
                DISPATCH();
            }
            CASE_CODE(BC_JUMP_IF_FALSE):
            {
                uint16_t offset = READ_SHORT();
                if(tea_obj_isfalse(T->top - 1))
                {
                    ip += offset;
                }
                DISPATCH();
            }
            CASE_CODE(BC_JUMP_IF_NIL):
            {
                uint16_t offset = READ_SHORT();
                if(tvisnil(T->top - 1))
                {
                    ip += offset;
                }
                DISPATCH();
            }
            CASE_CODE(BC_LOOP):
            {
                uint16_t offset = READ_SHORT();
                ip -= offset;
                DISPATCH();
            }
            CASE_CODE(BC_CALL):
            {
                uint8_t arg_count = READ_BYTE();
                STORE_FRAME;
                if(vm_precall(T, T->top - 1 - arg_count, arg_count))
                {
                    (T->ci - 1)->state = (CIST_TEA | CIST_CALLING);
                }
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_INVOKE):
            {
                GCstr* method = READ_STRING();
                uint8_t arg_count = READ_BYTE();
                STORE_FRAME;
                if(vm_invoke(T, T->top - 1 - arg_count, method, arg_count))
                {
                    (T->ci - 1)->state = (CIST_TEA | CIST_CALLING);
                }
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_SUPER):
            {
                GCstr* method = READ_STRING();
                uint8_t arg_count = READ_BYTE();
                GCclass* superclass = classV(--T->top);
                STORE_FRAME;
                if(vm_invoke_from_class(T, superclass, method, arg_count))
                {
                    (T->ci - 1)->state = (CIST_TEA | CIST_CALLING);
                }
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_CLOSURE):
            {
                GCproto* proto = protoV(READ_CONSTANT());
                GCfunc* func = tea_func_newT(T, proto, T->ci->func->t.module);
                setfuncV(T, T->top++, func);
                for(int i = 0; i < func->t.upvalue_count; i++)
                {
                    uint8_t is_local = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if(is_local)
                    {
                        func->t.upvalues[i] = tea_func_finduv(T, base + index);
                    }
                    else
                    {
                        func->t.upvalues[i] = T->ci->func->t.upvalues[index];
                    }
                }
                DISPATCH();
            }
            CASE_CODE(BC_CLOSE_UPVALUE):
            {
                tea_func_closeuv(T, T->top - 1);
                T->top--;
                DISPATCH();
            }
            CASE_CODE(BC_RETURN):
            {
                TValue* result = --T->top;
                tea_func_closeuv(T, base);
                STORE_FRAME;
                T->ci--;
                T->base = T->ci->base;
                T->top = base;
                copyTV(T, T->top++, result);
                if(!(T->ci->state & CIST_CALLING))
                {
                    if(iscci(T))
                    {
                        T->base++;
                    }
                    return;
                }
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_GET_ITER):
            {
                uint8_t slot1 = READ_BYTE();    /* seq */
                uint8_t slot2 = READ_BYTE();    /* iter */

                TValue* seq = base + slot1;
                TValue* iter = base + slot2;

                copyTV(T, T->top++, seq);
                copyTV(T, T->top++, iter);

                /* iterate */
                STORE_FRAME;
                if(!vm_iterator_call(T, MM_ITER, seq))
                {
                    RUNTIME_ERROR(TEA_ERR_ITER, tea_typename(seq));
                }
                READ_FRAME();
                copyTV(T, iter, T->top - 1);
                DISPATCH();
            }
            CASE_CODE(BC_FOR_ITER):
            {
                uint8_t slot1 = READ_BYTE();    /* seq */
                uint8_t slot2 = READ_BYTE();    /* iter */

                TValue* seq = base + slot1;
                TValue* iter = base + slot2;

                copyTV(T, T->top++, seq);
                copyTV(T, T->top++, iter);

                /* iteratorvalue */
                STORE_FRAME;
                if(!vm_iterator_call(T, MM_NEXT, seq))
                {
                    RUNTIME_ERROR(TEA_ERR_ITER, tea_typename(seq));
                }
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_CLASS):
            {
                GCclass* k = tea_class_new(T, READ_STRING());
                setclassV(T, T->top++, k);
                DISPATCH();
            }
            CASE_CODE(BC_INHERIT):
            {
                TValue* super = T->top - 2;

                if(!tvisclass(super))
                {
                    RUNTIME_ERROR(TEA_ERR_SUPER);
                }

                GCclass* superclass = classV(super);
                if(tea_meta_isclass(T, superclass))
                {
                    RUNTIME_ERROR(TEA_ERR_BUILTINSELF, str_data(superclass->name));
                }

                GCclass* klass = classV(T->top - 1);
                if(klass == superclass)
                {
                    RUNTIME_ERROR(TEA_ERR_SELF);
                }
                klass->super = superclass;
                klass->init = superclass->init;

                tea_tab_merge(T, &superclass->methods, &klass->methods);
                T->top--;
                DISPATCH();
            }
            CASE_CODE(BC_METHOD):
            {
                vm_define_method(T, READ_STRING());
                DISPATCH();
            }
            CASE_CODE(BC_EXTENSION_METHOD):
            {
                if(!tvisclass(T->top - 2))
                {
                    RUNTIME_ERROR(TEA_ERR_EXTMETHOD, tea_typename(T->top - 2));
                }
                vm_define_method(T, READ_STRING());
                T->top--;
                DISPATCH();
            }
            CASE_CODE(BC_IMPORT_STRING):
            {
                GCstr* path_name = READ_STRING();
                STORE_FRAME;
                tea_imp_relative(T, T->ci->func->t.module->path, path_name);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_IMPORT_NAME):
            {
                GCstr* name = READ_STRING();
                STORE_FRAME;
                tea_imp_logical(T, name);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_IMPORT_VARIABLE):
            {
                GCstr* variable = READ_STRING();
                TValue* module_variable = tea_tab_get(&T->last_module->vars, variable);
                if(!module_variable)
                {
                    RUNTIME_ERROR(TEA_ERR_VARMOD, str_data(variable), str_data(T->last_module->name));
                }
                copyTV(T, T->top++, module_variable);
                DISPATCH();
            }
            CASE_CODE(BC_IMPORT_ALIAS):
            {
                setmoduleV(T, T->top++, T->last_module);
                DISPATCH();
            }
            CASE_CODE(BC_IMPORT_END):
            {
                T->last_module = T->ci->func->t.module;
                DISPATCH();
            }
            CASE_CODE(BC_END):
            {
                DISPATCH();
            }
        }
    }
}
#undef STORE_FRAME
#undef READ_FRAME
#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
#undef BINARY_OP_FUNCTION
#undef RUNTIME_ERROR
#undef iscci

int tea_vm_pcall(tea_State* T, tea_CPFunction func, void* u, ptrdiff_t old_top)
{
    int oldnccalls = T->nccalls;
    ptrdiff_t old_ci = ci_save(T, T->ci);
    int status = tea_err_protected(T, func, u);
    if(status != TEA_OK)    /* An error occurred? */
    {
        TValue* old = stack_restore(T, old_top);
        T->open_upvalues = NULL;
        T->top = old;
        setnilV(T->top++);
        T->nccalls = oldnccalls;
        T->ci = ci_restore(T, old_ci);
        T->base = T->ci->base;

        /* Correct the stack */
        T->stack_max = T->stack + T->stack_size - 1;
        if(T->ci_size > TEA_MAX_CALLS)
        {
            int inuse = T->ci - T->ci_base;
            if(inuse + 1 < TEA_MAX_CALLS)
            {
                tea_state_reallocci(T, TEA_MAX_CALLS);
            }
        }
    }
    return status;
}

void tea_vm_call(tea_State* T, TValue* func, int arg_count)
{
    if(++T->nccalls >= TEA_MAX_CCALLS)
    {
        tea_err_run(T, TEA_ERR_STKOV);
    }

    if(vm_precall(T, func, arg_count))
    {
        vm_execute(T);
    }
    T->nccalls--;
}