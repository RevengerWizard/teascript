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
    /* Stack ops */ \
    _(GETLOCAL, 1, 1) \
    _(SETLOCAL, 0, 1) \
    _(CONSTANT, 1, 1) \
    _(POP, -1, 0) \
    \
    /* Constant ops */ \
    _(KNIL, 1, 0) \
    _(KTRUE, 1, 0) \
    _(KFALSE, 1, 0) \
    \
    /* Function calls */ \
    _(CALL, 0, 1) \
    _(INVOKE, 0, 2) \
    _(NEW, 0, 1) \
    _(SUPER, 0, 2) \
    _(RETURN, 0, 0) \
    \
    /* Arithmetic ops */ \
    _(ADD, -1, 0) \
    _(SUB, -1, 0) \
    _(MUL, -1, 0) \
    _(DIV, -1, 0) \
    _(NEG, 0, 0) \
    \
    /* Comparison ops */ \
    _(ISEQ, -1, 0) \
    _(ISLT, -1, 0) \
    _(ISLE, -1, 0) \
    _(ISGT, -1, 0) \
    _(ISGE, -1, 0) \
    \
    /* Control flow */ \
    _(JMP, 0, 2) \
    _(JMPFALSE, -1, 2) \
    _(JMPNIL, -1, 2) \
    _(LOOP, 0, 2) \
    \
    /* Collection ops */ \
    _(LIST, 1, 0) \
    _(MAP, 1, 0) \
    _(LISTITEM, -1, 0) \
    _(MAPFIELD, -2, 0) \
    _(RANGE, -2, 0) \
    _(UNPACK, -1, 1) \
    _(UNPACKREST, -1, 2) \
    _(LISTEXTEND, -1, 0) \
    _(SPREAD, 0, 3) \
    \
    /* Object access */ \
    _(GETATTR, 1, 1) \
    _(PUSHATTR, 0, 1) \
    _(SETATTR, 0, 1) \
    _(GETIDX, -1, 0) \
    _(PUSHIDX, 1, 0) \
    _(SETIDX, 0, 0) \
    _(GETSUPER, 1, 1) \
    \
    /* Global/module access */ \
    _(GETGLOBAL, 1, 1) \
    _(GETMODULE, 1, 1) \
    _(SETMODULE, 0, 1) \
    _(DEFMODULE, 0, 2) \
    \
    /* Closure and upvalue ops */ \
    _(CLOSURE, 1, 1) \
    _(CLOSEUPVAL, -1, 0) \
    _(GETUPVAL, 1, 1) \
    _(SETUPVAL, 0, 1) \
    \
    /* Other ops */ \
    _(MOD, -1, 0) \
    _(POW, -1, 0) \
    _(NOT, 0, 0) \
    _(IS, -1, 0) \
    _(IN, -1, 0) \
    \
    /* Bitwise ops */ \
    _(BAND, -1, 0) \
    _(BOR, -1, 0) \
    _(BNOT, 1, 0) \
    _(BXOR, -1, 0) \
    _(LSHIFT, -1, 0) \
    _(RSHIFT, -1, 0) \
    \
    /* Iterator ops */ \
    _(GETITER, 1, 1) \
    _(FORITER, 1, 1) \
    \
    /* Class ops */ \
    _(CLASS, 1, 1) \
    _(METHOD, -1, 2) \
    _(INHERIT, 0, 0) \
    _(ISTYPE, 0, 0) \
    \
    /* Import ops */ \
    _(IMPORTNAME, 0, 1) \
    _(IMPORTSTR, 0, 1) \
    _(IMPORTFMT, 0, 0) \
    _(IMPORTVAR, 1, 2) \
    _(IMPORTALIAS, 1, 0) \
    _(IMPORTEND, 1, 0) \
    \
    /* Special cases */ \
    _(DEFOPT, 0, 2) \
    _(MULTICASE, 0, 1) \
    _(JMPCMP, 0, 2) \
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