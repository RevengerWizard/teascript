/*
** tea_arch.h
** Teascript os defines
*/

#ifndef TEA_ARCH_H
#define TEA_ARCH_H

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

#endif