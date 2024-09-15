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

/* Bytecode dump header "\033Tea" */
#define BCDUMP_HEAD1    0x1b
#define BCDUMP_HEAD2    0x54
#define BCDUMP_HEAD3    0x65
#define BCDUMP_HEAD4    0x61

#define BCDUMP_VERSION    0

/* Bytecode flags */
#define BCDUMP_F_BE     0x01
#define BCDUMP_F_STRIP  0x02

/* Type codes for the GC constants of a prototype */
#define BCDUMP_KGC_NUM 0
#define BCDUMP_KGC_FUNC 1
#define BCDUMP_KGC_STR 5

/* -- Bytecode reader/writer ---------------------------------------------- */

TEA_FUNC int tea_bcwrite(tea_State* T, GCproto* pt, tea_Writer writer, void* data, uint32_t flags);
TEA_FUNC GCproto* tea_bcread(LexState* ls);

#endif