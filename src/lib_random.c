/*
** lib_random.c
** Teascript random module
*/

#include <stdlib.h>
#include <time.h>
#include <math.h>

#define lib_random_c
#define TEA_LIB

#include "tea.h"
#include "tealib.h"

#include "tea_import.h"
#include "tea_lib.h"
#include "tea_prng.h"
#include "tea_list.h"

/* ------------------------------------------------------------------------ */

/* 
** This implements a Tausworthe PRNG with period 2^223. Based on:
**   Tables of maximally-equidistributed combined LFSR generators,
**   Pierre L'Ecuyer, 1991, table 3, 1st entry.
** Full-period ME-CF generator with L=64, J=4, k=223, N1=49
*/

/* Union needed for bit-pattern conversion between uint64_t and double */
typedef union { uint64_t u64; double d; } U64double;

/* PRNG seeding function */
static void prng_seed(PRNGState* rs, double d)
{
    uint32_t r = 0x11090601;  /* 64-k[i] as four 8 bit constants */
    int i;
    for(i = 0; i < 4; i++)
    {
        U64double u;
        uint32_t m = 1u << (r&255);
        r >>= 8;
        u.d = d = d * 3.14159265358979323846 + 2.7182818284590452354;
        if(u.u64 < m) u.u64 += m;  /* Ensure k[i] MSB of u[i] are non-zero */
        rs->u[i] = u.u64;
    }
    for(i = 0; i < 10; i++)
        (void)tea_prng_u64(rs);
}

static void random_seed(tea_State* T)
{
    PRNGState* rs = (PRNGState*)tea_check_userdata(T, tea_upvalue_index(0));
    prng_seed(rs, tea_lib_checknumber(T, 0));
    tea_push_nil(T);
}

static void random_random(tea_State* T)
{
    PRNGState* rs = (PRNGState*)tea_check_userdata(T, tea_upvalue_index(0));
    U64double u;
    double d;
    u.u64 = tea_prng_u64d(rs);
    d = u.d - 1.0;
    setnumV(T->top++, d);
}

static void random_range(tea_State* T)
{
    int count = tea_get_top(T);
    PRNGState* rs = (PRNGState*)tea_check_userdata(T, tea_upvalue_index(0));
    U64double u;
    double d;
    u.u64 = tea_prng_u64d(rs);
    d = u.d - 1.0;
    if(count == 2)
    {
        double r1 = tea_check_number(T, 0);
        double r2 = tea_check_number(T, 1);
        d = floor(d * (r2 - r1)) + r1;
        setnumV(T->top++, d);
        return;
    }
    else if(count == 1)
    {
        double r1, r2;
        tea_check_range(T, 0, &r1, &r2, NULL);
        d = floor(d * (r2 - r1)) + r1;
        setnumV(T->top++, d);
        return;
    }
    tea_error(T, "Expected two numbers or a range");
}

static void random_shuffle(tea_State* T)
{
    GClist* list = tea_lib_checklist(T, 0);
    if(list->len < 2)
        return;
    PRNGState* rs = (PRNGState*)tea_check_userdata(T, tea_upvalue_index(0));
    U64double u;
    double d;
    for(int i = 0; i < list->len; i++)
    {
        u.u64 = tea_prng_u64d(rs);
        d = u.d - 1.0;
        int j = floor(d * i);

        TValue tmp1, tmp2;
        copyTV(T, &tmp1, list_slot(list, i));
        copyTV(T, &tmp2, list_slot(list, j));

        copyTV(T, list_slot(list, i), &tmp2);
        copyTV(T, list_slot(list, j), &tmp1);
    }
}

static void random_choice(tea_State* T)
{
    GClist* list = tea_lib_checklist(T, 0);
    PRNGState* rs = (PRNGState*)tea_check_userdata(T, tea_upvalue_index(0));
    U64double u;
    double d;
    u.u64 = tea_prng_u64d(rs);
    d = u.d - 1.0;
    int32_t index = floor(d * list->len);
    copyTV(T, T->top++, list_slot(list, index));
}

/* ------------------------------------------------------------------------ */

static const tea_Reg random_module[] = {
    { "seed", random_seed, 1 },
    { "random", random_random, 0 },
    { "range", random_range, -2 },
    { "choice", random_choice, 1 },
    { "shuffle", random_shuffle, 1 },
    { NULL, NULL },
};

TEAMOD_API void tea_import_random(tea_State* T)
{
    tea_new_module(T, TEA_MODULE_RANDOM);
    PRNGState* rs = (PRNGState*)tea_new_userdata(T, sizeof(PRNGState));
    prng_seed(rs, (double)time(NULL));
    tea_set_funcs(T, random_module, 1);
}