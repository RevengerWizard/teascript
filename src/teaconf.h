/*
** teaconf.h
** Configuration file for Teascript
*/

#ifndef _TEACONF_H
#define _TEACONF_H

#if defined(TEA_BUILD_AS_DLL)

#if defined(TEA_CORE) || defined(TEA_LIB)
#define TEA_API __declspec(dllexport)
#else
#define TEA_API __declspec(dllimport)
#endif

#else

#define TEA_API		extern

#endif

#define TEAMOD_API  extern

#ifndef TEA_NUMBER_FMT
#define TEA_NUMBER_FMT		"%.16g"
#endif

#define TEA_MAX_CSTACK  8000

#endif