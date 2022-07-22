#ifndef TEA_CONFIG_H
#define TEA_CONFIG_H

#define STR(a) STR_(a)
#define STR_(a) #a

#define TEA_REPOSITORY ""

#define TEA_VERSION_MAJOR 0
#define TEA_VERSION_MINOR 0
#define TEA_VERSION_PATCH 0

#define TEA_VERSION STR(TEA_VERSION_MAJOR) "." STR(TEA_VERSION_MINOR) "." STR(TEA_VERSION_PATCH)

#define TEA_VERSION_NUMBER 0

#define TEA_BYTECODE_VERSION 0

#if defined(TEA_BUILD_DLL)
#define TEA_API __declspec(dllexport)
#else
#define TEA_API		extern
#endif

#endif