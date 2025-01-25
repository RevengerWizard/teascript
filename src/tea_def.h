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

/* Various VM limits */
#define TEA_MAX_MEM32   0x7fffff00  /* Max. 32 bit memory allocation */
#define TEA_MAX_MEM64 ((uint64_t)1 << 47) /* Max. 64 bit memory allocation */

#define TEA_MAX_STR TEA_MAX_MEM32   /* Max string length */
#define TEA_MAX_BUF TEA_MAX_MEM32   /* Max. buffer length */
#define TEA_MAX_UDATA TEA_MAX_MEM32 /* Max. userdata length */

#define TEA_MAX_LINE TEA_MAX_MEM32

#define TEA_BUFFER_SIZE 512

#define TEA_MAX_INTEGER 9007199254740991
#define TEA_MIN_INTEGER -9007199254740991

/* Minimum buffer sizes */
#define TEA_MIN_STRTAB 256  /* Min. string table size (pow2) */
#define TEA_MIN_SBUF 32     /* Min. string buffer length */
#define TEA_MIN_VECSIZE 8   /* Min. size for growable vectors */

#define TEA_MAX_TOSTR 8    /* Max. string depth conversion */
#define TEA_MAX_SDEPTH (TEA_MAX_TOSTR * 2)  /* Max. string stack conversion */

#define TEA_MAX_BCINS (1 << 26) /* Max. # of bytecode instructions */
#define TEA_MAX_UPVAL 256  /* Max. # of upvalues */
#define TEA_MAX_LOCAL 256  /* Max. # of local variables */

/* Various macros */
#ifndef UNUSED
#define UNUSED(x) ((void)(x))   /* Avoid warnings */
#endif

#define U64x(hi, lo)    (((uint64_t)0x##hi << 32) + (uint64_t)0x##lo)

#define checki32(x) ((x) == (int32_t)(x))

#if defined(__GNUC__) || defined(__clang)

#define TEA_NORET __attribute__((noreturn))
#define TEA_INLINE inline
#define TEA_AINLINE inline __attribute__((always_inline))
#define TEA_NOINLINE __attribute__((noinline))

#if defined(__ELF__) || defined(__MACH__) || defined(__psp2__)
#if !((defined(__sun__) && defined(__svr4__)) || defined(__CELLOS_LV2__))
#define TEA_NOAPI    extern __attribute__((visibility("hidden")))
#endif
#endif

/* 
** Note: it's only beneficial to use fastcall on x86 and then only for up to
** two non-FP args. The amalgamated compile covers all TEA_FUNC cases. Only
** indirect calls and related tail-called C functions are marked as fastcall.
*/
#if defined(__i386__)
#define TEA_FASTCALL __attribute__((fastcall))
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

#if defined(__arm__)
static TEA_AINLINE uint32_t tea_bswap(uint32_t x)
{
#if defined(__psp2__)
    return __builtin_rev(x);
#else
  uint32_t r;
#if __ARM_ARCH_6__ || __ARM_ARCH_6J__ || __ARM_ARCH_6T2__ || __ARM_ARCH_6Z__ ||\
    __ARM_ARCH_6ZK__ || __ARM_ARCH_7__ || __ARM_ARCH_7A__ || __ARM_ARCH_7R__
    __asm__("rev %0, %1" : "=r" (r) : "r" (x));
    return r;
#else
#ifdef __thumb__
    r = x ^ lj_ror(x, 16);
#else
    __asm__("eor %0, %1, %1, ror #16" : "=r" (r) : "r" (x));
#endif
    return ((r & 0xff00ffffu) >> 8) ^ lj_ror(x, 8);
#endif
#endif
}

#elif (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3) || __clang__
static TEA_AINLINE uint32_t tea_bswap(uint32_t x)
{
    return (uint32_t)__builtin_bswap32((int32_t)x);
}

#elif defined(__i386__) || defined(__x86_64__)
static TEA_AINLINE uint32_t tea_bswap(uint32_t x)
{
    uint32_t r; __asm__("bswap %0" : "=r" (r) : "0" (x)); return r;
}

#else
static TEA_AINLINE uint32_t tea_bswap(uint32_t x)
{
    return (x << 24) | ((x & 0xff00) << 8) | ((x >> 8) & 0xff00) | (x >> 24);
}
#endif

#elif defined(_MSC_VER)

#define TEA_NORET __declspec(noreturn)
#define TEA_INLINE __inline
#define TEA_AINLINE __forceinline
#define TEA_NOINLINE __declspec(noinline)
#if defined(_M_IX86)
#define TEA_FASTCALL __fastcall
#endif

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

unsigned long _byteswap_ulong(unsigned long);
#define tea_bswap(x) (_byteswap_ulong((x)))

#else
#error "missing defines for your compiler"
#endif

/* Optional defines */
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
#if defined(TEA_USE_ASSERT) || defined(TEA_USE_APICHECK)
#define tea_assert_check(T, c, ...) \
    ((c) ? (void)0 : \
    (tea_assert_fail((T), __FILE__, __LINE__, __func__, __VA_ARGS__), 0))
#define tea_checkapi(c, ...) tea_assert_check((T), (c), __VA_ARGS__)
#else
#define tea_checkapi(c, ...) ((void)0)
#endif

#ifdef TEA_USE_ASSERT
#define tea_assertT_(T, c, ...) tea_assert_check((T), (c), __VA_ARGS__)
#define tea_assertT(c, ...) tea_assert_check((T), (c), __VA_ARGS__)
#define tea_assertX(c, ...) tea_assert_check(NULL, (c), __VA_ARGS__)
#else
#define tea_assertT(c, ...) ((void)0)
#define tea_assertX(c, ...) ((void)0)
#endif

/* Static assertions */
#define TEA_ASSERT_NAME2(name, line) name ## line
#define TEA_ASSERT_NAME(line) TEA_ASSERT_NAME2(tea_assert_, line)
#ifdef __COUNTER__
#define TEA_STATIC_ASSERT(cond) \
    extern void TEA_ASSERT_NAME(__COUNTER__)(int STATIC_ASSERTION_FAILED[(cond) ? 1 : -1])
#else
#define TEA_STATIC_ASSERT(cond) \
    extern void TEA_ASSERT_NAME(__LINE__)(int STATIC_ASSERTION_FAILED[(cond) ? 1 : -1])
#endif

/* PRNG state. Need this here, details in tea_prng.h */
typedef struct PRNGState
{
    uint64_t u[4];
} PRNGState;

#endif