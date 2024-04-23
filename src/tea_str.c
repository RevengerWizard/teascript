/*
** tea_str.h
** String handling
*/

#define tea_str_c
#define TEA_CORE

#include "tea_str.h"
#include "tea_state.h"
#include "tea_tab.h"
#include "tea_gc.h"

/* -- String interning ---------------------------------------------------- */

static GCstr* str_alloc(tea_State* T, char* chars, int len, StrHash hash)
{
    GCstr* str = tea_mem_newobj(T, GCstr, TEA_TSTRING);
    str->len = len;
    str->chars = chars;
    str->hash = hash;
    setstrV(T, T->top++, str);
    setnullV(tea_tab_set(T, &T->strings, str, NULL));
    T->top--;
    return str;
}

/* -- String hashing ------------------------------------------------------ */

static StrHash str_hash(const char* key, int len)
{
    StrHash hash = 2166136261u;
    for(int i = 0; i < len; i++)
    {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

GCstr* tea_str_take(tea_State* T, char* chars, int len)
{
    StrHash hash = str_hash(chars, len);
    GCstr* interned = tea_tab_findstr(&T->strings, chars, len, hash);
    if(interned != NULL)
    {
        tea_mem_freevec(T, char, chars, len + 1);
        return interned;
    }
    return str_alloc(T, chars, len, hash);
}

GCstr* tea_str_copy(tea_State* T, const char* chars, int len)
{
    StrHash hash = str_hash(chars, len);

    GCstr* interned = tea_tab_findstr(&T->strings, chars, len, hash);
    if(interned != NULL)
        return interned;

    char* heap_chars = tea_mem_newvec(T, char, len + 1);
    memcpy(heap_chars, chars, len);
    heap_chars[len] = '\0';

    return str_alloc(T, heap_chars, len, hash);
}