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

static int vm_argcheck(tea_State* T, int nargs, int numparams, int numops, int variadic)
{
    if(TEA_UNLIKELY(nargs < numparams))
    {
        if((nargs + variadic) == numparams)
        {
            /* Add missing variadic param ([]) */
            GClist* list = tea_list_new(T);
            setlistV(T, T->top++, list);
            nargs++;
        }
        else
        {
            tea_err_callerv(T, TEA_ERR_ARGS, numparams, nargs);
        }
    }
    else if(TEA_UNLIKELY(nargs > numparams + numops))
    {
        if(variadic)
        {
            int xargs = numparams + numops;
            /* +1 for the variadic param itself */
            int varargs = nargs - xargs + 1;
            GClist* list = tea_list_new(T);
            setlistV(T, T->top++, list);
            for(int i = varargs; i > 0; i--)
            {
                tea_list_add(T, list, T->top - 1 - i);
            }
            /* +1 for the list pushed earlier on the stack */
            T->top -= varargs + 1;
            setlistV(T, T->top++, list);
            nargs = xargs;
        }
        else
        {
            tea_err_callerv(T, TEA_ERR_ARGS, numparams + numops, nargs);
        }
    }
    else if(variadic)
    {
        /* Last argument is the variadic arg */
        GClist* list = tea_list_new(T);
        setlistV(T, T->top++, list);
        tea_list_add(T, list, T->top - 2);
        T->top -= 2;
        setlistV(T, T->top++, list);
    }
    return nargs;
}

static bool vm_call(tea_State* T, GCfunc* func, int nargs)
{
    if(isteafunc(func))
    {
        GCfuncT* f = &func->t;
        nargs = vm_argcheck(T, nargs, f->pt->numparams, f->pt->numopts, 
                    f->pt->flags == PROTO_VARARG);
        tea_state_checkstack(T, f->pt->max_slots);
        CallInfo* ci = tea_state_growci(T); /* Enter new function */
        ci->func = func;
        ci->ip = proto_bc(f->pt);
        ci->state = CIST_TEA;
        ci->base = T->top - nargs - 1;
        return true;
    }
    else
    {
        GCfuncC* cf = &func->c;
        int extra = cf->type > C_FUNCTION;
        if(cf->nargs != TEA_VARG)
        {
            vm_argcheck(T, nargs, cf->nargs - extra, cf->nopts, 0);
        }
        tea_state_checkstack(T, TEA_STACK_START);
        CallInfo* ci = tea_state_growci(T); /* Enter new function */
        ci->func = func;
        ci->ip = NULL;
        ci->state = CIST_C;
        ci->base = T->top - nargs - 1;
        
        /* -1 if it's a C method */
        T->base = T->top - nargs - extra;

        cf->fn(T);   /* Do the actual call */
        
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
}

static bool vm_precall(tea_State* T, TValue* callee, int nargs)
{
    switch(itype(callee))
    {
        case TEA_TMETHOD:
        {
            GCmethod* bound = methodV(callee);
            copyTV(T, T->top - nargs - 1, &bound->receiver);
            return vm_call(T, bound->func, nargs);
        }
        case TEA_TUDATA:
        case TEA_TINSTANCE:
        {
            TValue* mo = tea_meta_lookup(T, callee, MM_CALL);
            if(!mo) break;
            copyTV(T, callee, mo);   /* Fallback */
        }
        case TEA_TFUNC:
            return vm_call(T, funcV(callee), nargs);
        default:
            break; /* Non-callable object type */
    }
    tea_err_callerv(T, TEA_ERR_CALL, tea_typename(callee));
    return false;   /* Unreachable */
}

static bool vm_invoke(tea_State* T, TValue* receiver, GCstr* name, int nargs)
{
    switch(itype(receiver))
    {
        case TEA_TCLASS:
        {
            GCclass* type = classV(receiver);
            TValue* o = tea_tab_get(&type->methods, name);
            if(tvisfunc(o) && iscfunc(funcV(o)) && funcV(o)->c.type == C_FUNCTION)
            {
                return vm_precall(T, o, nargs);
            }
            tea_err_callerv(T, TEA_ERR_NOMETHOD, tea_typename(receiver), str_data(name));
        }
        case TEA_TMODULE:
        {
            GCmodule* module = moduleV(receiver);
            TValue* o = tea_tab_get(&module->exports, name);
            if(o)
            {
                return vm_precall(T, o, nargs);
            }
            tea_err_callerv(T, TEA_ERR_MODVAR, str_data(name), str_data(module->name));
        }
        case TEA_TUDATA:
        case TEA_TINSTANCE:
        {
            GCinstance* instance = instanceV(receiver);
            TValue* o = tea_tab_get(&instance->attrs, name);
            if(o)
            {
                copyTV(T, T->top - nargs - 1, o);
                return vm_precall(T, o, nargs);
            }
            o = tea_tab_get(&instance->klass->methods, name);
            if(!o)
            {
                tea_err_callerv(T, TEA_ERR_METHOD, str_data(name));
            }
            return vm_call(T, funcV(o), nargs);
        }
        default:
        {
            GCclass* type = tea_meta_getclass(T, receiver);
            if(type)
            {
                TValue* o = tea_tab_get(&type->methods, name);
                if(o)
                {
                    return vm_precall(T, o, nargs);
                }
            }
            tea_err_callerv(T, TEA_ERR_NOMETHOD, tea_typename(receiver), str_data(name));
        }
    }
    return false;
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
    tea_err_callerv(T, TEA_ERR_ITER, tea_typename(obj));
}

static void vm_arith_unary(tea_State* T, MMS mm, TValue* o)
{
    TValue* mo = tea_meta_lookup(T, o, mm);
    if(!mo) tea_err_unoptype(T, o, mm);

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
    TValue* mo = tea_meta_lookup(T, a, mm); /* Try first operand */
    if(!mo) mo = tea_meta_lookup(T, b, mm); /* Try second operand */
    if(!mo) tea_err_bioptype(T, a, b, mm);  /* Bad types */

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
    TValue* mo = tea_meta_lookup(T, a, mm); /* Try first operand */
    if(!mo) mo = tea_meta_lookup(T, b, mm); /* Try second operand */
    if(!mo) return false;

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
#define READ_CONSTANT() (proto_kgc(T->ci->func->t.pt, READ_BYTE()))
#define READ_STRING() strV(READ_CONSTANT())

#define RUNTIME_ERROR(...) \
    do \
    { \
        STORE_FRAME; \
        tea_err_callerv(T, __VA_ARGS__); \
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
            goto *dispatch_table[bc = READ_BYTE()]; \
        } \
        while(false)

    #define INTERPRET_LOOP  DISPATCH();
    #define CASE_CODE(name) name
#else
    #define INTERPRET_LOOP \
        loop: \
            switch(bc = READ_BYTE())

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
        uint8_t bc;
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
                uint8_t nargs = READ_BYTE();
                uint8_t nopts = READ_BYTE();
                int xargs = T->top - base - nopts - 1;

                /*
                ** Temp array while we shuffle the stack
                ** Cannot have more than 255 args to a function, so
                ** we can define this with a constant limit
                */
                TValue values[255];
                int idx;

                for(idx = 0; idx < nopts + xargs; idx++)
                {
                    TValue* o = --T->top;
                    copyTV(T, values + idx, o);
                }

                --idx;

                for(int i = 0; i < xargs; i++)
                {
                    copyTV(T, T->top++, values + (idx - i));
                }

                /* Calculate how many "default" values are required */
                int remaining = nargs + nopts - xargs;

                /* Push any "default" values back onto the stack */
                for(int i = remaining; i > 0; i--)
                {
                    copyTV(T, T->top++, values + (i - 1));
                }
                DISPATCH();
            }
            CASE_CODE(BC_DEFINE_MODULE):
            {
                GCstr* name = READ_STRING();
                uint8_t flag = READ_BYTE();
                cTValue* o = T->top - 1;
                if(flag == 1 || flag == 0)
                {
                    copyTV(T, tea_tab_set(T, &T->ci->func->t.module->vars, name, NULL), o);
                }
                if(flag == 1 || flag == 2)
                {
                    copyTV(T, tea_tab_set(T, &T->ci->func->t.module->exports, name, NULL), o);
                }
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
            {
                TValue* obj = T->top - 1;
                GCstr* name = READ_STRING();
                STORE_FRAME;
                cTValue* o = tea_meta_getattr(T, name, obj);
                T->top--;
                copyTV(T, T->top++, o);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_PUSH_ATTR):
            {
                TValue* obj = T->top - 1;
                GCstr* name = READ_STRING();
                STORE_FRAME;
                cTValue* o = tea_meta_getattr(T, name, obj);
                copyTV(T, T->top++, o);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_SET_ATTR):
            {
                GCstr* name = READ_STRING();
                TValue* obj = T->top - 2;
                TValue* item = T->top - 1;
                STORE_FRAME;
                cTValue* o = tea_meta_setattr(T, name, obj, item);
                T->top -= 2;
                copyTV(T, T->top++, o);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_GET_SUPER):
            {
                GCstr* name = READ_STRING();
                GCclass* superclass = classV(--T->top);
                TValue* o = tea_tab_get(&superclass->methods, name);
                if(!o)
                {
                    RUNTIME_ERROR(TEA_ERR_METHOD, str_data(name));
                }
                GCmethod* bound = tea_method_new(T, T->top - 1, funcV(o));
                T->top--;
                setmethodV(T, T->top++, bound);
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
            {
                TValue* obj = T->top - 2;
                TValue* idx = T->top - 1;
                STORE_FRAME;
                cTValue* o = tea_meta_getindex(T, obj, idx);
                T->top -= 2;
                copyTV(T, T->top++, o);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_PUSH_INDEX):
            {
                TValue* obj = T->top - 2;
                TValue* idx = T->top - 1;
                STORE_FRAME;
                cTValue* o = tea_meta_getindex(T, obj, idx);
                copyTV(T, T->top++, o);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_SET_INDEX):
            {
                TValue* obj = T->top - 3;
                TValue* idx = T->top - 2;
                TValue* item = T->top - 1;
                STORE_FRAME;
                cTValue* o = tea_meta_setindex(T, obj, idx, item);
                T->top -= 3;
                copyTV(T, T->top++, o);
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
                TValue* mo = tea_meta_lookup(T, &v1, MM_CONTAINS);
                if(!mo) RUNTIME_ERROR(TEA_ERR_ITER, tea_typename(&v1));
                tea_vm_call(T, mo, 1);
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
                uint16_t ofs = READ_SHORT();
                TValue* o = --T->top;
                if(!tea_obj_equal(T->top - 1, o))
                {
                    ip += ofs;
                }
                else
                {
                    T->top--;
                }
                DISPATCH();
            }
            CASE_CODE(BC_JUMP):
            {
                uint16_t ofs = READ_SHORT();
                ip += ofs;
                DISPATCH();
            }
            CASE_CODE(BC_JUMP_IF_FALSE):
            {
                uint16_t ofs = READ_SHORT();
                if(tea_obj_isfalse(T->top - 1))
                {
                    ip += ofs;
                }
                DISPATCH();
            }
            CASE_CODE(BC_JUMP_IF_NIL):
            {
                uint16_t ofs = READ_SHORT();
                if(tvisnil(T->top - 1))
                {
                    ip += ofs;
                }
                DISPATCH();
            }
            CASE_CODE(BC_LOOP):
            {
                uint16_t ofs = READ_SHORT();
                ip -= ofs;
                DISPATCH();
            }
            CASE_CODE(BC_INVOKE_NEW):
            {
                uint8_t nargs = READ_BYTE();
                if(!tvisclass(T->top - nargs - 1))
                {
                    RUNTIME_ERROR(TEA_ERR_ISCLASS, tea_typename(T->top - nargs - 1));
                }
                GCclass* klass = classV(T->top - nargs - 1);
                TValue* f = &klass->init;
                if(!tvisnil(f))
                {
                    if(tvisfunc(f) && !iscfunc(funcV(f)))
                    {
                        setinstanceV(T, T->top - nargs - 1, tea_instance_new(T, klass));
                    }
                    else
                    {
                        setclassV(T, T->top - nargs - 1, klass);
                    }
                    STORE_FRAME;
                    if(vm_precall(T, f, nargs))
                    {
                        (T->ci - 1)->state = (CIST_TEA | CIST_CALLING);
                    }
                    READ_FRAME();
                    DISPATCH();
                }
                else if(nargs != 0)
                {
                    RUNTIME_ERROR(TEA_ERR_ARGS, 0, nargs);
                }
                else
                {
                    setinstanceV(T, T->top - nargs - 1, tea_instance_new(T, klass));
                }
                DISPATCH();
            }
            CASE_CODE(BC_CALL):
            {
                uint8_t nargs = READ_BYTE();
                STORE_FRAME;
                if(vm_precall(T, T->top - 1 - nargs, nargs))
                {
                    (T->ci - 1)->state = (CIST_TEA | CIST_CALLING);
                }
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_INVOKE):
            {
                GCstr* method = READ_STRING();
                uint8_t nargs = READ_BYTE();
                STORE_FRAME;
                if(vm_invoke(T, T->top - 1 - nargs, method, nargs))
                {
                    (T->ci - 1)->state = (CIST_TEA | CIST_CALLING);
                }
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_SUPER):
            {
                GCstr* method = READ_STRING();
                uint8_t nargs = READ_BYTE();
                GCclass* superclass = classV(--T->top);
                STORE_FRAME;
                TValue* mo = tea_tab_get(&superclass->methods, method);
                if(mo && vm_call(T, funcV(mo), nargs))
                {
                    (T->ci - 1)->state = (CIST_TEA | CIST_CALLING);
                }
                else
                {
                    RUNTIME_ERROR(TEA_ERR_METHOD, str_data(method));
                }
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_CLOSURE):
            {
                GCproto* pt = protoV(READ_CONSTANT());
                GCfunc* func = tea_func_newT(T, pt, T->ci->func->t.module);
                setfuncV(T, T->top++, func);
                for(int i = 0; i < func->t.upvalue_count; i++)
                {
                    uint8_t is_local = READ_BYTE();
                    uint8_t idx = READ_BYTE();
                    if(is_local)
                    {
                        func->t.upvalues[i] = tea_func_finduv(T, base + idx);
                    }
                    else
                    {
                        func->t.upvalues[i] = T->ci->func->t.upvalues[idx];
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
                TValue* mo = tea_meta_lookup(T, seq, MM_ITER);
                if(!mo) RUNTIME_ERROR(TEA_ERR_ITER, tea_typename(seq));
                tea_vm_call(T, mo, 1);
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
                TValue* mo = tea_meta_lookup(T, seq, MM_NEXT);
                if(!mo) RUNTIME_ERROR(TEA_ERR_ITER, tea_typename(seq));
                tea_vm_call(T, mo, 1);
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
            CASE_CODE(BC_EXTENSION_METHOD):
            {
                if(!tvisclass(T->top - 2))
                {
                    RUNTIME_ERROR(TEA_ERR_ISCLASS, tea_typename(T->top - 2));
                }
                /* Fallback */
            }
            CASE_CODE(BC_METHOD):
            {
                GCstr* name = READ_STRING();
                TValue* mo = T->top - 1;
                GCclass* klass = classV(T->top - 2);
                copyTV(T, tea_tab_set(T, &klass->methods, name, NULL), mo);
                if(name == T->init_str) copyTV(T, &klass->init, mo);
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
                GCstr* name = READ_STRING();
                TValue* o = tea_tab_get(&T->last_module->exports, name);
                if(!o)
                {
                    RUNTIME_ERROR(TEA_ERR_VARMOD, str_data(name), str_data(T->last_module->name));
                }
                copyTV(T, T->top++, o);
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

void tea_vm_call(tea_State* T, TValue* func, int nargs)
{
    if(++T->nccalls >= TEA_MAX_CCALLS)
    {
        tea_err_stkov(T);
    }

    if(vm_precall(T, func, nargs))
    {
        vm_execute(T);
    }
    T->nccalls--;
}