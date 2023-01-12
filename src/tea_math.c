// tea_math.c
// Teascript math module

#include <string.h>
#include <math.h>

#include "tea.h"

#include "tea_module.h"
#include "tea_core.h"

#undef PI
#undef TAU
#undef E
#undef PHI
#define PI 3.14159265358979323846
#define TAU 6.28318530717958647692
#define E 2.71828182845904523536
#define PHI 1.61803398874989484820

static void math_min(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);

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

static void math_max(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);

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

static void math_mid(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 3);
    double x = tea_check_number(T, 0);
    double y = tea_check_number(T, 1);
    double z = tea_check_number(T, 2);
    tea_push_number(T, mid(x, y, z));
}

static void math_sum(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);
    double sum;
    for(int i = 0; i < count; i++) 
    {
        double n = tea_check_number(T, i);
        sum = sum + n;
    }
    tea_push_number(T, sum);
}

static void math_floor(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);
    tea_push_number(T, floor(tea_check_number(T, 0)));
}

static void math_ceil(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);
    tea_push_number(T, ceil(tea_check_number(T, 0)));
}

static void math_round(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);
    tea_push_number(T, round(tea_check_number(T, 0)));
}

static void math_acos(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);
    tea_push_number(T, acos(tea_check_number(T, 0)));
}

static void math_cos(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);
    tea_push_number(T, cos(tea_check_number(T, 0)));
}

static void math_asin(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);
    tea_push_number(T, asin(tea_check_number(T, 0)));
}

static void math_sin(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);
    tea_push_number(T, sin(tea_check_number(T, 0)));
}

static void math_atan(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);
    tea_push_number(T, atan(tea_check_number(T, 0)));
}

static void math_atan2(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 2);
    tea_push_number(T, atan2(tea_check_number(T, 0), tea_check_number(T, 1)));
}

static void math_tan(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);
    tea_push_number(T, tan(tea_check_number(T, 0)));
}

static void math_sign(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);
    double n = tea_check_number(T, 0);
    tea_push_number(T, (n > 0) ? 1 : ((n < 0) ? -1 : 0));
}

static void math_abs(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);
    tea_push_number(T, fabs(tea_check_number(T, 0)));
}

static void math_sqrt(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);
    tea_push_number(T, sqrt(tea_check_number(T, 0)));
}

static void math_deg(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);
    tea_push_number(T, tea_check_number(T, 0) * (180.0 / PI));
}

static void math_rad(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);
    tea_push_number(T, tea_check_number(T, 0) * (PI / 180.0));
}

static void math_exp(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);
    tea_push_number(T, exp(tea_check_number(T, 0)));
}

static const TeaModule math_module[] = 
{
    { "min", math_min },
    { "max", math_max },
    { "mid", math_mid },
    { "sum", math_sum },
    { "floor", math_floor },
    { "ceil", math_ceil },
    { "round", math_round },
    { "acos", math_acos },
    { "cos", math_cos },
    { "asin", math_asin },
    { "sin", math_sin },
    { "atan", math_atan },
    { "atan2", math_atan2 },
    { "tan", math_tan },
    { "sign", math_sign },
    { "abs", math_abs },
    { "sqrt", math_sqrt },
    { "deg", math_deg },
    { "rad", math_rad },
    { "exp", math_exp },
    { "pi", NULL },
    { "tau", NULL },
    { "e", NULL },
    { "phi", NULL },
    { NULL, NULL }
};

void tea_import_math(TeaState* T)
{
    tea_create_module(T, TEA_MATH_MODULE, math_module);
    tea_push_number(T, PI);
    tea_set_key(T, 0, "pi");
    tea_push_number(T, TAU);
    tea_set_key(T, 0, "tau");
    tea_push_number(T, E);
    tea_set_key(T, 0, "e");
    tea_push_number(T, PHI);
    tea_set_key(T, 0, "phi");
}