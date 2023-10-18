/*
** tea_string.h
** Teascript string implementation
*/

#define tea_string_c
#define TEA_CORE

#include "tea_string.h"
#include "tea_state.h"
#include "tea_vm.h"

static TeaOString* string_allocate(TeaState* T, char* chars, int length, uint32_t hash)
{
    TeaOString* string = TEA_ALLOCATE_OBJECT(T, TeaOString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    tea_vm_push(T, OBJECT_VAL(string));
    tea_tab_set(T, &T->strings, string, NULL_VAL);
    tea_vm_pop(T, 1);

    return string;
}

static uint32_t string_hash(const char* key, int length)
{
    uint32_t hash = 2166136261u;
    for(int i = 0; i < length; i++)
    {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

TeaOString* tea_str_take(TeaState* T, char* chars, int length)
{
    uint32_t hash = string_hash(chars, length);

    TeaOString* interned = tea_tab_findstr(&T->strings, chars, length, hash);
    if(interned != NULL)
    {
        TEA_FREE_ARRAY(T, char, chars, length + 1);
        return interned;
    }

    return string_allocate(T, chars, length, hash);
}

TeaOString* tea_str_copy(TeaState* T, const char* chars, int length)
{
    uint32_t hash = string_hash(chars, length);

    TeaOString* interned = tea_tab_findstr(&T->strings, chars, length, hash);
    if(interned != NULL)
        return interned;

    char* heap_chars = TEA_ALLOCATE(T, char, length + 1);
    memcpy(heap_chars, chars, length);
    heap_chars[length] = '\0';

    return string_allocate(T, heap_chars, length, hash);
}

TEA_FUNC TeaOString* tea_str_format(TeaState* T, const char* format, ...)
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
                /* a const char* C string */
                total += strlen(va_arg(arg_list, const char*));
                break;
            }
            case '@':
            {
                /* a TeaOString* */
                total += va_arg(arg_list, TeaOString*)->length;
                break;
            }
            default:
                // Any other character is interpreted literally
                total++;
        }
    }
    va_end(arg_list);

    char* bytes = TEA_ALLOCATE(T, char, total + 1);
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
                TeaOString* s = va_arg(arg_list, TeaOString*);
                memcpy(start, s->chars, s->length);
                start += s->length;
                break;
            }
            default:
                // Any other character is interpreted literally
                *start++ = *c;
        }
    }
    va_end(arg_list);

    return tea_str_take(T, bytes, total);
}