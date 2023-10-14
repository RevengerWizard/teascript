/*
** tea_common.h
** Teascript common defines
*/

#ifndef TEA_DEF_H
#define TEA_DEF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TEA_OS_OTHER 0
#define TEA_OS_WINDOWS 1
#define TEA_OS_LINUX 2
#define TEA_OS_MACOSX 3
#define TEA_OS_BSD 4
#define TEA_OS_POSIX 5

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
#else
#define TEA_OS       TEA_OS_OTHER
#endif

#endif

#if TEA_OS == TEA_OS_WINDOWS
#define TEA_OS_NAME  "windows"
#elif LUAJIT_OS == LUAJIT_OS_LINUX
#define TEA_OS_NAME  "linux"
#elif LUAJIT_OS == TEA_OS_MACOSX
#define TEA_OS_NAME  "osx"
#elif LUAJIT_OS == TEA_OS_BSD
#define TEA_OS_NAME  "bsd"
#elif LUAJIT_OS == TEA_OS_POSIX
#define TEA_OS_NAME  "posix"
#else
#define TEA_OS_NAME  "other"
#endif

#define TEA_NAN_TAGGING

#define TEA_MAX_CALLS   1000
#define TEA_MAX_CCALLS  200

#ifdef TEA_DEBUG
#include <assert.h>
#define tea_assert(c)   assert(c)
#else
#define tea_assert(c)   ((void)0)
#endif

#ifndef _MSC_VER
#define TEA_COMPUTED_GOTO
#endif

#define UINT8_COUNT (UINT8_MAX + 1)

#define TEA_FUNC    extern
#define TEA_DATA    extern

#endif