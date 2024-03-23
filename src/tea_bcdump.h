/*
** tea_bcdump.h
** Bytecode dump definitions
*/

#ifndef _TEA_BCDUMP_H
#define _TEA_BCDUMP_H

#include "tea.h"

#include "tea_state.h"
#include "tea_obj.h"
#include "tea_lex.h"

/* -- Bytecode dump format ------------------------------------------------ */

/* Type codes for the GC constants of a prototype */
#define BCDUMP_KGC_NUM 0
#define BCDUMP_KGC_FUNC 1
#define BCDUMP_KGC_STR 5

/* Bytecode dump header "\033Tea" */
#define BCDUMP_HEAD1    0x1b
#define BCDUMP_HEAD2    0x54
#define BCDUMP_HEAD3    0x65
#define BCDUMP_HEAD4    0x61

#define BCDUMP_VERSION    0

/* -- Bytecode reader/writer ---------------------------------------------- */

TEA_FUNC int tea_bcwrite(tea_State* T, GCproto* proto, tea_Writer writer, void* data);
TEA_FUNC GCproto* tea_bcread(Lexer* lex);

#endif