#ifndef TEA_CONFIG_H
#define TEA_CONFIG_H

#define str(a) str_(a)
#define str_(a) #a

#define TEA_REPOSITORY "https://github.com/RevengerWizard/teascript"

#define TEA_VERSION_MAJOR 0
#define TEA_VERSION_MINOR 0
#define TEA_VERSION_PATCH 0

#define TEA_VERSION str(TEA_VERSION_MAJOR) "." str(TEA_VERSION_MINOR) "." str(TEA_VERSION_PATCH)

#define TEA_VERSION_NUMBER 0

#define TEA_BYTECODE_VERSION 0

#if defined(TEA_BUILD_DLL)
#define TEA_API __declspec(dllexport)
#else
#define TEA_API		extern
#endif

#endif