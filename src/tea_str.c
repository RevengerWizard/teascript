/*
** tea_str.h
** String handling
*/

#define tea_str_c
#define TEA_CORE

#include "tea_str.h"
#include "tea_gc.h"
#include "tea_err.h"

/* -- String hashing ------------------------------------------------------ */

static StrHash str_hash(const char* key, uint32_t len)
{
    StrHash hash = 2166136261u;
    for(uint32_t i = 0; i < len; i++)
    {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

/* -- String interning ---------------------------------------------------- */

/* Resize the string interning hash table (grow and shrink) */
void tea_str_resize(tea_State* T, uint32_t newsize)
{
    GCobj** newhash = tea_mem_newvec(T, GCobj*, newsize);
    memset(newhash, 0, newsize * sizeof(GCobj*));

    for(int i = 0; i < T->str.size; i++)
    {
        GCobj* obj = T->str.hash[i];
        while(obj != NULL)
        {
            GCobj* next = obj->next;
            StrHash hash = ((GCstr*)obj)->hash;
            uint32_t idx = hash & (newsize - 1);
            obj->next = newhash[idx];
            newhash[idx] = obj;
            obj = next;
        }
    }

    tea_str_freetab(T);
    T->str.hash = newhash;
    T->str.size = newsize;
}

/* Allocate a new string and add to string interning table */
static GCstr* str_alloc(tea_State* T, const char* chars, uint32_t len, StrHash hash)
{
    GCobj* obj = (GCobj*)tea_mem_new(T, tea_str_size(len));
    obj->gct = TEA_TSTR;
    obj->marked = 0;
    GCstr* s = (GCstr*)obj;
    s->reserved = 0;
    s->len = len;
    s->hash = hash;
    memcpy(str_datawr(s), chars, len);
    str_datawr(s)[len] = '\0';
    /* Add to string hash table */
    hash &= (T->str.size - 1);
    obj->next = T->str.hash[hash];
    T->str.hash[hash] = (GCobj*)s;
    T->str.num++;
    if(T->str.num > T->str.size)
    {
        tea_str_resize(T, T->str.size << 1);    /* Grow string hash table */
    }
    return s; /* Return newly interned string */
}

/* Intern a string and/or return string object */
GCstr* tea_str_new(tea_State* T, const char* chars, size_t lenx)
{
    if(lenx - 1 < TEA_MAX_STR - 1)
    {
        uint32_t len = (uint32_t)lenx;
        StrHash hash = str_hash(chars, len);
        /* Check if the string has already been interned */
        GCobj* obj = T->str.hash[hash & (T->str.size - 1)];
        while(obj != NULL)
        {
            GCstr* sx = (GCstr*)obj;
            if(sx->hash == hash && sx->len == len && memcmp(chars, str_data(sx), len) == 0)
            {
                return sx;  /* Return existing string */
            }
            obj = obj->next;
        }
        /* Otherwise allocate a new string */
        return str_alloc(T, chars, len, hash);
    }
    else
    {
        if(lenx)
            tea_err_msg(T, TEA_ERR_STROV);
        return &T->strempty;
    }
}

void TEA_FASTCALL tea_str_free(tea_State* T, GCstr* str)
{
    T->str.num--;
    tea_mem_free(T, str, tea_str_size(str->len));
}

void TEA_FASTCALL tea_str_init(tea_State* T)
{
    tea_str_resize(T, TEA_MIN_STRTAB);
}