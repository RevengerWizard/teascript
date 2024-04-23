/*
** teaconf.h
** Configuration header
*/

#ifndef _TEACONF_H
#define _TEACONF_H

/* Linkage of public API functions */
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

#define TEA_NUMBER_FMT		"%.14g"

#define TEA_MAX_CSTACK  8000

/* Quoting in error messages */
#define TEA_QL(x)   "'" x "'"
#define TEA_QS      TEA_QL("%s")

#endif