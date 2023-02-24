// tea_common.h
// Teascript commons

#ifndef TEA_COMMON_H
#define TEA_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TEA_NAN_TAGGING
#define TEA_DEBUG_PRINT_CODE
#define TEA_DEBUG_TRACE_EXECUTION
#define TEA_DEBUG_TRACE_MEMORY
#define TEA_DEBUG_FINAL_MEMORY

#define TEA_DEBUG_STRESS_GC
#define TEA_DEBUG_LOG_GC

#ifndef _MSC_VER
#define TEA_COMPUTED_GOTO
#endif

#define UINT8_COUNT (UINT8_MAX + 1)

#endif

#undef TEA_DEBUG_PRINT_CODE
#undef TEA_DEBUG_TRACE_EXECUTION
#undef TEA_DEBUG_TRACE_MEMORY
#undef TEA_DEBUG_FINAL_MEMORY
#undef TEA_DEBUG_STRESS_GC
#undef TEA_DEBUG_LOG_GC