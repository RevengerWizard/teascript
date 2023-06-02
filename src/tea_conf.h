/*
** tea_conf.h
** Configuration file for Teascript
*/

#ifndef TEA_CONF_H
#define TEA_CONF_H

#define str(a) str_(a)
#define str_(a) #a

#if defined(TEA_BUILD_DLL)
#define TEA_API __declspec(dllexport)
#else
#define TEA_API		extern
#endif

#endif