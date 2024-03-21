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

static GCstr* str_alloc(tea_State* T, char* chars, int len, uint32_t hash)
{
    GCstr* str = tea_obj_new(T, GCstr, TEA_TSTRING);
    str->len = len;
    str->chars = chars;
    str->hash = hash;

    setstrV(T, T->top++, str);
    setnullV(tea_tab_set(T, &T->strings, str, NULL));
    T->top--;

    return str;
}

static uint32_t str_hash(const char* key, int len)
{
    uint32_t hash = 2166136261u;
    for(int i = 0; i < len; i++)
    {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

GCstr* tea_str_take(tea_State* T, char* chars, int len)
{
    uint32_t hash = str_hash(chars, len);

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
    uint32_t hash = str_hash(chars, len);

    GCstr* interned = tea_tab_findstr(&T->strings, chars, len, hash);
    if(interned != NULL)
        return interned;

    char* heap_chars = tea_mem_new(T, char, len + 1);
    memcpy(heap_chars, chars, len);
    heap_chars[len] = '\0';

    return str_alloc(T, heap_chars, len, hash);
}

GCstr* tea_str_format(tea_State* T, const char* format, ...)
{
    va_list arg_list;

    va_start(arg_list, format);
    size_t total = 0;
    for(const char* c = format; *c != '\0'; c++)
    {
        switch(*c)
        {
            case '$':
            {
                /* A const char* C string */
                total += strlen(va_arg(arg_list, const char*));
                break;
            }
            case '@':
            {
                /* An GCstr* */
                total += va_arg(arg_list, GCstr*)->len;
                break;
            }
            default:
                /* Any other character is interpreted literally */
                total++;
        }
    }
    va_end(arg_list);

    char* bytes = tea_mem_new(T, char, total + 1);
    bytes[total] = '\0';

    va_start(arg_list, format);
    char* start = bytes;
    for(const char* c = format; *c != '\0'; c++)
    {
        switch(*c)
        {
            case '$':
            {
                const char* s = va_arg(arg_list, const char*);
                size_t l = strlen(s);
                memcpy(start, s, l);
                start += l;
                break;
            }
            case '@':
            {
                GCstr* s = va_arg(arg_list, GCstr*);
                memcpy(start, s->chars, s->len);
                start += s->len;
                break;
            }
            default:
                /* Any other character is interpreted literally */
                *start++ = *c;
        }
    }
    va_end(arg_list);

    return tea_str_take(T, bytes, total);
}