/*
** lib_time.c
** Teascript time module
*/

#define lib_time_c
#define TEA_LIB

#include <stdlib.h>
#include <time.h>

#include "tea_arch.h"

#if TEA_TARGET_WINDOWS
#include <windows.h>
#else
#include <math.h>
#include <unistd.h>
#include <sys/utsname.h>
#endif

#include "tea.h"
#include "tealib.h"

#include "tea_import.h"

static void time_sleep(tea_State* T)
{
    double stop = tea_check_number(T, 0);

#if TEA_TARGET_WINDOWS
    Sleep(stop * 1000);
#elif _POSIX_C_SOURCE >= 199309L
    struct timespec ts;
    ts.tv_sec = stop;
    ts.tv_nsec = fmod(stop, 1) * 1000000000;
    nanosleep(&ts, NULL);
#else
    if(stop >= 1)
        sleep(stop);

    /* 1000000 = 1 second */
    usleep(fmod(stop, 1) * 1000000);
#endif

    tea_push_null(T);
}

static void time_clock(tea_State* T)
{
    tea_push_number(T, ((double)clock()) / ((double)CLOCKS_PER_SEC));
}

static void time_time(tea_State* T)
{
    tea_push_number(T, (double)time(NULL));
}

static const tea_Module time_module[] = {
    { "sleep", time_sleep, 1 },
    { "clock", time_clock, 0 },
    { "time", time_time, 0 },
    { NULL, NULL }
};

TEAMOD_API void tea_import_time(tea_State* T)
{
    tea_create_module(T, TEA_MODULE_TIME, time_module);
}