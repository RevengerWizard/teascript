#ifndef TEA_CONFIG_H
#define TEA_CONFIG_H

#define TEA_REPOSITORY ""

#define TEA_VERSION_MAJOR "0"
#define TEA_VERSION_MINOR "0"
#define TEA_VERSION_PATCH "0"

#define TEA_VERSION "teascript " TEA_VERSION_MAJOR "." TEA_VERSION_MINOR
#define TEA_RELEASE TEA_VERSION "." TEA_VERSION_PATCH

#define TEA_BYTECODE_VERSION 0

#if defined(TEA_BUILD_DLL)
#if defined(TEA_CORE) || defined(TEA_LIB)
#define TEA_API __declspec(dllexport)
#else
#define TEA_API __declspec(dllimport)
#endif
#else
#define TEA_API		extern
#endif

#endif