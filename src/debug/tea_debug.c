#include <stdio.h>

#include "debug/tea_debug.h"
#include "vm/tea_object.h"
#include "vm/tea_value.h"

void tea_disassemble_chunk(TeaChunk* chunk, const char* name)
{
    printf("== %s ==\n", name);

    for(int offset = 0; offset < chunk->count;)
    {
        offset = tea_disassemble_instruction(chunk, offset);
    }
}

static int constant_instruction(const char* name, TeaChunk* chunk, int offset)
{
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    tea_print_value(chunk->constants.values[constant]);
    printf("'\n");

    return offset + 2;
}

static int long_constant_instruction(const char* name, TeaChunk* chunk, int offset)
{
    uint32_t constant = chunk->code[offset + 1] | (chunk->code[offset + 2] << 8) | (chunk->code[offset + 3] << 16);
    printf("%-16s %4d '", name, constant);
    tea_print_value(chunk->constants.values[constant]);
    printf("'\n");

    return offset + 4;
}

static int invoke_instruction(const char* name, TeaChunk* chunk, int offset)
{
    uint8_t constant = chunk->code[offset + 1];
    uint8_t arg_count = chunk->code[offset + 2];
    printf("%-16s (%d args) %4d '", name, arg_count, constant);
    tea_print_value(chunk->constants.values[constant]);
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

int tea_disassemble_instruction(TeaChunk* chunk, int offset)
{
    printf("%04d ", offset);
    int line = tea_get_line(chunk, offset);
    if(offset > 0 && line == tea_get_line(chunk, offset - 1))
    {
        printf("   | ");
    }
    else
    {
        printf("%4d ", line);
    }

    uint8_t instruction = chunk->code[offset];
    switch(instruction)
    {
        case OP_CONSTANT:
            return constant_instruction("OP_CONSTANT", chunk, offset);
        case OP_CONSTANT_LONG:
            return long_constant_instruction("OP_CONSTANT_LONG", chunk, offset);
        case OP_NULL:
            return simple_instruction("OP_NULL", offset);
        case OP_TRUE:
            return simple_instruction("OP_TRUE", offset);
        case OP_FALSE:
            return simple_instruction("OP_FALSE", offset);
        case OP_POP:
            return simple_instruction("OP_POP", offset);
        case OP_GET_LOCAL:
            return byte_instruction("OP_GET_LOCAL", chunk, offset);
        case OP_SET_LOCAL:
            return byte_instruction("OP_SET_LOCAL", chunk, offset);
        case OP_GET_GLOBAL:
            return constant_instruction("OP_GET_GLOBAL", chunk, offset);
        case OP_DEFINE_GLOBAL:
            return constant_instruction("OP_DEFINE_GLOBAL", chunk, offset);
        case OP_SET_GLOBAL:
            return constant_instruction("OP_SET_GLOBAL", chunk, offset);
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
        case OP_LIST:
            return simple_instruction("OP_LIST", offset);
        case OP_SUBSCRIPT:
            return simple_instruction("OP_SUBSCRIPT", offset);
        case OP_SUBSCRIPT_STORE:
            return simple_instruction("OP_SUBSCRIPT_STORE", offset);
        case OP_EQUAL:
            return simple_instruction("OP_EQUAL", offset);
        case OP_GREATER:
            return simple_instruction("OP_GREATER", offset);
        case OP_LESS:
            return simple_instruction("OP_LESS", offset);
        case OP_ADD:
            return simple_instruction("OP_ADD", offset);
        //case OP_SUBTRACT:
        //    return simple_instruction("OP_SUBTRACT", offset);
        case OP_MULTIPLY:
            return simple_instruction("OP_MULTIPLY", offset);
        case OP_DIVIDE:
            return simple_instruction("OP_DIVIDE", offset);
        case OP_NOT:
            return simple_instruction("OP_NOT", offset);
        case OP_NEGATE:
            return simple_instruction("OP_NEGATE", offset);
        //case OP_PRINT:
        //    return simple_instruction("OP_PRINT", offset);
        case OP_JUMP:
            return jump_instruction("OP_JUMP", 1, chunk, offset);
        case OP_JUMP_IF_FALSE:
            return jump_instruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
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
            tea_print_value(chunk->constants.values[constant]);
            printf("\n");

            TeaObjectFunction* function = AS_FUNCTION(chunk->constants.values[constant]);
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
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}