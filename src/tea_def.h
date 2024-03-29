/*
** tea_common.h
** Teascript common internal definitions
*/

#ifndef _TEA_DEF_H
#define _TEA_DEF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#define TEA_MAX_CALLS   1000
#define TEA_MAX_CCALLS  200

#ifndef _MSC_VER
#define TEA_COMPUTED_GOTO
#endif

#define TEA_MAX_MEM32   0x7fffff00
#define TEA_MAX_MEM64 ((uint64_t)1<<47)

#define TEA_BUFFER_SIZE 512

#define TEA_MIN_BUF 32

#define TEA_MAX_BUF TEA_MAX_MEM32

#define UINT8_COUNT (UINT8_MAX + 1)

/* Various macros */
#ifndef UNUSED
#define UNUSED(x) ((void)(x))   /* Avoid warnings */
#endif

#define U64x(hi, lo)    (((uint64_t)0x##hi << 32) + (uint64_t)0x##lo)

#define checki32(x) ((x) == (int32_t)(x))

#if defined(__GNUC__) || defined(__clang)

#define TEA_INLINE  inline
#define TEA_AINLINE inline __attribute__((always_inline))
#define TEA_NOINLINE __attribute__((noinline))

#if defined(__ELF__) || defined(__MACH__) || defined(__psp2__)
#if !((defined(__sun__) && defined(__svr4__)) || defined(__CELLOS_LV2__))
#define LJ_NOAPI    extern __attribute__((visibility("hidden")))
#endif
#endif

#define TEA_LIKELY(x)   __builtin_expect(!!(x), 1)
#define TEA_UNLIKELY(x) __builtin_expect(!!(x), 0)

#define tea_ffs(x)  ((uint32_t)__builtin_ctz(x))

#if defined(__INTEL_COMPILER) && (defined(__i386__) || defined(__x86_64__))
static TEA_AINLINE uint32_t tea_fls(uint32_t x)
{
    uint32_t r; __asm__("bsrl %1, %0" : "=r" (r) : "rm" (x) : "cc"); return r;
}
#else
#define tea_fls(x)	((uint32_t)(__builtin_clz(x)^31))
#endif

#elif defined(_MSC_VER)

#define TEA_INLINE __inline
#define TEA_AINLINE __forceinline
#define TEA_NOINLINE __declspec(noinline)

unsigned char _BitScanForward(unsigned long*, unsigned long);
unsigned char _BitScanReverse(unsigned long*, unsigned long);
#pragma intrinsic(_BitScanForward)
#pragma intrinsic(_BitScanReverse)

static TEA_AINLINE uint32_t tea_ffs(uint32_t x)
{
    unsigned long r; _BitScanForward(&r, x); return (uint32_t)r;
}

static TEA_AINLINE uint32_t tea_fls(uint32_t x)
{
    unsigned long r; _BitScanReverse(&r, x); return (uint32_t)r;
}

#else
#error "missing defines for your compiler"
#endif

#ifndef TEA_FASTCALL
#define TEA_FASTCALL
#endif

#ifndef TEA_NORET
#define TEA_NORET
#endif

#ifndef TEA_NOAPI
#define TEA_NOAPI extern
#endif

#ifndef TEA_LIKELY
#define TEA_LIKELY(x)   (x)
#define TEA_UNLIKELY(x)   (x)
#endif

/* Attributes for internal functions */
#define TEA_DATA    TEA_NOAPI
#define TEA_DATADEF
#define TEA_FUNC    TEA_NOAPI
#define TEA_FUNC_NORET  TEA_FUNC TEA_NORET

/* Internal assertions */
#if defined(TEA_USE_ASSERT)
#define tea_assert_check(T, c, ...) \
    ((c) ? (void)0 : \
    (tea_assert_fail((T), __FILE__, __LINE__, __func__, __VA_ARGS__), 0))
#endif

#ifdef TEA_USE_ASSERT
#define tea_assertT(T, c, ...) tea_assert_check((T), (c), __VA_ARGS__)
#else
#define tea_assertT(T, c, ...) ((void)0)
#endif

#endif