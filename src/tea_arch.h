/*
** tea_arch.h
** Target architecture selection
*/

#ifndef _TEA_ARCH_H
#define _TEA_ARCH_H

/* -- Target definitions -------------------------------------------------- */

/* Target endianess */
#define TEA_ENDIAN_LE 0
#define TEA_ENDIAN_BE 1

/* Target architectures */
#define TEA_ARCH_X86 1
#define TEA_ARCH_X64 2
#define TEA_ARCH_ARM 3
#define TEA_ARCH_ARM64 4
#define TEA_ARCH_WASM 5

/* Target OS */
#define TEA_OS_OTHER 0
#define TEA_OS_WINDOWS 1
#define TEA_OS_LINUX 2
#define TEA_OS_MACOSX 3
#define TEA_OS_BSD 4
#define TEA_OS_POSIX 5
#define TEA_OS_WASM 6

/* -- Target detection ---------------------------------------------------- */

/* Select native target if no target defined */
#ifndef TEA_TARGET

#if defined(__i386) || defined(__i386__) || defined(_M_IX86)
#define TEA_TARGET   TEA_ARCH_X86
#elif defined(__x86_64__) || defined(__x86_64) || defined(_M_X64) || defined(_M_AMD64)
#define TEA_TARGET   TEA_ARCH_X64
#elif defined(__arm__) || defined(__arm) || defined(__ARM__) || defined(__ARM)
#define TEA_TARGET   TEA_ARCH_ARM
#elif defined(__aarch64__)
#define TEA_TARGET   TEA_ARCH_ARM64
#elif defined(EMSCRIPTEN)
#define TEA_TARGET   TEA_ARCH_WASM
#else
#error "No support for this architecture (yet)"
#endif

#endif

/* Select target OS if no target OS defined */
#ifndef TEA_OS

#if defined(_WIN32) || defined(_WIN64)
#define TEA_OS       TEA_OS_WINDOWS
#elif defined(__linux__)
#define TEA_OS       TEA_OS_LINUX
#elif defined(__MACH__) && defined(__APPLE__)
#define TEA_OS       TEA_OS_MACOSX
#elif (defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || \
       defined(__NetBSD__) || defined(__OpenBSD__) || \
       defined(__DragonFly__)) && !defined(__ORBIS__)
#define TEA_OS       TEA_OS_BSD
#elif defined(EMSCRIPTEN)
#define TEA_OS       TEA_OS_WASM
#else
#define TEA_OS       TEA_OS_OTHER
#endif

#endif

/* Set target OS properties */
#if TEA_OS == TEA_OS_WINDOWS
#define TEA_OS_NAME  "windows"
#elif TEA_OS == TEA_OS_LINUX
#define TEA_OS_NAME  "linux"
#elif TEA_OS == TEA_OS_MACOSX
#define TEA_OS_NAME  "osx"
#elif TEA_OS == TEA_OS_BSD
#define TEA_OS_NAME  "bsd"
#elif TEA_OS == TEA_OS_POSIX
#define TEA_OS_NAME  "posix"
#else
#define TEA_OS_NAME  "other"
#endif

#define TEA_TARGET_WINDOWS  (TEA_OS == TEA_OS_WINDOWS)
#define TEA_TARGET_LINUX    (TEA_OS == TEA_OS_LINUX)
#define TEA_TARGET_MACOSX   (TEA_OS == TEA_OS_MACOSX)
#define TEA_TARGET_BSD      (TEA_OS == TEA_OS_BSD)
#define TEA_TARGET_POSIX    (TEA_OS > TEA_OS_WINDOWS)
#define TEA_TARGET_DLOPEN   TEA_TARGET_POSIX
#define TEA_TARGET_WASM     (TEA_OS == TEA_OS_WASM)

/* -- Arch-specific settings ---------------------------------------------- */

/* Set target architecture properties */
#if TEA_TARGET == TEA_ARCH_X86

#define TEA_ARCH_NAME "x86"
#define TEA_ARCH_BITS 32
#define TEA_ARCH_ENDIAN TEA_ENDIAN_LE

#elif TEA_TARGET == TEA_ARCH_X64

#define TEA_ARCH_NAME "x64"
#define TEA_ARCH_BITS 64
#define TEA_ARCH_ENDIAN TEA_ENDIAN_LE

#elif TEA_TARGET == TEA_ARCH_ARM

#define TEA_ARCH_NAME "arm"
#define TEA_ARCH_BITS 32
#define TEA_ARCH_ENDIAN TEA_ENDIAN_LE

#elif TEA_TARGET == TEA_ARCH_ARM64

#define TEA_ARCH_BITS 64
#if defined(__AARCH64EB__)
#define TEA_ARCH_NAME "arm64be"
#define TEA_ARCH_ENDIAN TEA_ENDIAN_BE
#else
#define TEA_ARCH_NAME "arm64"
#define TEA_ARCH_ENDIAN TEA_ENDIAN_LE
#endif

#elif defined(EMSCRIPTEN)
#define TEA_ARCH_NAME "wasm"
#define TEA_ARCH_ENDIAN TEA_LE
#else
#error "No target architecture defined"
#endif

#if TEA_ARCH_ENDIAN == TEA_ENDIAN_BE
#define TEA_ARCH_BYTEORDER "big"
#define TEA_LE 0
#define TEA_BE 1
#define TEA_ENDIAN_SELECT(le, be) be
#define TEA_ENDIAN_LOHI(lo, hi) hi lo
#else
#define TEA_ARCH_BYTEORDER "little"
#define TEA_LE 1
#define TEA_BE 0
#define TEA_ENDIAN_SELECT(le, be) le
#define TEA_ENDIAN_LOHI(lo, hi) lo hi
#endif

#if TEA_ARCH_BITS == 32
#define TEA_32       1
#define TEA_64       0
#else
#define TEA_32       0
#define TEA_64       1
#endif

#endif