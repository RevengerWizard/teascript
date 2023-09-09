/*
** tea_timelib.c
** Teascript time module
*/

#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <math.h>
#include <unistd.h>
#include <sys/utsname.h>
#endif

#define tea_timelib_c
#define TEA_LIB

#include "tea.h"
#include "tealib.h"

#include "tea_import.h"
#include "tea_core.h"

static void time_sleep(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);

    double stop = tea_check_number(T, 0);
#ifdef _WIN32
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

static void time_clock(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 0);
    tea_push_number(T, ((double)clock()) / ((double)CLOCKS_PER_SEC));
}

static void time_time(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 0);
    tea_push_number(T, (double)time(NULL));
}

static const TeaModule time_module[] = {
    { "sleep", time_sleep },
    { "clock", time_clock },
    { "time", time_time },
    { NULL, NULL }
};

TEAMOD_API void tea_import_time(TeaState* T)
{
    tea_create_module(T, TEA_TIME_MODULE, time_module);
}