/*
** teaconf.h
** Configuration file for Teascript
*/

#ifndef TEACONF_H
#define TEACONF_H

#define str(a) str_(a)
#define str_(a) #a

#if defined(TEA_BUILD_DLL)

#if defined(TEA_CORE) || defined(TEA_LIB)
#define TEA_API __declspec(dllexport)
#else
#define TEA_API __declspec(dllimport)
#endif

#else

#define TEA_API		extern

#endif

#define TEAMOD_API  TEA_API

#ifndef TEA_NUMBER_FMT
#define TEA_NUMBER_FMT		"%.16g"
#endif

#endif