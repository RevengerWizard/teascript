/*
** lib_math.c
** Teascript math module
*/

#define lib_math_c
#define TEA_LIB

#include <string.h>
#include <math.h>

#include "tea.h"
#include "tealib.h"

#include "tea_import.h"

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
    tea_check_args(T, count < 1, "Expected at least 1 argument, got %d", count);

    if(count == 1 && tea_is_list(T, 0))
    {
        int len = tea_len(T, 0);
        tea_get_item(T, 0, 0);
        double min = tea_check_number(T, 1);
        tea_pop(T, 1);
        for(int i = 1; i < len; i++)
        {
            tea_get_item(T, 0, i);
            double n = tea_check_number(T, 1);
            if(min > n)
            {
                min = n;
            }
            tea_pop(T, 1);
        }
        tea_push_number(T, min);
        return;
    }

    double min = tea_check_number(T, 0);
    for(int i = 1; i < count; i++)
    {
        double n = tea_check_number(T, i);
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
    tea_check_args(T, count < 1, "Expected at least 1 argument, got %d", count);

    if(count == 1 && tea_is_list(T, 0))
    {
        int len = tea_len(T, 0);
        tea_get_item(T, 0, 0);
        double max = tea_check_number(T, 1);
        tea_pop(T, 1);
        for(int i = 1; i < len; i++)
        {
            tea_get_item(T, 0, i);
            double n = tea_check_number(T, 1);
            if(n > max)
            {
                max = n;
            }
            tea_pop(T, 1);
        }
        tea_push_number(T, max);
        return;
    }

    double max = tea_check_number(T, 0);
    for(int i = 1; i < count; i++)
    {
        double n = tea_check_number(T, i);
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
    double x = tea_check_number(T, 0);
    double y = tea_check_number(T, 1);
    double z = tea_check_number(T, 2);
    tea_push_number(T, mid(x, y, z));
}

static void math_floor(tea_State* T)
{
    tea_push_number(T, floor(tea_check_number(T, 0)));
}

static void math_ceil(tea_State* T)
{
    tea_push_number(T, ceil(tea_check_number(T, 0)));
}

static void math_round(tea_State* T)
{
    tea_push_number(T, round(tea_check_number(T, 0)));
}

static void math_acos(tea_State* T)
{
    tea_push_number(T, acos(tea_check_number(T, 0)));
}

static void math_acosh(tea_State* T)
{
    tea_push_number(T, acosh(tea_check_number(T, 0)));
}

static void math_cos(tea_State* T)
{
    tea_push_number(T, cos(tea_check_number(T, 0)));
}

static void math_cosh(tea_State* T)
{
    tea_push_number(T, cosh(tea_check_number(T, 0)));
}

static void math_asin(tea_State* T)
{
    tea_push_number(T, asin(tea_check_number(T, 0)));
}

static void math_asinh(tea_State* T)
{
    tea_push_number(T, asinh(tea_check_number(T, 0)));
}

static void math_sin(tea_State* T)
{
    tea_push_number(T, sin(tea_check_number(T, 0)));
}

static void math_sinh(tea_State* T)
{
    tea_push_number(T, sinh(tea_check_number(T, 0)));
}

static void math_atan(tea_State* T)
{
    tea_push_number(T, atan(tea_check_number(T, 0)));
}

static void math_atanh(tea_State* T)
{
    tea_push_number(T, atanh(tea_check_number(T, 0)));
}

static void math_atan2(tea_State* T)
{
    tea_push_number(T, atan2(tea_check_number(T, 0), tea_check_number(T, 1)));
}

static void math_tan(tea_State* T)
{
    tea_push_number(T, tan(tea_check_number(T, 0)));
}

static void math_tanh(tea_State* T)
{
    tea_push_number(T, tanh(tea_check_number(T, 0)));
}

static void math_sign(tea_State* T)
{
    double n = tea_check_number(T, 0);
    tea_push_number(T, (n > 0) ? 1 : ((n < 0) ? -1 : 0));
}

static void math_abs(tea_State* T)
{
    tea_push_number(T, fabs(tea_check_number(T, 0)));
}

static void math_sqrt(tea_State* T)
{
    tea_push_number(T, sqrt(tea_check_number(T, 0)));
}

static void math_deg(tea_State* T)
{
    tea_push_number(T, tea_check_number(T, 0) * (180.0 / PI));
}

static void math_rad(tea_State* T)
{
    tea_push_number(T, tea_check_number(T, 0) * (PI / 180.0));
}

static void math_exp(tea_State* T)
{
    tea_push_number(T, exp(tea_check_number(T, 0)));
}

static void math_log(tea_State* T)
{
    tea_push_number(T, log(tea_check_number(T, 0)));
}

static void math_log2(tea_State* T)
{
    tea_push_number(T, log2(tea_check_number(T, 0)));
}

static void math_log10(tea_State* T)
{
    tea_push_number(T, log(tea_check_number(T, 0)));
}

static void math_isinfinity(tea_State* T)
{
    tea_push_bool(T, isinf(tea_check_number(T, 0)));
}

static void math_isnan(tea_State* T)
{
    tea_push_bool(T, isnan(tea_check_number(T, 0)));
}

static const tea_Module math_module[] =
{
    { "min", math_min, TEA_VARARGS },
    { "max", math_max, TEA_VARARGS },
    { "mid", math_mid, 3 },
    { "floor", math_floor, 1 },
    { "ceil", math_ceil, 1 },
    { "round", math_round, 1 },
    { "acos", math_acos, 1 },
    { "acosh", math_acosh, 1 },
    { "cos", math_cos, 1 },
    { "cosh", math_cosh, 1 },
    { "asin", math_asin, 1 },
    { "asinh", math_asinh, 1 },
    { "sin", math_sin, 1 },
    { "sinh", math_sinh, 1 },
    { "atan", math_atan, 1 },
    { "atanh", math_atanh, 1 },
    { "atan2", math_atan2, 2 },
    { "tan", math_tan, 1 },
    { "tanh", math_tanh, 1 },
    { "sign", math_sign, 1 },
    { "abs", math_abs, 1 },
    { "sqrt", math_sqrt, 1 },
    { "deg", math_deg, 1 },
    { "rad", math_rad, 1 },
    { "exp", math_exp, 1 },
    { "log", math_log, 1 },
    { "log2", math_log2, 1 },
    { "log10", math_log10, 1 },
    { "isinfinity", math_isinfinity, 1 },
    { "isnan", math_isnan, 1 },
    { "pi", NULL },
    { "tau", NULL },
    { "e", NULL },
    { "phi", NULL },
    { "infinity", NULL },
    { "nan", NULL },
    { NULL, NULL }
};

TEAMOD_API void tea_import_math(tea_State* T)
{
    tea_create_module(T, TEA_MODULE_MATH, math_module);
    tea_push_number(T, PI);
    tea_set_key(T, 0, "pi");
    tea_push_number(T, TAU);
    tea_set_key(T, 0, "tau");
    tea_push_number(T, E);
    tea_set_key(T, 0, "e");
    tea_push_number(T, PHI);
    tea_set_key(T, 0, "phi");
    tea_push_number(T, INFINITY);
    tea_set_key(T, 0, "infinity");
    tea_push_number(T, NAN);
    tea_set_key(T, 0, "nan");
}