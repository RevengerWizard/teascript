/*
** tea_debug.c
** Teascript debug functions
*/

#include <stdio.h>

#define tea_debug_c
#define TEA_CORE

#include "tea_debug.h"
#include "tea_obj.h"
#include "tea_func.h"
#include "tea_bc.h"

void tea_debug_value(TValue* v)
{
    switch(itype(v))
    {
        case TEA_TNIL:
            printf("nil");
            break;
        case TEA_TBOOL:
            boolV(v) ? printf("true") : printf("false");
            break;
        case TEA_TNUM:
            printf(TEA_NUMBER_FMT, numV(v));
            break;
        case TEA_TPOINTER:
            printf("pointer");
            break;
        case TEA_TCLASS:
            printf("<class %s>", str_data(classV(v)->name));
            break;
        case TEA_TFUNC:
        {
            GCfunc* func = funcV(v);
            if(iscfunc(func))
            {
                printf("<native>");
                break;
            }
            if(func->t.proto->name == NULL)
                printf("<script>");
            else
                printf("<function>");
            break;
        }
        case TEA_TPROTO:
        {
            if(protoV(v)->name == NULL)
                printf("<script>");
            else
                printf("<function>");
            break;
        }
        case TEA_TINSTANCE:
            printf("<instance>");
            break;
        case TEA_TLIST:
            printf("<list>");
            break;
        case TEA_TMAP:
            printf("<map>");
            break;
        case TEA_TMODULE:
            printf("<module>");
            break;
        case TEA_TRANGE:
            printf("<range>");
            break;
        case TEA_TSTR:
        {
            GCstr* str = strV(v);
            if(str->len > 40)
                printf("<string>");
            else
                printf("%s", str_data(str));
            break;
        }
        case TEA_TUPVAL:
            printf("<upvalue>");
            break;
        case TEA_TUDATA:
            printf("<userdata>");
            break;
        default:
            printf("<unknown>");
            break;
    }
}

void tea_debug_chunk(tea_State* T, GCproto* f, const char* name)
{
    printf("== " TEA_QS " ==\n", name);
    for(int ofs = 0; ofs < f->bc_count;)
    {
        ofs = tea_debug_instruction(T, f, ofs);
    }
}

static int debug_constant(GCproto* pt, int ofs)
{
    uint8_t k = pt->bc[ofs + 1];
    printf("%4d '", k);
    tea_debug_value(proto_kgc(pt, k));
    printf("'\n");
    return ofs + 2;
}

static int debug_invoke(GCproto* pt, int ofs)
{
    uint8_t k = pt->bc[ofs + 1];
    uint8_t nargs = pt->bc[ofs + 2];
    printf("   (%d args) %4d '", nargs, k);
    tea_debug_value(proto_kgc(pt, k));
    printf("'\n");
    return ofs + 3;
}

static int debug_simple(int ofs)
{
    putchar('\n');
    return ofs + 1;
}

static int debug_byte(GCproto* f, int ofs)
{
    uint8_t slot = f->bc[ofs + 1];
    printf("%4d\n", slot);
    return ofs + 2;
}

static int debug_iter(GCproto* f, int ofs)
{
    uint8_t seq = f->bc[ofs + 1];
    uint8_t iter = f->bc[ofs + 2];
    printf("%4d     %4d\n", seq, iter);
    return ofs + 3;
}

static int debug_jump(int sign, GCproto* f, int ofs)
{
    uint16_t jump = (uint16_t)(f->bc[ofs + 1] << 8);
    jump |= f->bc[ofs + 2];
    printf("%4d -> %d\n", ofs, ofs + 3 + sign * jump);
    return ofs + 3;
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

        tea_debug_value(slot);
        printf(" ]");
    }
    printf("\n");
}

int tea_debug_instruction(tea_State* T, GCproto* f, int ofs)
{
    printf("%04d ", ofs);
    int line = tea_func_getline(f, ofs);
    if(ofs > 0 && line == tea_func_getline(f, ofs - 1))
    {
        printf("   | ");
    }
    else
    {
        printf("%4d ", line);
    }

    uint8_t instruction = f->bc[ofs];
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
        case BC_GET_MODULE:
        case BC_SET_MODULE:
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
            return debug_constant(f, ofs);
        case BC_DEFINE_OPTIONAL:
        case BC_FOR_ITER:
        case BC_GET_ITER:
            return debug_iter(f, ofs);
        case BC_NIL:
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
        case BC_LIST_EXTEND:
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
            return debug_simple(ofs);
        case BC_GET_LOCAL:
        case BC_SET_LOCAL:
        case BC_GET_UPVALUE:
        case BC_SET_UPVALUE:
        case BC_MULTI_CASE:
        case BC_UNPACK:
        case BC_UNPACK_REST:
        case BC_CALL:
            return debug_byte(f, ofs);
        case BC_COMPARE_JUMP:
        case BC_JUMP:
        case BC_JUMP_IF_FALSE:
        case BC_JUMP_IF_NIL:
            return debug_jump(1, f, ofs);
        case BC_LOOP:
            return debug_jump(-1, f, ofs);
        case BC_INVOKE:
        case BC_SUPER:
            return debug_invoke(f, ofs);
        case BC_CLOSURE:
        {
            ofs++;
            uint8_t k = f->bc[ofs++];
            printf("%4d ", k);
            tea_debug_value(proto_kgc(f, k));
            printf("\n");

            GCproto* proto = protoV(proto_kgc(f, k));
            for(int j = 0; j < proto->upvalue_count; j++)
            {
                int is_local = f->bc[ofs++];
                int idx = f->bc[ofs++];
                printf("%04d    |                     %s %d\n", ofs - 2, is_local ? "local" : "upvalue", idx);
            }

            return ofs;
        }
        default:
            return ofs + 1;
    }
}