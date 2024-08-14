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
#include "tea_err.h"

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

/* -- String interning ---------------------------------------------------- */

/* Allocate a new string and add to string interning table */
static GCstr* str_alloc(tea_State* T, const char* chars, int len, StrHash hash)
{
    GCstr* str = (GCstr*)tea_mem_newgco(T, tea_str_size(len), TEA_TSTR);
    str->reserved = 0;
    str->len = len;
    str->hash = hash;
    memcpy(str_datawr(str), chars, len);
    str_datawr(str)[len] = '\0';
    setstrV(T, T->top++, str);
    setnilV(tea_tab_set(T, &T->strings, str, NULL));
    T->top--;
    return str;
}

/* Intern a string and/or return string object */
GCstr* tea_str_new(tea_State* T, const char* chars, size_t len)
{
    if(len - 1 < TEA_MAX_STR - 1)
    {
        StrHash hash = str_hash(chars, len);
        /* Check if the string has already been interned */
        GCstr* str = tea_tab_findstr(&T->strings, chars, len, hash);
        if(str != NULL)
            return str; /* Return existing string */
        /* Otherwise allocate a new string */
        return str_alloc(T, chars, len, hash);
    }
    else
    {
        if(len)
            tea_err_msg(T, TEA_ERR_STROV);
        return &T->strempty;
    }
}

void TEA_FASTCALL tea_str_free(tea_State* T, GCstr* str)
{
    tea_mem_free(T, str, tea_str_size(str->len));
}