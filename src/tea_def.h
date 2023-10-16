/*
** tea_common.h
** Teascript common defines
*/

#ifndef TEA_DEF_H
#define TEA_DEF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TEA_NAN_TAGGING

#define TEA_MAX_CALLS   1000
#define TEA_MAX_CCALLS  200

#ifdef TEA_DEBUG
#include <assert.h>
#define TEA_ASSERT(c)   assert(c)
#else
#define TEA_ASSERT(c)   ((void)0)
#endif

#ifndef _MSC_VER
#define TEA_COMPUTED_GOTO
#endif

#define UINT8_COUNT (UINT8_MAX + 1)

#define TEA_FUNC    extern
#define TEA_DATA    extern

#endif