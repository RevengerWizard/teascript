/*
** tea_debug.c
** Teascript debug functions
*/

#include <stdio.h>

#define tea_debug_c
#define TEA_CORE

#include "tea_debug.h"
#include "tea_object.h"
#include "tea_value.h"
#include "tea_state.h"

static const char* const opnames[] = {
    "CONSTANT",
    "NULL",
    "TRUE",
    "FALSE",
    "POP",
    "POP_REPL",
    "GET_LOCAL",
    "SET_LOCAL",
    "GET_GLOBAL",
    "SET_GLOBAL",
    "GET_MODULE",
    "SET_MODULE",
    "DEFINE_OPTIONAL",
    "DEFINE_GLOBAL",
    "DEFINE_MODULE",
    "GET_UPVALUE",
    "SET_UPVALUE",
    "GET_PROPERTY",
    "GET_PROPERTY_NO_POP",
    "SET_PROPERTY",
    "GET_SUPER",
    "RANGE",
    "LIST",
    "UNPACK_LIST",
    "UNPACK_REST_LIST",
    "MAP",
    "SUBSCRIPT",
    "SUBSCRIPT_STORE",
    "SUBSCRIPT_PUSH",
    "SLICE",
    "PUSH_LIST_ITEM",
    "PUSH_MAP_FIELD",
    "IS",
    "IN",
    "EQUAL",
    "GREATER",
    "GREATER_EQUAL",
    "LESS",
    "LESS_EQUAL",
    "ADD",
    "SUBTRACT",
    "MULTIPLY",
    "DIVIDE",
    "MOD",
    "POW",
    "BAND",
    "BOR",
    "BNOT",
    "BXOR",
    "LSHIFT",
    "RSHIFT",
    "AND",
    "OR",
    "NOT",
    "NEGATE",
    "MULTI_CASE",
    "COMPARE_JUMP",
    "JUMP",
    "JUMP_IF_FALSE",
    "JUMP_IF_NULL",
    "LOOP",
    "CALL",
    "INVOKE",
    "SUPER",
    "CLOSURE",
    "CLOSE_UPVALUE",
    "RETURN",
    "GET_ITER",
    "FOR_ITER",
    "CLASS",
    "SET_CLASS_VAR",
    "INHERIT",
    "METHOD",
    "EXTENSION_METHOD",
    "IMPORT_STRING",
    "IMPORT_NAME",
    "IMPORT_VARIABLE",
    "IMPORT_ALIAS",
    "IMPORT_END",
    "END",
    NULL
};

static void print_object(TeaValue object)
{
    switch(OBJECT_TYPE(object))
    {
        case OBJ_CLASS:
            printf("<class %s>", AS_CLASS(object)->name->chars);
            break;
        case OBJ_CLOSURE:
        {
            if(AS_CLOSURE(object)->function->name == NULL)
                printf("<script>");
            else
                printf("<function>");
            break;
        }
        case OBJ_FUNCTION:
        {
            if(AS_FUNCTION(object)->name == NULL)
                printf("<script>");
            else
                printf("<function>");
            break;
        }
        case OBJ_NATIVE:
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
            TeaOString* string = AS_STRING(object);
            if(string->length > 40)
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

void tea_debug_print_value(TeaValue value)
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
        print_object(value);
    }
}

void tea_debug_chunk(TeaState* T, TeaChunk* chunk, const char* name)
{
    printf("== '%s' ==\n", name);

    for(int offset = 0; offset < chunk->count;)
    {
        offset = tea_debug_instruction(T, chunk, offset);
    }
}

static int constant_instruction(TeaChunk* chunk, int offset)
{
    uint8_t constant = chunk->code[offset + 1];
    printf("%4d '", constant);
    tea_debug_print_value(chunk->constants.values[constant]);
    printf("'\n");

    return offset + 2;
}

static int invoke_instruction(TeaChunk* chunk, int offset)
{
    uint8_t constant = chunk->code[offset + 1];
    uint8_t arg_count = chunk->code[offset + 2];
    printf("   (%d args) %4d '", arg_count, constant);
    tea_debug_print_value(chunk->constants.values[constant]);
    printf("'\n");

    return offset + 3;
}

static int simple_instruction(int offset)
{
    putchar('\n');

    return offset + 1;
}

static int byte_instruction(TeaChunk* chunk, int offset)
{
    uint8_t slot = chunk->code[offset + 1];
    printf("%4d\n", slot);

    return offset + 2;
}

static int iter_instruction(TeaChunk* chunk, int offset)
{
    uint8_t seq = chunk->code[offset + 1];
    uint8_t iter = chunk->code[offset + 2];
    printf("%4d     %4d\n", seq, iter);
    
    return offset + 3;
}

static int jump_instruction(int sign, TeaChunk* chunk, int offset)
{
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%4d -> %d\n", offset, offset + 3 + sign * jump);

    return offset + 3;
}

void tea_debug_stack(TeaState* T)
{
    printf("          ");
    for(TeaValue* slot = T->stack; slot < T->top; slot++)
    {
        if(slot == T->base)
            printf("[ ^");
        else
            printf("[ ");

        tea_debug_print_value(*slot);
        printf(" ]");
    }
    printf("\n");
}

int tea_debug_instruction(TeaState* T, TeaChunk* chunk, int offset)
{
    printf("%04d ", offset);
    if(offset > 0 && tea_chunk_getline(chunk, offset - 1))
    {
        printf("   | ");
    }
    else
    {
        printf("%4d ", tea_chunk_getline(chunk, offset));
    }

    uint8_t instruction = chunk->code[offset];
    if(instruction < OP_END)
    {
        printf("%-16s ", opnames[instruction]);
    }
    else
    {
        printf("Unknown opcode %d\n", instruction);
    }

    switch(instruction)
    {
        case OP_CONSTANT:
        case OP_GET_PROPERTY_NO_POP:
        case OP_SET_CLASS_VAR:
        case OP_GET_GLOBAL:
        case OP_SET_GLOBAL:
        case OP_GET_MODULE:
        case OP_SET_MODULE:
        case OP_DEFINE_OPTIONAL:
        case OP_DEFINE_GLOBAL:
        case OP_DEFINE_MODULE:
        case OP_GET_PROPERTY:
        case OP_SET_PROPERTY:
        case OP_GET_SUPER:
        case OP_CLASS:
        case OP_METHOD:
        case OP_EXTENSION_METHOD:
        case OP_IMPORT_STRING:
        case OP_IMPORT_NAME:
        case OP_IMPORT_VARIABLE:
            return constant_instruction(chunk, offset);
        case OP_FOR_ITER:
        case OP_GET_ITER:
            return iter_instruction(chunk, offset);
        case OP_NULL:
        case OP_TRUE:
        case OP_FALSE:
        case OP_POP:
        case OP_POP_REPL:
        case OP_RANGE:
        case OP_LIST:
        case OP_MAP:
        case OP_SUBSCRIPT:
        case OP_SUBSCRIPT_STORE:
        case OP_SUBSCRIPT_PUSH:
        case OP_SLICE:
        case OP_PUSH_LIST_ITEM:
        case OP_PUSH_MAP_FIELD:
        case OP_EQUAL:
        case OP_IS:
        case OP_IN:
        case OP_GREATER:
        case OP_GREATER_EQUAL:
        case OP_LESS:
        case OP_LESS_EQUAL:
        case OP_ADD:
        case OP_SUBTRACT:
        case OP_MULTIPLY:
        case OP_DIVIDE:
        case OP_MOD:
        case OP_POW:
        case OP_BAND:
        case OP_BOR:
        case OP_BNOT:
        case OP_BXOR:
        case OP_LSHIFT:
        case OP_RSHIFT:
        case OP_NOT:
        case OP_NEGATE:
        case OP_CLOSE_UPVALUE:
        case OP_RETURN:
        case OP_INHERIT:
        case OP_IMPORT_ALIAS:
        case OP_IMPORT_END:
        case OP_END:
            return simple_instruction(offset);
        case OP_GET_LOCAL:
        case OP_SET_LOCAL:
        case OP_GET_UPVALUE:
        case OP_SET_UPVALUE:
        case OP_MULTI_CASE:
        case OP_UNPACK_LIST:
        case OP_UNPACK_REST_LIST:
        case OP_CALL:
            return byte_instruction(chunk, offset);
        case OP_AND:
        case OP_OR:
        case OP_COMPARE_JUMP:
        case OP_JUMP:
        case OP_JUMP_IF_FALSE:
        case OP_JUMP_IF_NULL:
            return jump_instruction(1, chunk, offset);
        case OP_LOOP:
            return jump_instruction(-1, chunk, offset);
        case OP_INVOKE:
        case OP_SUPER:
            return invoke_instruction(chunk, offset);
        case OP_CLOSURE:
        {
            offset++;
            uint8_t constant = chunk->code[offset++];
            printf("%4d ", constant);
            tea_debug_print_value(chunk->constants.values[constant]);
            printf("\n");

            TeaOFunction* function = AS_FUNCTION(chunk->constants.values[constant]);
            for(int j = 0; j < function->upvalue_count; j++)
            {
                int is_local = chunk->code[offset++];
                int index = chunk->code[offset++];
                printf("%04d    |                     %s %d\n", offset - 2, is_local ? "local" : "upvalue", index);
            }

            return offset;
        }
        default:
            return offset + 1;
    }
}