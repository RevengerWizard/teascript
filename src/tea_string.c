/*
** tea_string.h
** Teascript string implementation
*/

#define tea_string_c
#define TEA_CORE

#include "tea_string.h"
#include "tea_state.h"
#include "tea_vm.h"

static TeaObjectString* string_allocate(TeaState* T, char* chars, int length, uint32_t hash)
{
    TeaObjectString* string = ALLOCATE_OBJECT(T, TeaObjectString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    tea_vm_push(T, OBJECT_VAL(string));
    tea_table_set(T, &T->strings, string, NULL_VAL);
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

TeaObjectString* tea_string_take(TeaState* T, char* chars, int length)
{
    uint32_t hash = string_hash(chars, length);

    TeaObjectString* interned = tea_table_find_string(&T->strings, chars, length, hash);
    if(interned != NULL)
    {
        TEA_FREE_ARRAY(T, char, chars, length + 1);
        return interned;
    }

    return string_allocate(T, chars, length, hash);
}

TeaObjectString* tea_string_copy(TeaState* T, const char* chars, int length)
{
    uint32_t hash = string_hash(chars, length);

    TeaObjectString* interned = tea_table_find_string(&T->strings, chars, length, hash);
    if(interned != NULL)
        return interned;

    char* heap_chars = TEA_ALLOCATE(T, char, length + 1);
    memcpy(heap_chars, chars, length);
    heap_chars[length] = '\0';

    return string_allocate(T, heap_chars, length, hash);
}