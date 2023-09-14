/* 
** tea_dump.c
** Teascript bytecode saving
*/

#define tea_dump_c
#define TEA_CORE

#include <stdio.h>

#include "tea_dump.h"
#include "tea_object.h"
#include "tea_state.h"

typedef struct
{
    TeaState* T;
    FILE* file;
} TeaDumpState;

#define dump_vector(D, v, n) dump_block(D, v, (n) * sizeof((v)[0]))
#define dump_literal(D, s) dump_block(D, s, sizeof(s) - sizeof(char))

static void dump_block(TeaDumpState* D, const void* b, size_t size)
{
    if(size > 0)
    {
        fwrite(b, size, 1, D->file);
    }
}

#define dump_var(D, x)   dump_vector(D, &x, 1)

static void dump_byte(TeaDumpState* D, uint8_t byte)
{
    dump_var(D, byte);
}

/* buffer size of dump_int */
#define DIBS ((sizeof(size_t) * 8 / 7) + 1)

static void dump_size(TeaDumpState* D, size_t x)
{
    unsigned char buff[DIBS];
    int n = 0;
    do 
    {
        /* fill buffer in reverse order */
        buff[DIBS - (++n)] = x & 0x7f;
        x >>= 7;
    } 
    while (x != 0);
    buff[DIBS - 1] |= 0x80; /* mark last byte */
    dump_vector(D, buff + DIBS - n, n);
}

static void dump_int(TeaDumpState* D, int x)
{
    dump_size(D, x);
}

static void dump_number(TeaDumpState* D, double n)
{
    printf("NUM %.16g\n", n);
    dump_var(D, n);
}

static void dump_string(TeaDumpState* D, TeaOString* string)
{
    if(string == NULL)
    {
        dump_size(D, 0);
    }
    else
    {
        int size = string->length;
        const char* s = string->chars;
        dump_size(D, size + 1);
        dump_vector(D, s, size);
    }
}

static void dump_chunk(TeaDumpState* D, TeaChunk* chunk)
{
    dump_int(D, chunk->count);
    dump_vector(D, chunk->code, chunk->count);
    /*dump_vector(D, chunk->lines, chunk->count);*/
}

static void dump_function(TeaDumpState* D, TeaOFunction* f);

static void dump_constants(TeaDumpState* D, TeaChunk* chunk)
{
    dump_int(D, chunk->constants.count);
    for(int i = 0; i < chunk->constants.count; i++)
    {
        TeaValue constant = chunk->constants.values[i];
        if(IS_OBJECT(constant))
        {
            TeaObjectType type = AS_OBJECT(constant)->type;
            dump_byte(D, type + 1);
            switch(type)
            {
                case OBJ_STRING:
                {
                    dump_string(D, AS_STRING(constant));
                    break;
                }
                case OBJ_FUNCTION:
                {
                    dump_function(D, AS_FUNCTION(constant));
                    break;
                }
                default:    /* Unreachable */
                    break;
            }
        }
        else
        {
            dump_byte(D, 0);
            dump_number(D, AS_NUMBER(constant));
        }
    }
}

static void dump_function(TeaDumpState* D, TeaOFunction* f)
{
    dump_string(D, f->name);
    dump_int(D, f->arity);
    dump_int(D, f->arity_optional);
    dump_int(D, f->variadic);
    dump_int(D, f->upvalue_count);
    dump_int(D, f->max_slots);
    dump_byte(D, f->type);
    dump_chunk(D, &f->chunk);
    dump_constants(D, &f->chunk);
}

static void dump_header(TeaDumpState* D)
{
    dump_literal(D, TEA_SIGNATURE);
    dump_byte(D, TEA_VERSION_MAJOR);
    dump_byte(D, TEA_VERSION_MINOR);
    dump_byte(D, TEA_VERSION_PATCH);
    dump_byte(D, TEA_BYTECODE_FORMAT);
}

void tea_dump(TeaState* T, TeaOFunction* f, FILE* file)
{
    TeaDumpState D;
    D.T = T;
    D.file = file;
    dump_header(&D);
    dump_function(&D, f);
}