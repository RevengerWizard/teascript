/*
** onetea.c
** Teascript core, libraries and interpreter in a single file
**
** gcc -O2 -std=c99 -o onetea onetea.c -lm
*/

#define onetea_c
#define TEA_CORE

#include "tea.h"

#include "tea_api.c"
#include "tea_chunk.c"
#include "tea_compiler.c"
#include "tea_core.c"
#include "tea_debug.c"
#include "tea_do.c"
#include "tea_gc.c"
#include "tea_import.c"
#include "tea_memory.c"
#include "tea_object.c"
#include "tea_scanner.c"
#include "tea_state.c"
#include "tea_table.c"
#include "tea_utf.c"
#include "tea_util.c"
#include "tea_value.c"
#include "tea_vm.c"

#include "tea_fileclass.c"
#include "tea_listclass.c"
#include "tea_mapclass.c"
#include "tea_rangeclass.c"
#include "tea_stringclass.c"
#include "tea_iolib.c"
#include "tea_oslib.c"
#include "tea_randomlib.c"
#include "tea_mathlib.c"
#include "tea_syslib.c"
#include "tea_timelib.c"

#include "tea.c"