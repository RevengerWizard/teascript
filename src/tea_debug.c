/*
** tea_debug.c
** Teascript debug functions
*/

#define tea_debug_c
#define TEA_CORE

#include <stdio.h>

#include "tea_debug.h"
#include "tea_state.h"
#include "tea_func.h"
#include "tea_bc.h"

static void debug_object(TValue object)
{
    switch(OBJECT_TYPE(object))
    {
        case OBJ_CLASS:
            printf("<class %s>", AS_CLASS(object)->name->chars);
            break;
        case OBJ_FUNC:
        {
            if(AS_FUNC(object)->proto->name == NULL)
                printf("<script>");
            else
                printf("<function>");
            break;
        }
        case OBJ_PROTO:
        {
            if(AS_PROTO(object)->name == NULL)
                printf("<script>");
            else
                printf("<function>");
            break;
        }
        case OBJ_CFUNC:
            printf("<native>");
            break;
        case OBJ_INSTANCE:
            printf("<instance>");
            break;
        case OBJ_LIST:
            printf("<list>");
            break;
        case OBJ_MAP:
            printf("<map>");
            break;
        case OBJ_MODULE:
            printf("<module>");
            break;
        case OBJ_RANGE:
            printf("<range>");
            break;
        case OBJ_STRING:
        {
            GCstr* string = AS_STRING(object);
            if(string->len > 40)
                printf("<string>");
            else
                printf("%s", AS_CSTRING(object));
            break;
        }
        case OBJ_UPVALUE:
            printf("<upvalue>");
            break;
        default:
            printf("<unknown>");
            break;
    }
}

void tea_debug_value(TValue value)
{
    if(IS_BOOL(value))
    {
        AS_BOOL(value) ? printf("true") : printf("false");
    }
    else if(IS_NULL(value))
    {
        printf("null");
    }
    else if(IS_NUMBER(value))
    {
        printf(TEA_NUMBER_FMT, AS_NUMBER(value));
    }
    else if(IS_OBJECT(value))
    {
        debug_object(value);
    }
}

void tea_debug_chunk(tea_State* T, GCproto* f, const char* name)
{
    printf("== '%s' ==\n", name);

    for(int offset = 0; offset < f->bc_count;)
    {
        offset = tea_debug_instruction(T, f, offset);
    }
}

static int debug_constant(GCproto* f, int offset)
{
    uint8_t constant = f->bc[offset + 1];
    printf("%4d '", constant);
    tea_debug_value(f->k[constant]);
    printf("'\n");

    return offset + 2;
}

static int debug_invoke(GCproto* f, int offset)
{
    uint8_t constant = f->bc[offset + 1];
    uint8_t arg_count = f->bc[offset + 2];
    printf("   (%d args) %4d '", arg_count, constant);
    tea_debug_value(f->k[constant]);
    printf("'\n");

    return offset + 3;
}

static int debug_simple(int offset)
{
    putchar('\n');

    return offset + 1;
}

static int debug_byte(GCproto* f, int offset)
{
    uint8_t slot = f->bc[offset + 1];
    printf("%4d\n", slot);

    return offset + 2;
}

static int debug_iter(GCproto* f, int offset)
{
    uint8_t seq = f->bc[offset + 1];
    uint8_t iter = f->bc[offset + 2];
    printf("%4d     %4d\n", seq, iter);
    
    return offset + 3;
}

static int debug_jump(int sign, GCproto* f, int offset)
{
    uint16_t jump = (uint16_t)(f->bc[offset + 1] << 8);
    jump |= f->bc[offset + 2];
    printf("%4d -> %d\n", offset, offset + 3 + sign * jump);

    return offset + 3;
}

void tea_debug_stack(tea_State* T)
{
    printf("          ");
    for(TValue* slot = T->stack; slot < T->top; slot++)
    {
        if(slot == T->base)
            printf("[ ^");
        else
            printf("[ ");

        tea_debug_value(*slot);
        printf(" ]");
    }
    printf("\n");
}

int tea_debug_instruction(tea_State* T, GCproto* f, int offset)
{
    printf("%04d ", offset);
    int line = tea_func_getline(f, offset);
    if(offset > 0 && line == tea_func_getline(f, offset - 1))
    {
        printf("   | ");
    }
    else
    {
        printf("%4d ", line);
    }

    uint8_t instruction = f->bc[offset];
    if(instruction < BC_END)
    {
        printf("%-16s ", tea_bcnames[instruction]);
    }
    else
    {
        printf("Unknown opcode %d\n", instruction);
    }

    switch(instruction)
    {
        case BC_CONSTANT:
        case BC_PUSH_ATTR:
        case BC_SET_CLASS_VAR:
        case BC_GET_GLOBAL:
        case BC_SET_GLOBAL:
        case BC_GET_MODULE:
        case BC_SET_MODULE:
        case BC_DEFINE_OPTIONAL:
        case BC_DEFINE_GLOBAL:
        case BC_DEFINE_MODULE:
        case BC_GET_ATTR:
        case BC_SET_ATTR:
        case BC_GET_SUPER:
        case BC_CLASS:
        case BC_METHOD:
        case BC_EXTENSION_METHOD:
        case BC_IMPORT_STRING:
        case BC_IMPORT_NAME:
        case BC_IMPORT_VARIABLE:
            return debug_constant(f, offset);
        case BC_FOR_ITER:
        case BC_GET_ITER:
            return debug_iter(f, offset);
        case BC_NULL:
        case BC_TRUE:
        case BC_FALSE:
        case BC_POP:
        case BC_PRINT:
        case BC_RANGE:
        case BC_LIST:
        case BC_MAP:
        case BC_GET_INDEX:
        case BC_SET_INDEX:
        case BC_PUSH_INDEX:
        case BC_LIST_ITEM:
        case BC_MAP_FIELD:
        case BC_EQUAL:
        case BC_IS:
        case BC_IN:
        case BC_GREATER:
        case BC_GREATER_EQUAL:
        case BC_LESS:
        case BC_LESS_EQUAL:
        case BC_ADD:
        case BC_SUBTRACT:
        case BC_MULTIPLY:
        case BC_DIVIDE:
        case BC_MOD:
        case BC_POW:
        case BC_BAND:
        case BC_BOR:
        case BC_BNOT:
        case BC_BXOR:
        case BC_LSHIFT:
        case BC_RSHIFT:
        case BC_NOT:
        case BC_NEGATE:
        case BC_CLOSE_UPVALUE:
        case BC_RETURN:
        case BC_INHERIT:
        case BC_IMPORT_ALIAS:
        case BC_IMPORT_END:
        case BC_END:
            return debug_simple(offset);
        case BC_GET_LOCAL:
        case BC_SET_LOCAL:
        case BC_GET_UPVALUE:
        case BC_SET_UPVALUE:
        case BC_MULTI_CASE:
        case BC_UNPACK:
        case BC_UNPACK_REST:
        case BC_CALL:
            return debug_byte(f, offset);
        case BC_COMPARE_JUMP:
        case BC_JUMP:
        case BC_JUMP_IF_FALSE:
        case BC_JUMP_IF_NULL:
            return debug_jump(1, f, offset);
        case BC_LOOP:
            return debug_jump(-1, f, offset);
        case BC_INVOKE:
        case BC_SUPER:
            return debug_invoke(f, offset);
        case BC_CLOSURE:
        {
            offset++;
            uint8_t constant = f->bc[offset++];
            printf("%4d ", constant);
            tea_debug_value(f->k[constant]);
            printf("\n");

            GCproto* proto = AS_PROTO(f->k[constant]);
            for(int j = 0; j < proto->upvalue_count; j++)
            {
                int is_local = f->bc[offset++];
                int index = f->bc[offset++];
                printf("%04d    |                     %s %d\n", offset - 2, is_local ? "local" : "upvalue", index);
            }

            return offset;
        }
        default:
            return offset + 1;
    }
}