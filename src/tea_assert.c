/*
** tea_assert.c
** Internal assertions
*/

#define tea_assert_c
#define TEA_CORE

#if defined(TEA_USE_ASSERT)

#include <stdio.h>

void tea_assert_fail(tea_State* T, const char* file, int line, const char* func, const char* fmt, ...)
{
    va_list argp;
    va_start(argp, fmt);
    fprintf(stderr, "Teascript ASSERT %s:%d: %s: ", file, line, func);
    vfprintf(stderr, fmt, argp);
    fputc('\n', stderr);
    va_end(argp);
    UNUSED(T);  /* Close state? */
    abort();
}

#endif