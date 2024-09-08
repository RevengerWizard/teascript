/*
** tea_bc.h
** Bytecode instructions
*/

#ifndef _TEA_BC_H
#define _TEA_BC_H

#include "tea_def.h"

/* 
** Bytecode instruction definition
**
** (name, stack effect)
*/
#define BCDEF(_) \
    _(CONSTANT, 1) \
    _(NIL, 1) \
    _(TRUE, 1) \
    _(FALSE, 1) \
    _(POP, -1) \
    _(GET_LOCAL, 1) \
    _(SET_LOCAL, 0) \
    _(GET_MODULE, 1) \
    _(SET_MODULE, 0) \
    _(DEFINE_OPTIONAL, 0) \
    _(DEFINE_MODULE, 0) \
    _(GET_UPVALUE, 1) \
    _(SET_UPVALUE, 0) \
    _(GET_ATTR, 1) \
    _(PUSH_ATTR, 0) \
    _(SET_ATTR, 0) \
    _(GET_SUPER, 1) \
    _(RANGE, -2) \
    _(LIST, 1) \
    _(UNPACK, -1) \
    _(UNPACK_REST, -1) \
    _(LIST_EXTEND, -1) \
    _(MAP, 1) \
    _(GET_INDEX, -1) \
    _(PUSH_INDEX, 1) \
    _(SET_INDEX, 0) \
    _(LIST_ITEM, -1) \
    _(MAP_FIELD, -2) \
    _(IS, -1) \
    _(IN, -1) \
    _(EQUAL, -1) \
    _(GREATER, -1) \
    _(GREATER_EQUAL, -1) \
    _(LESS, -1) \
    _(LESS_EQUAL, -1) \
    _(ADD, -1) \
    _(SUBTRACT, -1) \
    _(MULTIPLY, -1) \
    _(DIVIDE, -1) \
    _(MOD, -1) \
    _(POW, -1) \
    _(BAND, -1) \
    _(BOR, -1) \
    _(BNOT, 1) \
    _(BXOR, -1) \
    _(LSHIFT, -1) \
    _(RSHIFT, -1) \
    _(NOT, 0) \
    _(NEGATE, 0) \
    _(MULTI_CASE, 0) \
    _(COMPARE_JUMP, 0) \
    _(JUMP, 0) \
    _(JUMP_IF_FALSE, -1) \
    _(JUMP_IF_NIL, -1) \
    _(LOOP, 0) \
    _(INVOKE_NEW, 0) \
    _(CALL, 0) \
    _(INVOKE, 0) \
    _(SUPER, 0) \
    _(CLOSURE, 1) \
    _(CLOSE_UPVALUE, -1) \
    _(RETURN, 0) \
    _(GET_ITER, 1) \
    _(FOR_ITER, 1) \
    _(CLASS, 1) \
    _(INHERIT, 0) \
    _(METHOD, -1) \
    _(EXTENSION_METHOD, 0) \
    _(IMPORT_STRING, 0) \
    _(IMPORT_NAME, 0) \
    _(IMPORT_VARIABLE, 1) \
    _(IMPORT_ALIAS, 1) \
    _(IMPORT_END, 1) \
    _(END, 0)

/* Bytecode opcode numbers */
typedef enum
{
#define BCENUM(name, _) BC_##name,
    BCDEF(BCENUM)
#undef BCENUM
} BCOp;

TEA_DATA const char* const tea_bcnames[];

#endif