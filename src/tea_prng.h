/*
** tea_prng.h
** Pseudo-random number generation
*/

#ifndef _TEA_PRNG_H
#define _TEA_PRNG_H

#include "tea_def.h"

TEA_FUNC uint64_t tea_prng_u64(PRNGState* rs);
TEA_FUNC uint64_t tea_prng_u64d(PRNGState* rs);

#endif