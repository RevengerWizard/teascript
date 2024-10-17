/*
** lib_math.c
** Teascript math module
*/

#include <string.h>
#include <math.h>

#define lib_math_c
#define TEA_LIB

#include "tea.h"
#include "tealib.h"

#include "tea_import.h"
#include "tea_lib.h"

#undef PI
#undef TAU
#undef E
#undef PHI

#define PI 3.14159265358979323846
#define TAU 6.28318530717958647692
#define E 2.71828182845904523536
#define PHI 1.61803398874989484820

static void math_min(tea_State* T)
{
    int count = tea_get_top(T);
    if(count == 1 && tea_is_list(T, 0))
    {
        int len = tea_len(T, 0);
        tea_get_item(T, 0, 0);
        double min = tea_lib_checknumber(T, 1);
        tea_pop(T, 1);
        for(int i = 1; i < len; i++)
        {
            tea_get_item(T, 0, i);
            double n = tea_lib_checknumber(T, 1);
            if(min > n)
            {
                min = n;
            }
            tea_pop(T, 1);
        }
        tea_push_number(T, min);
        return;
    }

    double min = tea_lib_checknumber(T, 0);
    for(int i = 1; i < count; i++)
    {
        double n = tea_lib_checknumber(T, i);
        if(min > n)
        {
            min = n;
        }
    }
    tea_push_number(T, min);
}

static void math_max(tea_State* T)
{
    int count = tea_get_top(T);
    if(count == 1 && tea_is_list(T, 0))
    {
        int len = tea_len(T, 0);
        tea_get_item(T, 0, 0);
        double max = tea_lib_checknumber(T, 1);
        tea_pop(T, 1);
        for(int i = 1; i < len; i++)
        {
            tea_get_item(T, 0, i);
            double n = tea_lib_checknumber(T, 1);
            if(n > max)
            {
                max = n;
            }
            tea_pop(T, 1);
        }
        tea_push_number(T, max);
        return;
    }

    double max = tea_lib_checknumber(T, 0);
    for(int i = 1; i < count; i++)
    {
        double n = tea_lib_checknumber(T, i);
        if(n > max)
        {
            max = n;
        }
    }
    tea_push_number(T, max);
}

static double mid(double x, double y, double z)
{
    double mid;
    if(x > y)
    {
        if(y > z)
            mid = y;
        else if(x > z)
            mid = z;
        else
            mid = x;
    }
    else
    {
        if(x > z)
            mid = x;
        else if(y > z)
            mid = z;
        else
            mid = y;
    }
    return mid;
}

static void math_mid(tea_State* T)
{
    double x = tea_lib_checknumber(T, 0);
    double y = tea_lib_checknumber(T, 1);
    double z = tea_lib_checknumber(T, 2);
    tea_push_number(T, mid(x, y, z));
}

static void math_clamp(tea_State* T)
{
    double d = tea_lib_checknumber(T, 0);
    double min = tea_lib_checknumber(T, 1);
    double max = tea_lib_checknumber(T, 2);
    const double t = d < min ? min : d;
    tea_push_number(T, t > max ? max : t);
}

static void math_floor(tea_State* T)
{
    tea_push_number(T, floor(tea_lib_checknumber(T, 0)));
}

static void math_ceil(tea_State* T)
{
    tea_push_number(T, ceil(tea_lib_checknumber(T, 0)));
}

static void math_round(tea_State* T)
{
    tea_push_number(T, round(tea_lib_checknumber(T, 0)));
}

static void math_acos(tea_State* T)
{
    tea_push_number(T, acos(tea_lib_checknumber(T, 0)));
}

static void math_acosh(tea_State* T)
{
    tea_push_number(T, acosh(tea_lib_checknumber(T, 0)));
}

static void math_cos(tea_State* T)
{
    tea_push_number(T, cos(tea_lib_checknumber(T, 0)));
}

static void math_cosh(tea_State* T)
{
    tea_push_number(T, cosh(tea_lib_checknumber(T, 0)));
}

static void math_asin(tea_State* T)
{
    tea_push_number(T, asin(tea_lib_checknumber(T, 0)));
}

static void math_asinh(tea_State* T)
{
    tea_push_number(T, asinh(tea_lib_checknumber(T, 0)));
}

static void math_sin(tea_State* T)
{
    tea_push_number(T, sin(tea_lib_checknumber(T, 0)));
}

static void math_sinh(tea_State* T)
{
    tea_push_number(T, sinh(tea_lib_checknumber(T, 0)));
}

static void math_atan(tea_State* T)
{
    tea_push_number(T, atan(tea_lib_checknumber(T, 0)));
}

static void math_atanh(tea_State* T)
{
    tea_push_number(T, atanh(tea_lib_checknumber(T, 0)));
}

static void math_atan2(tea_State* T)
{
    tea_push_number(T, atan2(tea_lib_checknumber(T, 0), tea_lib_checknumber(T, 1)));
}

static void math_tan(tea_State* T)
{
    tea_push_number(T, tan(tea_lib_checknumber(T, 0)));
}

static void math_tanh(tea_State* T)
{
    tea_push_number(T, tanh(tea_lib_checknumber(T, 0)));
}

static void math_sign(tea_State* T)
{
    double n = tea_lib_checknumber(T, 0);
    tea_push_number(T, (n > 0) ? 1 : ((n < 0) ? -1 : 0));
}

static void math_abs(tea_State* T)
{
    tea_push_number(T, fabs(tea_lib_checknumber(T, 0)));
}

static void math_sqrt(tea_State* T)
{
    tea_push_number(T, sqrt(tea_lib_checknumber(T, 0)));
}

static void math_deg(tea_State* T)
{
    tea_push_number(T, tea_lib_checknumber(T, 0) * (180.0 / PI));
}

static void math_rad(tea_State* T)
{
    tea_push_number(T, tea_lib_checknumber(T, 0) * (PI / 180.0));
}

static void math_exp(tea_State* T)
{
    tea_push_number(T, exp(tea_lib_checknumber(T, 0)));
}

static void math_trunc(tea_State* T)
{
    tea_push_number(T, trunc(tea_lib_checknumber(T, 0)));
}

static void math_frexp(tea_State* T)
{
    int e;
    double n = frexp(tea_lib_checknumber(T, 0), &e);
    tea_new_list(T, 2);
    tea_push_number(T, e);
    tea_push_number(T, n);
    tea_add_item(T, 1);
    tea_add_item(T, 1);
}

static void math_ldexp(tea_State* T)
{
    tea_push_number(T, ldexp(tea_lib_checknumber(T, 0), (int)tea_lib_checknumber(T, 1)));
}

static void math_log(tea_State* T)
{
    tea_push_number(T, log(tea_lib_checknumber(T, 0)));
}

static void math_log1p(tea_State* T)
{
    tea_push_number(T, log1p(tea_lib_checknumber(T, 0)));
}

static void math_log2(tea_State* T)
{
    tea_push_number(T, log2(tea_lib_checknumber(T, 0)));
}

static void math_log10(tea_State* T)
{
    tea_push_number(T, log10(tea_lib_checknumber(T, 0)));
}

static void math_classify(tea_State* T)
{
    switch(fpclassify(tea_lib_checknumber(T, 0)))
    {
        case FP_INFINITE: tea_push_literal(T, "infinity"); break;
        case FP_NAN: tea_push_literal(T, "nan"); break;
        case FP_NORMAL: tea_push_literal(T, "normal"); break;
        case FP_SUBNORMAL: tea_push_literal(T, "subnormal"); break;
        case FP_ZERO: tea_push_literal(T, "zero"); break;
    }
}

static void math_isinfinity(tea_State* T)
{
    tea_push_bool(T, isinf(tea_lib_checknumber(T, 0)));
}

static void math_isnan(tea_State* T)
{
    tea_push_bool(T, isnan(tea_lib_checknumber(T, 0)));
}

/* ------------------------------------------------------------------------ */

static const tea_Reg math_module[] =
{
    { "min", math_min, TEA_VARG, 0 },
    { "max", math_max, TEA_VARG, 0 },
    { "mid", math_mid, 3, 0 },
    { "clamp", math_clamp, 3, 0 },
    { "floor", math_floor, 1, 0 },
    { "ceil", math_ceil, 1, 0 },
    { "round", math_round, 1, 0 },
    { "acos", math_acos, 1, 0 },
    { "acosh", math_acosh, 1, 0 },
    { "cos", math_cos, 1, 0 },
    { "cosh", math_cosh, 1, 0 },
    { "asin", math_asin, 1, 0 },
    { "asinh", math_asinh, 1, 0 },
    { "sin", math_sin, 1, 0 },
    { "sinh", math_sinh, 1, 0 },
    { "atan", math_atan, 1, 0 },
    { "atanh", math_atanh, 1, 0 },
    { "atan2", math_atan2, 2, 0 },
    { "tan", math_tan, 1, 0 },
    { "tanh", math_tanh, 1, 0 },
    { "sign", math_sign, 1, 0 },
    { "abs", math_abs, 1, 0 },
    { "sqrt", math_sqrt, 1, 0 },
    { "deg", math_deg, 1, 0 },
    { "rad", math_rad, 1, 0 },
    { "exp", math_exp, 1, 0 },
    { "trunc", math_trunc, 1, 0 },
    { "frexp", math_frexp, 1, 0 },
    { "ldexp", math_ldexp, 2, 0 },
    { "log", math_log, 1, 0 },
    { "log1p", math_log1p, 1, 0 },
    { "log2", math_log2, 1, 0 },
    { "log10", math_log10, 1, 0 },
    { "classify", math_classify, 1, 0 },
    { "isinfinity", math_isinfinity, 1, 0 },
    { "isnan", math_isnan, 1, 0 },
    { "pi", NULL },
    { "tau", NULL },
    { "e", NULL },
    { "phi", NULL },
    { "infinity", NULL },
    { "nan", NULL },
    { "maxinteger", NULL },
    { "mininteger", NULL },
    { NULL, NULL }
};

TEAMOD_API void tea_import_math(tea_State* T)
{
    tea_create_module(T, TEA_MODULE_MATH, math_module);
    tea_push_number(T, PI);
    tea_set_attr(T, 0, "pi");
    tea_push_number(T, TAU);
    tea_set_attr(T, 0, "tau");
    tea_push_number(T, E);
    tea_set_attr(T, 0, "e");
    tea_push_number(T, PHI);
    tea_set_attr(T, 0, "phi");
    tea_push_number(T, INFINITY);
    tea_set_attr(T, 0, "infinity");
    tea_push_number(T, NAN);
    tea_set_attr(T, 0, "nan");
    tea_push_number(T, TEA_MAX_INTEGER);
    tea_set_attr(T, 0, "maxinteger");
    tea_push_number(T, TEA_MIN_INTEGER);
    tea_set_attr(T, 0, "mininteger");
}