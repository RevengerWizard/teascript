/*
** tea_bc.c
** Bytecode instructions
*/

#define tea_bc_c
#define TEA_CORE

#include "tea_bc.h"

TEA_DATADEF const char* const tea_bcnames[] = {
#define BCNAME(name, _) #name,
    BCDEF(BCNAME)
#undef BCNAME
};