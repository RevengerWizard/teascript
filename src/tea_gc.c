/*
** tea_gc.c
** Garbage collector
*/

#include <stdlib.h>

#define tea_gc_c
#define TEA_CORE

#include "tea_gc.h"
#include "tea_buf.h"
#include "tea_tab.h"
#include "tea_func.h"
#include "tea_udata.h"
#include "tea_list.h"
#include "tea_map.h"
#include "tea_str.h"
#include "tea_vm.h"

#ifdef TEA_DEBUG_LOG_GC
#include <stdio.h>
#endif

#define GC_HEAP_GROW_FACTOR 2

#define is_finalized(u) ((u)->marked & TEA_GC_FINALIZED)

/* -- Collector -------------------------------------------------- */

static void gc_blacken(tea_State* T, GCobj* obj)
{
    switch(obj->gct)
    {
        case TEA_TUDATA:
        {
            GCudata* ud = (GCudata*)obj;
            tea_gc_markobj(T, (GCobj*)ud->klass);
            tea_tab_mark(T, &ud->attrs);
            break;
        }
        case TEA_TMODULE:
        {
            GCmodule* module = (GCmodule*)obj;
            tea_gc_markobj(T, (GCobj*)module->name);
            tea_gc_markobj(T, (GCobj*)module->path);
            tea_tab_mark(T, &module->vars);
            tea_tab_mark(T, &module->exports);
            break;
        }
        case TEA_TLIST:
        {
            GClist* list = (GClist*)obj;
            for(int i = 0; i < list->len; i++)
            {
                tea_gc_markval(T, list_slot(list, i));
            }
            break;
        }
        case TEA_TMAP:
        {
            GCmap* map = (GCmap*)obj;
            for(int i = 0; i < map->size; i++)
            {
                MapEntry* item = &map->entries[i];
                tea_gc_markval(T, &item->key);
                tea_gc_markval(T, &item->val);
            }
            break;
        }
        case TEA_TMETHOD:
        {
            GCmethod* bound = (GCmethod*)obj;
            tea_gc_markval(T, &bound->receiver);
            tea_gc_markobj(T, (GCobj*)bound->func);
            break;
        }
        case TEA_TCLASS:
        {
            GCclass* klass = (GCclass*)obj;
            tea_gc_markobj(T, (GCobj*)klass->name);
            tea_gc_markobj(T, (GCobj*)klass->super);
            tea_tab_mark(T, &klass->methods);
            break;
        }
        case TEA_TFUNC:
        {
            GCfunc* func = (GCfunc*)obj;
            if(isteafunc(func))
            {
                tea_gc_markobj(T, (GCobj*)func->t.pt);
                for(int i = 0; i < func->t.upvalue_count; i++)
                {
                    tea_gc_markobj(T, (GCobj*)func->t.upvalues[i]);
                }
            }
            else
            {
                for(int i = 0; i < func->c.upvalue_count; i++)
                {
                    tea_gc_markval(T, &func->c.upvalues[i]);
                }
            }
            break;
        }
        case TEA_TPROTO:
        {
            GCproto* pt = (GCproto*)obj;
            tea_gc_markobj(T, (GCobj*)pt->name);
            for(int i = 0; i < pt->sizek; i++)
            {
                tea_gc_markval(T, proto_kgc(pt, i));
            }
            break;
        }
        case TEA_TINSTANCE:
        {
            GCinstance* instance = (GCinstance*)obj;
            tea_gc_markobj(T, (GCobj*)instance->klass);
            tea_tab_mark(T, &instance->attrs);
            break;
        }
        case TEA_TUPVAL:
        {
            GCupval* uv = (GCupval*)obj;
            tea_gc_markval(T, &uv->closed);
            break;
        }
        case TEA_TSTR:
        case TEA_TRANGE:
            break;
    }
}

/* Type of GC free functions */
typedef void (TEA_FASTCALL *GCFreeFunc)(tea_State* T, GCobj* o);

/* GC free functions */
static const GCFreeFunc gc_freefunc[] = {
    (GCFreeFunc)tea_str_free,
    (GCFreeFunc)tea_range_free,
    (GCFreeFunc)tea_func_free,
    (GCFreeFunc)tea_module_free,
    (GCFreeFunc)tea_class_free,
    (GCFreeFunc)tea_instance_free,
    (GCFreeFunc)tea_list_free,
    (GCFreeFunc)tea_map_free,
    (GCFreeFunc)tea_udata_free,
    (GCFreeFunc)tea_func_freeproto,
    (GCFreeFunc)tea_func_freeuv,
    (GCFreeFunc)tea_method_free
};

static void gc_free(tea_State* T, GCobj* obj)
{
    gc_freefunc[obj->gct - TEA_TSTR](T, obj);
}

/* Mark GC roots */
static void gc_mark_roots(tea_State* T)
{
    for(TValue* slot = T->stack; slot < T->top; slot++)
    {
        tea_gc_markval(T, slot);
    }

    for(CallInfo* ci = T->ci_base; ci <= T->ci; ci++)
    {
        tea_gc_markobj(T, (GCobj*)ci->func);
    }

    for(GCupval* upvalue = T->open_upvalues; upvalue != NULL; upvalue = upvalue->next)
    {
        tea_gc_markobj(T, (GCobj*)upvalue);
    }

    tea_gc_markval(T, registry(T));
    tea_tab_mark(T, &T->modules);
    tea_tab_mark(T, &T->globals);

    tea_gc_markobj(T, (GCobj*)T->number_class);
    tea_gc_markobj(T, (GCobj*)T->bool_class);
    tea_gc_markobj(T, (GCobj*)T->func_class);
    tea_gc_markobj(T, (GCobj*)T->list_class);
    tea_gc_markobj(T, (GCobj*)T->map_class);
    tea_gc_markobj(T, (GCobj*)T->string_class);
    tea_gc_markobj(T, (GCobj*)T->range_class);
    tea_gc_markobj(T, (GCobj*)T->object_class);
}

static void gc_trace_references(tea_State* T)
{
    while(T->gc.gray_count > 0)
    {
        GCobj* obj = T->gc.gray_stack[--T->gc.gray_count];
        gc_blacken(T, obj);
    }
}

static void gc_sweep(tea_State* T, GCobj** p)
{
    GCobj* prev = NULL;
    GCobj* obj = *p;

    while(obj != NULL)
    {
        if(obj->marked)
        {
            if(!(obj->marked & TEA_GC_FIXED))
                obj->marked = 0;
            prev = obj;
            obj = obj->next;
        }
        else
        {
            GCobj* unreached = obj;
            obj = obj->next;
            if(prev != NULL)
            {
                prev->next = obj;
            }
            else
            {
                *p = obj;
            }
            gc_free(T, unreached);
        }
    }
}

/* Mark userdata in mmudata list */
static void gc_mark_mmudata(tea_State* T)
{
    GCobj* obj;
    for(obj = T->gc.mmudata; obj; obj = obj->next)
    {
        obj->marked = 0;
        gc_blacken(T, obj);
    }
}

/* Separate userdata objects to be finalized to mmudata list */
void tea_gc_separateudata(tea_State* T)
{
    GCobj** p = &T->gc.rootud;
    GCobj* curr;
    GCobj* collected = NULL; /* to collect udata with gc event */
    GCobj** lastcollected = &collected;
    while((curr = *p) != NULL)
    {
        tea_assertT(curr->gct == TEA_TUDATA, "trying to separate non-userdata");
        GCudata* ud = (GCudata*)curr;
        if((curr->marked == 1) || is_finalized(curr))
            p = &curr->next;    /* Don't bother with them */
        else if(tea_tab_get(&ud->klass->methods, mmname_str(T, MM_GC)) == NULL)
        {
            mark_finalized(curr);   /* Don't need finalization */
            p = &curr->next;
        }
        else
        {
            /* Must call its gc method */
            *p = curr->next;
            curr->next = NULL;  /* Link 'curr' at the end of 'collected' list */
            *lastcollected = curr;
            lastcollected = &curr->next;
        }
    }
    /* Insert finalizable userdata into 'mmudata' list */
    *lastcollected = T->gc.mmudata;
    T->gc.mmudata = collected;
}

/* Finalize all userdata objects from mmudata list */
void tea_gc_finalize_udata(tea_State* T)
{
    T->top++;
    while(T->gc.mmudata != NULL)
    {
        GCobj* obj = T->gc.mmudata;
        GCudata* ud = (GCudata*)obj;
        T->gc.mmudata = obj->next;  /* Remove userdata from mmudata list */
        obj->next = T->gc.rootud;   /* Add it back to the 'rootud' list */
        T->gc.rootud = obj;
        setudataV(T, T->top - 1, ud);   /* Keep a reference to it */
        obj->marked = 0;
        mark_finalized(ud);
        cTValue* mo = tea_tab_get(&ud->klass->methods, mmname_str(T, MM_GC));
        if(mo != NULL)
        {
            setudataV(T, T->top++, ud);
            tea_vm_call(T, (TValue*)mo, 0);
            T->top--;
        }
    }
    T->top--;
}

/* Mark a TValue (if needed) */
void tea_gc_markval(tea_State* T, TValue* o)
{
    if(tvisgcv(o))
        tea_gc_markobj(T, gcV(o));
}

/* Mark a GC object (if needed) */
void tea_gc_markobj(tea_State* T, GCobj* obj)
{
    if(obj == NULL)
        return;
    if(obj->marked)
        return;

    obj->marked = 1;

    if(T->gc.gray_size < T->gc.gray_count + 1)
    {
        T->gc.gray_size = TEA_MEM_GROW(T->gc.gray_size);
        T->gc.gray_stack = (GCobj**)T->allocf(T->allocd, T->gc.gray_stack, 0, sizeof(GCobj*) * T->gc.gray_size);

        if(T->gc.gray_stack == NULL)
            tea_err_mem(T);
    }

    T->gc.gray_stack[T->gc.gray_count++] = obj;
}

/* Perform a GC collection */
void tea_gc_collect(tea_State* T)
{
#ifdef TEA_DEBUG_LOG_GC
    printf("-- gc begin\n");
    size_t before = T->gc.total;
#endif

    tea_gc_markval(T, &T->registrytv);
    gc_mark_roots(T);
    gc_trace_references(T);

    tea_gc_separateudata(T);    /* Separate userdata to be finalized */
    gc_mark_mmudata(T);     /* Mark them */
    gc_trace_references(T); /* And propagate the marks */

    gc_sweep(T, &T->gc.rootud);
    tea_tab_white(&T->strings);
    gc_sweep(T, &T->gc.root);
    tea_buf_shrink(T, &T->tmpbuf);  /* Shrink temp buffer */
    tea_buf_shrink(T, &T->strbuf);  /* Shrink string buffer */

    T->gc.next_gc = T->gc.total * GC_HEAP_GROW_FACTOR;

    tea_gc_finalize_udata(T);

#ifdef TEA_DEBUG_LOG_GC
    printf("-- gc end\n");
    printf("   collected %llu bytes (from %llu to %llu) next at %llu\n", before - T->gc.total, before, T->gc.total, T->gc.next_gc);
#endif
}

/* Free all remaining GC objects */
void tea_gc_freeall(tea_State* T)
{
    GCobj* obj = T->gc.root;
    while(obj != NULL)
    {
        GCobj* next = obj->next;
        gc_free(T, obj);
        obj = next;
    }

    obj = T->gc.rootud;
    while(obj != NULL)
    {
        GCobj* next = obj->next;
        gc_free(T, obj);
        obj = next;
    }

    /* Free the gray stack */
    T->allocf(T->allocd, T->gc.gray_stack, sizeof(GCobj*) * T->gc.gray_size, 0);
}

/* -- Allocator -------------------------------------------------- */

/* Call pluggable memory allocator to allocate or resize a fragment */
void* tea_mem_realloc(tea_State* T, void* p, size_t old_size, size_t new_size)
{
    tea_assertT((old_size == 0) == (p == NULL), "realloc API violation");
    T->gc.total += new_size - old_size;

    if(new_size > old_size)
    {
#ifdef TEA_DEBUG_STRESS_GC
        tea_gc_collect(T);
#endif

        if(T->gc.total > T->gc.next_gc)
        {
            tea_gc_collect(T);
        }
    }

    p = T->allocf(T->allocd, p, old_size, new_size);
    if(p == NULL && new_size > 0)
        tea_err_mem(T);
    tea_assertT((new_size == 0) == (p == NULL), "allocf API violation");
    return p;
}

/* Allocate new GC object and link it to the objects root */
GCobj* tea_mem_newgco(tea_State* T, size_t size, uint8_t type)
{
    GCobj* obj = (GCobj*)tea_mem_realloc(T, NULL, 0, size);
    obj->gct = type;
    obj->marked = 0;
    obj->next = T->gc.root;
    T->gc.root = obj;
    return obj;
}

/* Resize growable vector */
void* tea_mem_grow(tea_State* T, void* p, uint32_t* size, size_t size_elem, int limit)
{
    size_t new_size = (*size) << 1;
    if(new_size < TEA_MIN_VECSIZE)
        new_size = TEA_MIN_VECSIZE;
    if(new_size > limit)
        new_size = limit;
    p = tea_mem_realloc(T, p, (*size) * size_elem, new_size * size_elem);
    *size = new_size;
    return p;
}