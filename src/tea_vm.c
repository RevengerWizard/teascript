/*
** tea_vm.c
** Teascript virtual machine
*/

#define tea_vm_c
#define TEA_CORE

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

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

static bool vm_callC(tea_State* T, GCfunc* f, int arg_count)
{
    GCfuncC* cfunc = &f->c;
    int extra = cfunc->type > C_FUNCTION;
    if((cfunc->nargs != TEA_VARARGS) && ((arg_count + extra) != cfunc->nargs))
    {
        tea_err_run(T, TEA_ERR_ARGS, cfunc->nargs, arg_count + extra);
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
    T->base = ci->base;
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
            return vm_call(T, bound->method, arg_count);
        }
        case TEA_TCLASS:
        {
            GCclass* klass = classV(callee);
            GCinstance* inst = tea_obj_new_instance(T, klass);
            setinstanceV(T, T->top - arg_count - 1, inst);
            if(!tvisnull(&klass->constructor)) 
            {
                return vm_precall(T, &klass->constructor, arg_count);
            }
            else if(arg_count != 0)
            {
                tea_err_run(T, TEA_ERR_NOARGS, arg_count);
            }
            return false;
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
    TValue* method = tea_tab_get(&klass->methods, name);
    if(!method)
    {
        tea_err_run(T, TEA_ERR_METH, name->chars);
    }
    return vm_call(T, funcV(method), arg_count);
}

static bool vm_invoke(tea_State* T, TValue* receiver, GCstr* name, int arg_count)
{
    switch(itype(receiver))
    {
        case TEA_TMODULE:
        {
            GCmodule* module = moduleV(receiver);

            TValue* value = tea_tab_get(&module->values, name);
            if(value)
            {
                return vm_precall(T, value, arg_count);
            }

            tea_err_run(T, TEA_ERR_MODVAR, name->chars, module->name->chars);
        }
        case TEA_TINSTANCE:
        {
            GCinstance* instance = instanceV(receiver);

            TValue* value = tea_tab_get(&instance->fields, name);
            if(value)
            {
                copyTV(T, T->top - arg_count - 1, value);
                return vm_precall(T, value, arg_count);
            }

            return vm_invoke_from_class(T, instance->klass, name, arg_count);
        }
        case TEA_TCLASS:
        {
            GCclass* klass = classV(receiver);
            TValue* method = tea_tab_get(&klass->methods, name);
            if(method)
            {
                if((tvisfunc(method) && iscfunc(funcV(method))) || 
                    funcV(method)->t.proto->type != PROTO_STATIC)
                {
                    tea_err_run(T, TEA_ERR_NOSTATIC, name->chars);
                }
                return vm_precall(T, method, arg_count);
            }

            tea_err_run(T, TEA_ERR_METH, name->chars);
        }
        default:
        {
            GCclass* type = tea_state_getclass(T, receiver);
            if(type != NULL)
            {
                TValue* value = tea_tab_get(&type->methods, name);
                if(value)
                {
                    return vm_precall(T, value, arg_count);
                }

                tea_err_run(T, TEA_ERR_NOMETH, tea_typename(receiver), name->chars);
            }
        }
    }
    return false;
}

static bool vm_bind_method(tea_State* T, GCclass* klass, GCstr* name)
{
    TValue* method = tea_tab_get(&klass->methods, name);
    if(!method)
    {
        tea_err_run(T, TEA_ERR_METH, name->chars);
    }
    GCmethod* bound = tea_obj_new_method(T, T->top - 1, funcV(method));
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
                    setnumberV(&n, i);
                    tea_list_add(T, list, &n);
                }
            }
            else if(step < 0)
            {
                for(int i = end + step; i >= 0; i += step)
                {
                    setnumberV(&n, i);
                    tea_list_add(T, list, &n);
                }
            }
            return;
        }
        case TEA_TLIST:
        {
            GClist* l = listV(obj);
            for(int i = 0; i < l->count; i++)
            {
                tea_list_add(T, list, l->items + i);
            }
            return;
        }
        case TEA_TSTRING:
        {
            GCstr* str = strV(obj);
            int len = tea_utf_len(str);
            for(int i = 0; i < len; i++)
            {
                GCstr* c = tea_utf_codepoint_at(T, str, tea_utf_char_offset(str->chars, i));
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

static void vm_splice(tea_State* T, TValue* obj, GCrange* range, TValue* item)
{
    switch(itype(obj))
    {
        case TEA_TLIST:
        {
            GClist* list = listV(obj);

            int32_t start = range->start;
            int32_t end;
            int32_t step = range->step;

            if(isinf(range->end))
            {
                end = list->count;
            } 
            else
            {
                end = range->end;
                if(end > list->count)
                {
                    end = list->count;
                } 
                else if(end < 0)
                {
                    end = list->count + end;
                }
            }

            /* Handle negative indexing */
            if(start < 0)
            {
                start = list->count + start;
                if(start < 0) start = 0;
            }
            if(end < 0) end = list->count + end;

            if(step <= 0) step = 1;

            /* Insert into list the item values based on the range */

            T->top -= 3;
            copyTV(T, T->top++, obj);
            return;
        }
        default:
            break;
    }

    tea_err_run(T, TEA_ERR_SLICE, tea_typename(obj));
}

static void vm_slice(tea_State* T, TValue* obj, GCrange* range, bool assign)
{
    switch(itype(obj))
    {
        case TEA_TLIST:
        {
            GClist* new_list = tea_list_new(T);
            setlistV(T, T->top++, new_list);
            GClist* list = listV(obj);

            int32_t start = range->start;
            int32_t end;
            int32_t step = range->step;

            if(isinf(range->end))
            {
                end = list->count;
            }
            else
            {
                end = range->end;
                if(end > list->count)
                {
                    end = list->count;
                }
                else if(end < 0)
                {
                    end = list->count + end;
                }
            }

            if(step > 0)
            {
                for(int i = start; i < end; i += step)
                {
                    tea_list_add(T, new_list, list->items + i);
                }
            }
            else if(step < 0)
            {
                for(int i = end + step; i >= start; i += step)
                {
                    tea_list_add(T, new_list, list->items + i);
                }
            }

            if(assign)
            {
                T->top -= 2;
            }

            T->top--;   /* Pop the pushed list */
            setlistV(T, T->top++, new_list);
            return;
        }
        case TEA_TSTRING:
        {
            GCstr* string = strV(obj);
            int len = tea_utf_len(string);

            int32_t start = range->start;
            int32_t end;

            if(isinf(range->end))
            {
                end = string->len;
            }
            else
            {
                end = range->end;
                if(end > len)
                {
                    end = len;
                }
                else if(end < 0)
                {
                    end = len + end;
                }
            }

            if(assign)
            {
                T->top -= 2;
            }

            /* Ensure the start index is below the end index */
            if(start > end)
            {
                GCstr* s = tea_str_newlit(T, "");
                setstrV(T, T->top++, s);
            }
            else
            {
                start = tea_utf_char_offset(string->chars, start);
                end = tea_utf_char_offset(string->chars, end);
                GCstr* s = tea_utf_from_range(T, string, start, end - start, 1);
                setstrV(T, T->top++, s);
            }
            return;
        }
        default:
            break;
    }

    tea_err_run(T, TEA_ERR_SLICE, tea_typename(obj));
}

static void vm_get_index(tea_State* T, TValue* index_value, TValue* obj, bool assign)
{
    switch(itype(obj))
    {
        case TEA_TINSTANCE:
        {
            GCinstance* instance = instanceV(obj);

            GCstr* subs = T->opm_name[MM_INDEX];

            TValue* method = tea_tab_get(&instance->klass->methods, subs);
            if(method)
            {
                if(!assign)
                {
                    copyTV(T, T->top++, obj);
                    copyTV(T, T->top++, index_value);
                }
                setnullV(T->top++);
                tea_vm_call(T, method, 2);
                return;
            }

            tea_err_run(T, TEA_ERR_INSTSUBSCR, instance->klass->name);
        }
        case TEA_TRANGE:
        {
            if(!tvisnumber(index_value))
            {
                tea_err_run(T, TEA_ERR_NUMRANGE);
            }

            GCrange* range = rangeV(obj);
            double index = numberV(index_value);

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
                setnumberV(T->top++, range->start + index * range->step);
                return;
            }

            tea_err_run(T, TEA_ERR_IDXRANGE);
        }
        case TEA_TLIST:
        {
            if(!tvisnumber(index_value))
            {
                tea_err_run(T, TEA_ERR_NUMLIST);
            }

            GClist* list = listV(obj);
            int index = numberV(index_value);

            /* Allow negative indexes */
            if(index < 0)
            {
                index = list->count + index;
            }

            if(index >= 0 && index < list->count)
            {
                if(assign)
                {
                    T->top -= 2;
                }
                copyTV(T, T->top++, list->items + index);
                return;
            }

            tea_err_run(T, TEA_ERR_IDXLIST);
        }
        case TEA_TMAP:
        {
            GCmap* map = mapV(obj);
            if(!tea_map_hashable(index_value))
            {
                tea_err_run(T, TEA_ERR_HASH);
            }

            TValue* value = tea_map_get(map, index_value);
            if(value)
            {
                if(assign)
                {
                    T->top -= 2;
                }
                copyTV(T, T->top++, value);
                return;
            }

            tea_err_run(T, TEA_ERR_MAPKEY);
        }
        case TEA_TSTRING:
        {
            if(!tvisnumber(index_value))
            {
                tea_err_run(T, TEA_ERR_NUMSTR, tea_typename(index_value));
            }

            GCstr* string = strV(obj);
            int index = numberV(index_value);
            int real_len = tea_utf_len(string);

            /* Allow negative indexes */
            if(index < 0)
            {
                index = real_len + index;
            }

            if(index >= 0 && index < string->len)
            {
                if(assign)
                {
                    T->top -= 2;
                }
                GCstr* c = tea_utf_codepoint_at(T, string, tea_utf_char_offset(string->chars, index));
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
        case TEA_TINSTANCE:
        {
            GCinstance* instance = instanceV(obj);

            GCstr* subs = T->opm_name[MM_INDEX];

            TValue* method = tea_tab_get(&instance->klass->methods, subs);
            if(method)
            {             
                tea_vm_call(T, method, 2);
                return;
            }

            tea_err_run(T, TEA_ERR_SETSUBSCR, instance->klass->name);
        }
        case TEA_TLIST:
        {
            if(!tvisnumber(index_value))
            {
                tea_err_run(T, TEA_ERR_NUMLIST);
            }

            GClist* list = listV(obj);
            int index = numberV(index_value);

            if(index < 0)
            {
                index = list->count + index;
            }

            if(index >= 0 && index < list->count)
            {
                copyTV(T, list->items + index, item_value);
                T->top -= 3;
                copyTV(T, T->top++, item_value);
                return;
            }

            tea_err_run(T, TEA_ERR_IDXLIST);
        }
        case TEA_TMAP:
        {
            GCmap* map = mapV(obj);
            if(!tea_map_hashable(index_value))
            {
                tea_err_run(T, TEA_ERR_HASH);
            }

            TValue* v = tea_map_set(T, map, index_value);
            copyTV(T, v, item_value);

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
        case TEA_TINSTANCE:
        {
            GCinstance* instance = instanceV(obj);

            TValue* value = tea_tab_get(&instance->fields, name);
            if(value)
            {
                if(dopop)
                {
                    T->top--; /* Instance */
                }
                copyTV(T, T->top++, value);
                return;
            }

            if(vm_bind_method(T, instance->klass, name))
                return;

            GCclass* klass = instance->klass;
            while(klass != NULL)
            {
                value = tea_tab_get(&klass->statics, name);
                if(value)
                {
                    if(dopop)
                    {
                        T->top--; /* Instance */
                    }
                    copyTV(T, T->top++, value);
                    return;
                }

                klass = klass->super;
            }

            tea_err_run(T, TEA_ERR_INSTATTR, instance->klass->name->chars, name->chars);
        }
        case TEA_TCLASS:
        {
            GCclass* klass = classV(obj);
            GCclass* klass_store = klass;

            while(klass != NULL)
            {
                TValue* value = tea_tab_get(&klass->statics, name);
                if(value)
                {
                    if(dopop)
                    {
                        T->top--; /* Class */
                    }
                    copyTV(T, T->top++, value);
                    return;
                }

                klass = klass->super;
            }

            tea_err_run(T, TEA_ERR_CLSATTR, klass_store->name->chars, name->chars);
        }
        case TEA_TMODULE:
        {
            GCmodule* module = moduleV(obj);

            TValue* value = tea_tab_get(&module->values, name);
            if(value)
            {
                if(dopop)
                {
                    T->top--; /* Module */
                }
                copyTV(T, T->top++, value);
                return;
            }

            tea_err_run(T, TEA_ERR_MODATTR, module->name->chars, name->chars);
        }
        case TEA_TMAP:
        {
            GCmap* map = mapV(obj);
            
            TValue v;
            setstrV(T, &v, name);
            TValue* value = tea_map_get(map, &v);
            if(value)
            {
                if(dopop)
                {
                    T->top--;
                }
                copyTV(T, T->top++, value);
                return;
            }
            else
            {
                goto retry;
            }

            tea_err_run(T, TEA_ERR_MAPATTR, name->chars);
        }
        default:
retry:
        {
            GCclass* klass = tea_state_getclass(T, obj);
            if(klass != NULL)
            {
                TValue* value = tea_tab_get(&klass->methods, name);
                if(value)
                {
                    if((tvisfunc(value) && iscfunc(funcV(value))) && 
                        funcV(value)->c.type == C_PROPERTY)
                    {
                        if(!dopop)
                        {
                            copyTV(T, T->top++, obj);
                        }
                        tea_vm_call(T, value, 0);
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

    tea_err_run(T, TEA_ERR_NOATTR, tea_typename(obj), name->chars);
}

static void vm_set_attr(tea_State* T, GCstr* name, TValue* obj, TValue* item)
{
    switch(itype(obj))
    {
        case TEA_TINSTANCE:
        {
            GCinstance* instance = instanceV(obj);
            TValue* v = tea_tab_set(T, &instance->fields, name, NULL);
            copyTV(T, v, item);
            T->top -= 2;
            copyTV(T, T->top++, item);
            return;
        }
        case TEA_TCLASS:
        {
            GCclass* klass = classV(obj);
            TValue* v = tea_tab_set(T, &klass->statics, name, NULL);
            copyTV(T, v, item);
            T->top -= 2;
            copyTV(T, T->top++, item);
            return;
        }
        case TEA_TMAP:
        {
            GCmap* map = mapV(obj);
            TValue v;
            setstrV(T, &v, name);
            TValue* tv = tea_map_set(T, map, &v);
            copyTV(T, tv, item);
            T->top -= 2;
            copyTV(T, T->top++, item);
            return;
        }
        case TEA_TMODULE:
        {
            GCmodule* module = moduleV(obj);
            TValue* v = tea_tab_set(T, &module->values, name, NULL);
            copyTV(T, v, item);
            T->top -= 2;
            copyTV(T, T->top++, item);
            return;
        }
        default:
        {
            GCclass* klass = tea_state_getclass(T, obj);
            if(klass != NULL)
            {
                TValue* value = tea_tab_get(&klass->methods, name);
                if(value)
                {
                    if((tvisfunc(value) && iscfunc(funcV(value))) && 
                        funcV(value)->c.type == C_PROPERTY)
                    {
                        tea_vm_call(T, value, 1);
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
    TValue* method = T->top - 1;
    GCclass* klass = classV(T->top - 2);
    TValue* v = tea_tab_set(T, &klass->methods, name, NULL);
    copyTV(T, v, method);
    if(name == T->constructor_string) copyTV(T, &klass->constructor, method);
    T->top--;
}

static TValue* vm_get_bcmethod(tea_State* T, GCstr* key, TValue* v)
{
    GCclass* klass = tea_state_getclass(T, v);
    if(klass == NULL)
        return false;

    return tea_tab_get(&klass->methods, key);
}

static void vm_arith_unary(tea_State* T, MMS op, TValue* v)
{
    GCstr* method_name = T->opm_name[op];

    TValue* method = vm_get_bcmethod(T, method_name, v);
    if(!method)
    {
        tea_err_run(T, TEA_ERR_UNOP, method_name->chars, tea_typename(v));
    }

    TValue v1;
    copyTV(T, &v1, v);

    T->top--;
    copyTV(T, T->top++, method);
    copyTV(T, T->top++, &v1);
    setnullV(T->top++);
    tea_vm_call(T, method, 2);
}

static void vm_arith(tea_State* T, MMS op, TValue* a, TValue* b)
{
    GCstr* method_name = T->opm_name[op];

    TValue* method = vm_get_bcmethod(T, method_name, a);    /* try first operand */
    if(!method)
    {
        method = vm_get_bcmethod(T, method_name, b); /* try second operand */
    }
    if(!method)
    {
        tea_err_run(T, TEA_ERR_BIOP, method_name->chars, tea_typename(a), tea_typename(b));
    }

    TValue v1, v2;
    copyTV(T, &v1, a);
    copyTV(T, &v2, b);

    T->top -= 2;
    copyTV(T, T->top++, method);
    copyTV(T, T->top++, &v1);
    copyTV(T, T->top++, &v2);
    tea_vm_call(T, method, 2);
}

static bool vm_arith_comp(tea_State* T, MMS op, TValue* a, TValue* b)
{
    GCstr* method_name = T->opm_name[op];

    TValue* method = vm_get_bcmethod(T, method_name, a);    /* try first operand */
    if(!method)
    {
        method = vm_get_bcmethod(T, method_name, b); /* try second operand */
    }
    if(!method)
    {
        return false;
    }

    TValue v1, v2;
    copyTV(T, &v1, a);
    copyTV(T, &v2, b);

    T->top -= 2;
    copyTV(T, T->top++, method);
    copyTV(T, T->top++, &v1);
    copyTV(T, T->top++, &v2);
    tea_vm_call(T, method, 2);
    return true;
}

static bool vm_iterator_call(tea_State* T, MMS op, TValue* receiver)
{
    GCstr* method_name = T->opm_name[op];

    GCclass* klass = tea_state_getclass(T, receiver);
    if(klass == NULL)
        return false;

    TValue* method = tea_tab_get(&klass->methods, method_name);
    if(method)
    {
        tea_vm_call(T, method, 1);
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
#define READ_CONSTANT() (T->ci->func->t.proto->k + READ_BYTE())
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
        if(tvisnumber(v1) && tvisnumber(v2)) \
        { \
            type b = numberV(--T->top); \
            type a = numberV(T->top - 1); \
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
        if(tvisnumber(v1)) \
        { \
            type v = numberV(v1); \
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
                TValue* v = READ_CONSTANT();
                copyTV(T, T->top++, v);
                DISPATCH();
            }
            CASE_CODE(BC_NULL):
            {
                setnullV(T->top++);
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
                TValue* value = T->top - 1;
                if(!tvisnull(value))
                {
                    TValue* v = tea_tab_set(T, &T->globals, T->repl_string, NULL);
                    copyTV(T, v, value);
                    GCstr* str = tea_strfmt_obj(T, value, 0);
                    setstrV(T, T->top++, str);
                    fwrite(str->chars, sizeof(char), str->len, stdout);
                    putchar('\n');
                    T->top--;
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
            CASE_CODE(BC_GET_GLOBAL):
            {
                GCstr* name = READ_STRING();
                TValue* value = tea_tab_get(&T->globals, name);
                if(!value)
                {
                    RUNTIME_ERROR(TEA_ERR_VAR, name->chars);
                }
                copyTV(T, T->top++, value);
                DISPATCH();
            }
            CASE_CODE(BC_SET_GLOBAL):
            {
                bool b;
                GCstr* name = READ_STRING();
                TValue* v = tea_tab_set(T, &T->globals, name, &b);
                if(b)
                {
                    tea_tab_delete(&T->globals, name);
                    RUNTIME_ERROR(TEA_ERR_VAR, name->chars);
                }
                copyTV(T, v, T->top - 1);
                DISPATCH();
            }
            CASE_CODE(BC_GET_MODULE):
            {
                GCstr* name = READ_STRING();
                TValue* value = tea_tab_get(&T->ci->func->t.proto->module->values, name);
                if(!value)
                {
                    RUNTIME_ERROR(TEA_ERR_VAR, name->chars);
                }
                copyTV(T, T->top++, value);
                DISPATCH();
            }
            CASE_CODE(BC_SET_MODULE):
            {
                bool b;
                GCstr* name = READ_STRING();
                TValue* v = tea_tab_set(T, &T->ci->func->t.proto->module->values, name, &b);
                if(b)
                {
                    tea_tab_delete(&T->ci->func->t.proto->module->values, name);
                    RUNTIME_ERROR(TEA_ERR_VAR, name->chars);
                }
                copyTV(T, v, T->top - 1);
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
            CASE_CODE(BC_DEFINE_GLOBAL):
            {
                GCstr* name = READ_STRING();
                TValue* v = tea_tab_set(T, &T->globals, name, NULL);
                copyTV(T, v, T->top - 1);
                T->top--;
                DISPATCH();
            }
            CASE_CODE(BC_DEFINE_MODULE):
            {
                GCstr* name = READ_STRING();
                TValue* v = tea_tab_set(T, &T->ci->func->t.proto->module->values, name, NULL);
                copyTV(T, v, T->top - 1);
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

                if(!tvisnumber(a) || !tvisnumber(b) || !tvisnumber(c))
                {
                    RUNTIME_ERROR(TEA_ERR_RANGE);
                }

                GCrange* r = tea_obj_new_range(T, numberV(a), numberV(b), numberV(c));
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

                if(var_count != list->count)
                {
                    if(var_count < list->count)
                    {
                        RUNTIME_ERROR(TEA_ERR_MAXUNPACK);
                    }
                    else
                    {
                        RUNTIME_ERROR(TEA_ERR_MINUNPACK);
                    }
                }

                for(int i = 0; i < list->count; i++)
                {
                    copyTV(T, T->top++, list->items + i);
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

                if(var_count > list->count)
                {
                    RUNTIME_ERROR(TEA_ERR_MINUNPACK);
                }

                for(int i = 0; i < list->count; i++)
                {
                    if(i == rest_pos)
                    {
                        GClist* rest_list = tea_list_new(T);
                        setlistV(T, T->top++, rest_list);
                        int j;
                        for(j = i; j < list->count - (var_count - rest_pos) + 1; j++)
                        {
                            tea_list_add(T, rest_list, list->items + j);
                        }
                        i = j - 1;
                    }
                    else
                    {
                        copyTV(T, T->top++, list->items + i);
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
                if(!tvisrange(index))
                    vm_get_index(T, index, obj, assign);
                else
                    vm_slice(T, obj, rangeV(index), assign);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_SET_INDEX):
            {
                TValue* obj = T->top - 3;
                TValue* index = T->top - 2;
                TValue* item = T->top - 1;
                STORE_FRAME;
                if(!tvisrange(index))
                    vm_set_index(T, item, index, obj);
                else
                    vm_splice(T, obj, rangeV(index), item);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_LIST_ITEM):
            {
                GClist* list = listV(T->top - 2);
                TValue* item = T->top - 1;
                tea_list_add(T, list, item);
                T->top--;
                DISPATCH();
            }
            CASE_CODE(BC_MAP_FIELD):
            {
                GCmap* map = mapV(T->top - 3);
                TValue* key = T->top - 2;
                TValue* value = T->top - 1;
                if(!tea_map_hashable(key))
                {
                    RUNTIME_ERROR(TEA_ERR_KHASH, tea_typename(key));
                }
                TValue* v = tea_map_set(T, map, key);
                copyTV(T, v, value);
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

                GCclass* instance_klass = tea_state_getclass(T, instance);
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
                if(tvisinstance(a) || tvisinstance(b))
                {
                    STORE_FRAME;
                    if(!vm_arith_comp(T, MM_EQ, a, b))
                    {
                        T->top -= 2;
                        bool x = tea_obj_equal(a, b);
                        setboolV(T->top++, x);
                        DISPATCH();
                    }
                    READ_FRAME();
                    DISPATCH();
                }
                else
                {
                    T->top -= 2;
                    bool x = tea_obj_equal(a, b);
                    setboolV(T->top++, x);
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
                BINARY_OP(setnumberV, (a + b), MM_PLUS, double);
                DISPATCH();
            }
            CASE_CODE(BC_SUBTRACT):
            {
                BINARY_OP(setnumberV, (a - b), MM_MINUS, double);
                DISPATCH();
            }
            CASE_CODE(BC_MULTIPLY):
            {
                BINARY_OP(setnumberV, (a * b), MM_MULT, double);
                DISPATCH();
            }
            CASE_CODE(BC_DIVIDE):
            {
                BINARY_OP(setnumberV, (a / b), MM_DIV, double);
                DISPATCH();
            }
            CASE_CODE(BC_MOD):
            {
                BINARY_OP(setnumberV, (fmod(a, b)), MM_MOD, double);
                DISPATCH();
            }
            CASE_CODE(BC_POW):
            {
                BINARY_OP(setnumberV, (pow(a, b)), MM_POW, double);
                DISPATCH();
            }
            CASE_CODE(BC_BAND):
            {
                BINARY_OP(setnumberV, (a & b), MM_BAND, uint32_t);
                DISPATCH();
            }
            CASE_CODE(BC_BOR):
            {
                BINARY_OP(setnumberV, (a | b), MM_BOR, uint32_t);
                DISPATCH();
            }
            CASE_CODE(BC_BNOT):
            {
                UNARY_OP(setnumberV, (~v), MM_BNOT, uint32_t);
                DISPATCH();
            }
            CASE_CODE(BC_BXOR):
            {
                BINARY_OP(setnumberV, (a ^ b), MM_BXOR, uint32_t);
                DISPATCH();
            }
            CASE_CODE(BC_LSHIFT):
            {
                BINARY_OP(setnumberV, (a << b), MM_LSHIFT, uint32_t);
                DISPATCH();
            }
            CASE_CODE(BC_RSHIFT):
            {
                BINARY_OP(setnumberV, (a >> b), MM_RSHIFT, uint32_t);
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
                UNARY_OP(setnumberV, (-v), MM_MINUS, double);
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
            CASE_CODE(BC_JUMP_IF_NULL):
            {
                uint16_t offset = READ_SHORT();
                if(tvisnull(T->top - 1))
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
                GCfunc* func = tea_func_newT(T, proto);
                setfuncV(T, T->top++, func);
                for(int i = 0; i < func->t.upvalue_count; i++)
                {
                    uint8_t is_local = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if(is_local)
                    {
                        func->t.upvalues[i] = tea_func_capture(T, base + index);
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
                tea_func_close(T, T->top - 1);
                T->top--;
                DISPATCH();
            }
            CASE_CODE(BC_RETURN):
            {
                TValue* result = --T->top;
                tea_func_close(T, base);
                STORE_FRAME;
                T->ci--;
                if(!(T->ci->state & CIST_CALLING))
                {
                    T->base = T->ci->base;
                    T->top = base;
                    if(T->ci->func != NULL &&
                        iscfunc(T->ci->func) &&
                        T->ci->func->c.type == C_FUNCTION)
                    {
                        T->base++;
                    }
                    copyTV(T, T->top++, result);
                    return;
                }
                T->base = T->ci->base;
                T->top = base;
                copyTV(T, T->top++, result);
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

                copyTV(T, base + slot2, T->top - 1);
                copyTV(T, iter, base + slot2);
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
                GCclass* k = tea_obj_new_class(T, READ_STRING(), NULL);
                setclassV(T, T->top++, k);
                DISPATCH();
            }
            CASE_CODE(BC_SET_CLASS_VAR):
            {
                GCclass* klass = classV(T->top - 2);
                GCstr* key = READ_STRING();
                TValue* v = tea_tab_set(T, &klass->statics, key, NULL);
                copyTV(T, v, T->top - 1);
                T->top--;
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
                if(tea_state_isclass(T, superclass))
                {
                    RUNTIME_ERROR(TEA_ERR_BUILTINSELF, superclass->name->chars);
                }

                GCclass* klass = classV(T->top - 1);
                if(klass == superclass)
                {
                    RUNTIME_ERROR(TEA_ERR_SELF);
                }
                klass->super = superclass;
                klass->constructor = superclass->constructor;

                tea_tab_addall(T, &superclass->methods, &klass->methods);
                tea_tab_addall(T, &superclass->statics, &klass->statics);
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
                    RUNTIME_ERROR(TEA_ERR_EXTMETH, tea_typename(T->top - 2));
                }
                vm_define_method(T, READ_STRING());
                T->top--;
                DISPATCH();
            }
            CASE_CODE(BC_IMPORT_STRING):
            {
                GCstr* path_name = READ_STRING();
                STORE_FRAME;
                tea_imp_relative(T, T->ci->func->t.proto->module->path, path_name);
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

                TValue* module_variable = tea_tab_get(&T->last_module->values, variable);
                if(!module_variable)
                {
                    RUNTIME_ERROR(TEA_ERR_VARMOD, variable->chars, T->last_module->name->chars);
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
                T->last_module = T->ci->func->t.proto->module;
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

int tea_vm_pcall(tea_State* T, tea_CPFunction func, void* u, ptrdiff_t old_top)
{
    int oldnccalls = T->nccalls;
    ptrdiff_t old_ci = ci_save(T, T->ci);
    int status = tea_err_protected(T, func, u);
    if(status != TEA_OK)    /* An error occurred? */
    {
        TValue* old = stack_restore(T, old_top);
        T->open_upvalues = NULL;
        T->nccalls = oldnccalls;
        T->ci = ci_restore(T, old_ci);
        T->base = T->ci->base;
        T->top = old;
        setnullV(T->top++);

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
        tea_err_run(T, TEA_ERR_CSTKOV);
    }

    if(vm_precall(T, func, arg_count))
    {
        vm_execute(T);
    }
    T->nccalls--;
}