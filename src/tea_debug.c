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
                printf("'%s'", AS_CSTRING(object)); 
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
        printf("%.16g", AS_NUMBER(value));
    }
    else if(IS_OBJECT(value))
    {
        print_object(value);
    }
}

void tea_debug_chunk(TeaState* T, TeaChunk* chunk, const char* name)
{
    printf("== %s ==\n", name);

    for(int offset = 0; offset < chunk->count;)
    {
        offset = tea_debug_instruction(T, chunk, offset);
    }
}

static int constant_instruction(const char* name, TeaChunk* chunk, int offset)
{
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    tea_debug_print_value(chunk->constants.values[constant]);
    printf("'\n");

    return offset + 2;
}

static int invoke_instruction(const char* name, TeaChunk* chunk, int offset)
{
    uint8_t constant = chunk->code[offset + 1];
    uint8_t arg_count = chunk->code[offset + 2];
    printf("%-16s    (%d args) %4d '", name, arg_count, constant);
    tea_debug_print_value(chunk->constants.values[constant]);
    printf("'\n");

    return offset + 3;
}

static int native_import_instruction(const char* name, TeaChunk* chunk, int offset)
{
    uint8_t module = chunk->code[offset + 2];
    printf("%-16s '", name);
    tea_debug_print_value(chunk->constants.values[module]);
    printf("'\n");

    return offset + 3;
}

static int simple_instruction(const char* name, int offset)
{
    printf("%s\n", name);

    return offset + 1;
}

static int byte_instruction(const char* name, TeaChunk* chunk, int offset)
{
    uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);

    return offset + 2;
}

static int jump_instruction(const char* name, int sign, TeaChunk* chunk, int offset)
{
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);

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
    switch(instruction)
    {
        case OP_CONSTANT:
            return constant_instruction("OP_CONSTANT", chunk, offset);
        case OP_NULL:
            return simple_instruction("OP_NULL", offset);
        case OP_TRUE:
            return simple_instruction("OP_TRUE", offset);
        case OP_FALSE:
            return simple_instruction("OP_FALSE", offset);
        case OP_POP:
            return simple_instruction("OP_POP", offset);
        case OP_POP_REPL:
            return simple_instruction("OP_POP_REPL", offset);
        case OP_GET_PROPERTY_NO_POP:
            return constant_instruction("OP_GET_PROPERTY_NO_POP", chunk, offset);
        case OP_SET_CLASS_VAR:
            return constant_instruction("OP_SET_CLASS_VAR", chunk, offset);
        case OP_GET_LOCAL:
            return byte_instruction("OP_GET_LOCAL", chunk, offset);
        case OP_SET_LOCAL:
            return byte_instruction("OP_SET_LOCAL", chunk, offset);
        case OP_GET_GLOBAL:
            return constant_instruction("OP_GET_GLOBAL", chunk, offset);
        case OP_SET_GLOBAL:
            return constant_instruction("OP_SET_GLOBAL", chunk, offset);
        case OP_GET_MODULE:
            return constant_instruction("OP_GET_MODULE", chunk, offset);
        case OP_SET_MODULE:
            return constant_instruction("OP_SET_MODULE", chunk, offset);
        case OP_DEFINE_OPTIONAL:
            return constant_instruction("OP_DEFINE_OPTIONAL", chunk, offset);
        case OP_DEFINE_GLOBAL:
            return constant_instruction("OP_DEFINE_GLOBAL", chunk, offset);
        case OP_DEFINE_MODULE:
            return constant_instruction("OP_DEFINE_MODULE", chunk, offset);
        case OP_GET_UPVALUE:
            return byte_instruction("OP_GET_UPVALUE", chunk, offset);
        case OP_SET_UPVALUE:
            return byte_instruction("OP_SET_UPVALUE", chunk, offset);
        case OP_GET_PROPERTY:
            return constant_instruction("OP_GET_PROPERTY", chunk, offset);
        case OP_SET_PROPERTY:
            return constant_instruction("OP_SET_PROPERTY", chunk, offset);
        case OP_GET_SUPER:
            return constant_instruction("OP_GET_SUPER", chunk, offset);
        case OP_RANGE:
            return simple_instruction("OP_RANGE", offset);
        case OP_MULTI_CASE:
            return byte_instruction("OP_MULTI_CASE", chunk, offset);
        case OP_LIST:
            return byte_instruction("OP_LIST", chunk, offset);
        case OP_UNPACK_LIST:
            return byte_instruction("OP_UNPACK_LIST", chunk, offset);
        case OP_UNPACK_REST_LIST:
            return byte_instruction("UNPACK_REST_LIST", chunk, offset);
        case OP_MAP:
            return byte_instruction("OP_MAP", chunk, offset);
        case OP_SUBSCRIPT:
            return simple_instruction("OP_SUBSCRIPT", offset);
        case OP_SUBSCRIPT_STORE:
            return simple_instruction("OP_SUBSCRIPT_STORE", offset);
        case OP_SUBSCRIPT_PUSH:
            return simple_instruction("OP_SUBSCRIPT_PUSH", offset);
        case OP_EQUAL:
            return simple_instruction("OP_EQUAL", offset);
        case OP_IS:
            return simple_instruction("OP_IS", offset);
        case OP_IN:
            return simple_instruction("OP_IN", offset);
        case OP_GREATER:
            return simple_instruction("OP_GREATER", offset);
        case OP_GREATER_EQUAL:
            return simple_instruction("OP_GREATER_EQUAL", offset);
        case OP_LESS:
            return simple_instruction("OP_LESS", offset);
        case OP_LESS_EQUAL:
            return simple_instruction("OP_LESS_EQUAL", offset);
        case OP_ADD:
            return simple_instruction("OP_ADD", offset);
        case OP_SUBTRACT:
            return simple_instruction("OP_SUBTRACT", offset);
        case OP_MULTIPLY:
            return simple_instruction("OP_MULTIPLY", offset);
        case OP_DIVIDE:
            return simple_instruction("OP_DIVIDE", offset);
        case OP_MOD:
            return simple_instruction("OP_MOD", offset);
        case OP_POW:
            return simple_instruction("OP_POW", offset);
        case OP_BAND:
            return simple_instruction("OP_BAND", offset);
        case OP_BOR:
            return simple_instruction("OP_BOR", offset);
        case OP_BNOT:
            return simple_instruction("OP_BNOT", offset);
        case OP_BXOR:
            return simple_instruction("OP_BXOR", offset);
        case OP_LSHIFT:
            return simple_instruction("OP_LSHIFT", offset);
        case OP_RSHIFT:
            return simple_instruction("OP_RSHIFT", offset);
        case OP_NOT:
            return simple_instruction("OP_NOT", offset);
        case OP_NEGATE:
            return simple_instruction("OP_NEGATE", offset);
        case OP_AND:
            return jump_instruction("OP_AND", 1, chunk, offset);
        case OP_OR:
            return jump_instruction("OP_OR", 1, chunk, offset);
        case OP_COMPARE_JUMP:
            return jump_instruction("OP_COMPARE_JUMP", 1, chunk, offset);
        case OP_JUMP:
            return jump_instruction("OP_JUMP", 1, chunk, offset);
        case OP_JUMP_IF_FALSE:
            return jump_instruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_JUMP_IF_NULL:
            return jump_instruction("OP_JUMP_IF_NULL", 1, chunk, offset);
        case OP_LOOP:
            return jump_instruction("OP_LOOP", -1, chunk, offset);
        case OP_CALL:
            return byte_instruction("OP_CALL", chunk, offset);
        case OP_INVOKE:
            return invoke_instruction("OP_INVOKE", chunk, offset);
        case OP_SUPER:
            return invoke_instruction("OP_SUPER", chunk, offset);
        case OP_CLOSURE:
        {
            offset++;
            uint8_t constant = chunk->code[offset++];
            printf("%-16s %4d ", "OP_CLOSURE", constant);
            printf("%s", tea_val_tostring(T, chunk->constants.values[constant])->chars);
            printf("\n");

            TeaOFunction* function = AS_FUNCTION(chunk->constants.values[constant]);
            for(int j = 0; j < function->upvalue_count; j++)
            {
                int is_local = chunk->code[offset++];
                int index = chunk->code[offset++];
                printf("%04d      |                     %s %d\n", offset - 2, is_local ? "local" : "upvalue", index);
            }

            return offset;
        }
        case OP_CLOSE_UPVALUE:
            return simple_instruction("OP_CLOSE_UPVALUE", offset);
        case OP_RETURN:
            return simple_instruction("OP_RETURN", offset);
        case OP_CLASS:
            return constant_instruction("OP_CLASS", chunk, offset);
        case OP_INHERIT:
            return simple_instruction("OP_INHERIT", offset);
        case OP_METHOD:
            return constant_instruction("OP_METHOD", chunk, offset);
        case OP_EXTENSION_METHOD:
            return constant_instruction("OP_EXTENSION_METHOD", chunk, offset);
        case OP_IMPORT_STRING:
            return constant_instruction("OP_IMPORT_STRING", chunk, offset);
        case OP_IMPORT_NAME:
            return native_import_instruction("OP_IMPORT_NAME", chunk, offset);
        case OP_IMPORT_VARIABLE:
            return constant_instruction("OP_IMPORT_VARIABLE", chunk, offset);
        case OP_IMPORT_ALIAS:
            return simple_instruction("OP_IMPORT_ALIAS", offset);
        case OP_IMPORT_END:
            return simple_instruction("OP_IMPORT_END", offset);
        case OP_END:
            return simple_instruction("OP_END", offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}