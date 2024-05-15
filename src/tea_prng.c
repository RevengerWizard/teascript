/*
** tea_prng.c
** Pseudo-random number generation
**
** Implementation taken verbatim from LuaJIT by Mike Pall
*/

#define tea_prng_c
#define TEA_CORE

#include "tea_def.h"
#include "tea_prng.h"

/* -- PRNG step function -------------------------------------------------- */

/*
** This implements a Tausworthe PRNG with period 2^223. Based on:
**   Tables of maximally-equidistributed combined LFSR generators,
**   Pierre L'Ecuyer, 1991, table 3, 1st entry.
** Full-period ME-CF generator with L=64, J=4, k=223, N1=49
**
** Important note: This PRNG is NOT suitable for cryptographic use!
**
** But it works fine for random module functions, which have an API that's not
** suitable for cryptography, anyway
*/

/* Update generator i and compute a running xor of all states */
#define TW223_GEN(rs, z, r, i, k, q, s) \
    z = rs->u[i]; \
    z = (((z<<q)^z) >> (k-s)) ^ ((z&((uint64_t)(int64_t)-1 << (64-k)))<<s); \
    r ^= z; rs->u[i] = z;

#define TW223_STEP(rs, z, r) \
    TW223_GEN(rs, z, r, 0, 63, 31, 18) \
    TW223_GEN(rs, z, r, 1, 58, 19, 28) \
    TW223_GEN(rs, z, r, 2, 55, 24,  7) \
    TW223_GEN(rs, z, r, 3, 47, 21,  8)

/* PRNG step function with uint64_t result */
TEA_NOINLINE uint64_t tea_prng_u64(PRNGState* rs)
{
    uint64_t z, r = 0;
    TW223_STEP(rs, z, r)
    return r;
}

/* PRNG step function with double in uint64_t result */
TEA_NOINLINE uint64_t tea_prng_u64d(PRNGState* rs)
{
    uint64_t z, r = 0;
    TW223_STEP(rs, z, r)
    /* Returns a double bit pattern in the range 1.0 <= d < 2.0 */
    return (r & U64x(000fffff,ffffffff)) | U64x(3ff00000,00000000);
}