#ifndef TEA_COMMON_H
#define TEA_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NAN_TAGGING
#define DEBUG_PRINT_CODE
#define DEBUG_TRACE_EXECUTION

#define DEBUG_STRESS_GC
#define DEBUG_LOG_GC

#ifndef _MSC_VER
#define COMPUTED_GOTO
#endif

#define UINT8_COUNT (UINT8_MAX + 1)

#endif

#undef DEBUG_PRINT_CODE
#undef DEBUG_TRACE_EXECUTION
#undef DEBUG_STRESS_GC
#undef DEBUG_LOG_GC