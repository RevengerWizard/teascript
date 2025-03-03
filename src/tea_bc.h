/*
** tea_bc.h
** Bytecode instructions
*/

#ifndef _TEA_BC_H
#define _TEA_BC_H

#include "tea_def.h"

/* 
** Bytecode instruction definition. Order matters
**
** (name, stack effect, byte count)
*/
#define BCDEF(_) \
    /* Stack ops. */ \
    _(GET_LOCAL, 1, 1) \
    _(SET_LOCAL, 0, 1) \
    _(CONSTANT, 1, 1) \
    _(POP, -1, 0) \
    \
    /* Constant ops. */ \
    _(NIL, 1, 0) \
    _(TRUE, 1, 0) \
    _(FALSE, 1, 0) \
    \
    /* Function calls */ \
    _(CALL, 0, 1) \
    _(INVOKE, 0, 2) \
    _(INVOKE_NEW, 0, 1) \
    _(SUPER, 0, 2) \
    _(RETURN, 0, 0) \
    \
    /* Arithmetic ops. */ \
    _(ADD, -1, 0) \
    _(SUBTRACT, -1, 0) \
    _(MULTIPLY, -1, 0) \
    _(DIVIDE, -1, 0) \
    _(NEGATE, 0, 0) \
    \
    /* Comparison ops. */ \
    _(EQUAL, -1, 0) \
    _(LESS, -1, 0) \
    _(LESS_EQUAL, -1, 0) \
    _(GREATER, -1, 0) \
    _(GREATER_EQUAL, -1, 0) \
    \
    /* Control flow */ \
    _(JUMP, 0, 2) \
    _(JUMP_IF_FALSE, -1, 2) \
    _(JUMP_IF_NIL, -1, 2) \
    _(LOOP, 0, 2) \
    \
    /* Collection ops. */ \
    _(LIST, 1, 0) \
    _(MAP, 1, 0) \
    _(LIST_ITEM, -1, 0) \
    _(MAP_FIELD, -2, 0) \
    _(RANGE, -2, 0) \
    _(UNPACK, -1, 1) \
    _(UNPACK_REST, -1, 2) \
    _(LIST_EXTEND, -1, 0) \
    \
    /* Object access */ \
    _(GET_ATTR, 1, 1) \
    _(PUSH_ATTR, 0, 1) \
    _(SET_ATTR, 0, 1) \
    _(GET_INDEX, -1, 0) \
    _(PUSH_INDEX, 1, 0) \
    _(SET_INDEX, 0, 0) \
    _(GET_SUPER, 1, 1) \
    \
    /* Global/module access */ \
    _(GET_GLOBAL, 1, 1) \
    _(GET_MODULE, 1, 1) \
    _(SET_MODULE, 0, 1) \
    _(DEFINE_MODULE, 0, 2) \
    \
    /* Closure and upvalue ops. */ \
    _(CLOSURE, 1, 1) \
    _(CLOSE_UPVALUE, -1, 0) \
    _(GET_UPVALUE, 1, 1) \
    _(SET_UPVALUE, 0, 1) \
    \
    /* Other ops. */ \
    _(MOD, -1, 0) \
    _(POW, -1, 0) \
    _(NOT, 0, 0) \
    _(IS, -1, 0) \
    _(IN, -1, 0) \
    \
    /* Bitwise ops.*/ \
    _(BAND, -1, 0) \
    _(BOR, -1, 0) \
    _(BNOT, 1, 0) \
    _(BXOR, -1, 0) \
    _(LSHIFT, -1, 0) \
    _(RSHIFT, -1, 0) \
    \
    /* Iterator ops. */ \
    _(GET_ITER, 1, 2) \
    _(FOR_ITER, 1, 2) \
    \
    /* OOP ops. */ \
    _(CLASS, 1, 1) \
    _(METHOD, -1, 2) \
    _(INHERIT, 0, 0) \
    _(ISTYPE, 0, 0) \
    \
    /* Import ops. */ \
    _(IMPORT_NAME, 0, 1) \
    _(IMPORT_STRING, 0, 1) \
    _(IMPORT_FMT, 0, 0) \
    _(IMPORT_VARIABLE, 1, 2) \
    _(IMPORT_ALIAS, 1, 0) \
    _(IMPORT_END, 1, 0) \
    \
    /* Special cases */ \
    _(DEFINE_OPTIONAL, 0, 2) \
    _(MULTI_CASE, 0, 1) \
    _(COMPARE_JUMP, 0, 2) \
    _(END, 0, 0)

/* Bytecode opcode numbers */
typedef enum
{
#define BCENUM(name, _, __) BC_##name,
    BCDEF(BCENUM)
#undef BCENUM
} BCOp;

TEA_DATA const char* const tea_bcnames[];

#endif