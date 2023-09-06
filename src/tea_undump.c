/* 
** tea_undump.c
** Teascript bytecode loading
*/

#include <stdio.h>
#include <stdlib.h>

#include "tea_dump.h"
#include "tea_object.h"
#include "tea_state.h"

typedef struct
{
    TeaState* T;
    FILE* file;
    const char* name;
} TeaLoadState;

/* error function for malformed bytecode files */
static void error(TeaLoadState* S, const char* message)
{
    fprintf(stderr, "%s bad binary format: %s", message);
    exit(1);
}

#define load_vector(S, b, n) load_block(S, b, (n) * sizeof((b)[0]))

static void load_block(TeaLoadState* S, void* b, size_t size)
{
    if(fread(b, size, 1, S->file) != 0)
    {
        error(S, "truncated chunk");
    }
}

#define load_var(S, x) load_vector(S, &x, 1)

static uint8_t load_byte(TeaLoadState* S)
{
    uint8_t byte;
    load_var(S, byte);
    return (uint8_t)byte;
}

static size_t load_unsigned(TeaLoadState* S, size_t limit)
{
    size_t x = 0;
    int b;
    limit >>= 7;
    do 
    {
        b = load_byte(S);
        if(x >= limit)
            error(S, "integer overflow");
        x = (x << 7) | (b & 0x7f);
    } 
    while((b & 0x80) == 0);
    return x;
}

static size_t load_size(TeaLoadState* S)
{
    return load_unsigned(S, ~(size_t)0);
}

static int load_int(TeaLoadState* S)
{
    return (int)load_unsigned(S, __INT_MAX__);
}

static double load_number(TeaLoadState* S)
{
    double n;
    load_var(S, n);
    return n;
}

/* load nullable string */
static TeaObjectString* load_null_string(TeaLoadState* S, TeaObjectFunction* f)
{
    TeaState* T = S->T;
    TeaObjectString* s;
    size_t size = load_size(S);
    if(size == 0)
    {
        return NULL;
    }
    else
    {
        char* buff = TEA_ALLOCATE(T, char, size);
        load_vector(S, buff, size);
        s = tea_take_string(T, buff, size);
    }
    return s;
}

/* load non nullable string */
static TeaObjectString* load_string(TeaLoadState* S, TeaObjectFunction* f)
{
    TeaObjectString* s = load_null_string(S, f);
    if(s == NULL)
        error(S, "bad constant string");
    return s;
}

static void load_chunk(TeaLoadState* S, TeaChunk* chunk)
{
    TeaState* T = S->T;
    int count = load_byte(S);

    chunk->code = TEA_ALLOCATE(T, uint8_t, count);
    chunk->count = count;
    chunk->capacity = count;
    load_vector(S, chunk->code, chunk->count);

    chunk->lines = TEA_ALLOCATE(T, int, count);
    load_vector(S, chunk->lines, chunk->count);
}

static TeaObjectFunction* load_function(TeaLoadState* S);

static void load_constants(TeaLoadState* S, TeaObjectFunction* f)
{
    TeaState* T = S->T;
    TeaChunk* chunk = &f->chunk;
    int count = load_int(S);

    /* allocate constants */
    chunk->constants.values = TEA_ALLOCATE(T, TeaValue, count);
	chunk->constants.count = count;
	chunk->constants.capacity = count;

    for(int i = 0; i < count; i++)
    {
        TeaObjectType type = load_byte(S);
        if(type == 0)
        {
            chunk->constants.values[i] = NUMBER_VAL(load_number(S));
        }
        else
        {
            switch(type - 1)
            {
                case OBJ_STRING:
                {
                    chunk->constants.values[i] = OBJECT_VAL(load_string(S, f));
                    break;
                }
                case OBJ_FUNCTION:
                {
                    chunk->constants.values[i] = OBJECT_VAL(load_function(S));
                    break;
                }
                default:    /* Unreachable */
                    break;
            }
        }
    }
}

static TeaObjectFunction* load_function(TeaLoadState* S)
{
    TeaState* T = S->T;
    TeaObjectFunction* f = tea_new_function(T, 0, NULL, 0);
    f->name = load_null_string(S, f);
    f->arity = load_int(S);
    f->arity_optional = load_int(S);
    f->variadic = load_int(S);
    f->upvalue_count = load_int(S);
    f->max_slots = load_int(S);
    f->type = load_byte(S);
    load_chunk(S, &f->chunk);
    load_constants(S, f);
}

static void check_literal(TeaLoadState* S, const char* str, const char* message)
{
    char buff[sizeof(TEA_SIGNATURE)];
    int len = strlen(buff);
    load_vector(S, buff, len);
    if(memcmp(str, buff, len) != 0)
        error(S, message);
}

static void check_header(TeaLoadState* S)
{
    check_literal(S, &TEA_SIGNATURE[1], "not a binary chunk");
    uint8_t major, minor, patch;
    major = load_byte(S);
    minor = load_byte(S);
    patch = load_byte(S);
    if(major != TEA_VERSION_MAJOR || minor != TEA_VERSION_MINOR)
        error(S, "version mismatch");
    if(load_byte(S) != TEA_BYTECODE_FORMAT)
        error(S, "format mismatch");
}

TeaObjectModule* tea_undump(TeaState* T, const char* name, FILE* file)
{
    TeaLoadState S;
    S.T = T;
    S.file = file;
    S.name = name;
    check_header(&S);
    TeaObjectFunction* f = load_function(&S);
    return NULL;
}