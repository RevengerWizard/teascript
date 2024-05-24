/*
** lib_time.c
** Teascript time module
*/

#include <stdlib.h>
#include <time.h>

#define lib_time_c
#define TEA_LIB

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
#include "tea_buf.h"

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

    tea_push_nil(T);
}

static void time_clock(tea_State* T)
{
    tea_push_number(T, ((double)clock()) * (1.0 / ((double)CLOCKS_PER_SEC)));
}

/* ------------------------------------------------------------------------ */

static void setfield(tea_State* T, const char* key, int value)
{
    tea_push_number(T, value);
    tea_set_key(T, -2, key);
}

static void setboolfield(tea_State* T, const char* key, int value)
{
    /* Undefined? */
    if(value < 0)
        return;    /* Does not set field */
    tea_push_bool(T, value);
    tea_set_key(T, -2, key);
}

static int getboolfield(tea_State* T, const char* key)
{
    int res;
    bool found = tea_get_key(T, -1, key);
    if(found)
    {
        res = tea_to_bool(T, -1);
        tea_pop(T, 1);
    }
    else
    {
        res = -1;
    }
    return res;
}

static int getfield(tea_State* T, const char* key, int d)
{
    int res;
    bool found = tea_get_key(T, -1, key);
    if(found && tea_is_number(T, -1))
    {
        res = (int)tea_to_number(T, -1);
        tea_pop(T, 1);
    }
    else
    {
        if(d < 0)
            tea_error(T, "Field " TEA_QS " missing in time map", key);
        res = d;
    }
    return res;
}

static void time_format(tea_State* T)
{
    const char* s = tea_opt_string(T, 0, "%c");
    time_t t = tea_is_none(T, 1) ? time(NULL) : (time_t)tea_check_number(T, 1);
    struct tm* stm;

#if TEA_TARGET_POSIX
    struct tm rtm;
#endif
    if(*s == '!')
    {  
        /* UTC? */
        s++;  /* Skip '!' */
#if tea_TARGET_POSIX
    stm = gmtime_r(&t, &rtm);
#else
    stm = gmtime(&t);
#endif
    }
    else
    {
#if TEA_TARGET_POSIX
        stm = localtime_r(&t, &rtm);
#else
        stm = localtime(&t);
#endif
    }

    if(stm == NULL)
    {
        /* Invalid date? */
        setnilV(T->top++);
    }
    else if(strcmp(s, "*t") == 0)
    {
        tea_new_map(T);   /* 9 = number of fields */
        setfield(T, "sec", stm->tm_sec);
        setfield(T, "min", stm->tm_min);
        setfield(T, "hour", stm->tm_hour);
        setfield(T, "day", stm->tm_mday);
        setfield(T, "month", stm->tm_mon + 1);
        setfield(T, "year", stm->tm_year + 1900);
        setfield(T, "wday", stm->tm_wday + 1);
        setfield(T, "yday", stm->tm_yday + 1);
        setboolfield(T, "isdst", stm->tm_isdst);
    }
    else if(*s)
    {
        SBuf* sb = &T->tmpbuf;
        size_t size = 0, retry = 4;
        const char* q;
        for(q = s; *q; q++)
            size += (*q == '%') ? 30 : 1;   /* Overflow doesn't matter */
        while(retry--)
        {
            /* Limit growth for invalid format or empty result */
            char* buf = tea_buf_need(T, sb, size);
            size_t len = strftime(buf, sbuf_size(sb), s, stm);
            if(len)
            {
                setstrV(T, T->top++, tea_str_new(T, buf, len));
                break;
            }
            size += (size | 1);
        }
    }
    else
    {
        setstrV(T, T->top++, &T->strempty);
    }
}

static void time_time(tea_State* T)
{
    int count = tea_get_top(T);
    time_t t;
    if(count == 0)
    {
        /* Called without arguments? */
        t = time(NULL); /* Get current time */
    }
    else
    {
        struct tm ts;
        tea_check_map(T, 0);
        ts.tm_sec = getfield(T, "sec", 0);
        ts.tm_min = getfield(T, "min", 0);
        ts.tm_hour = getfield(T, "hour", 12);
        ts.tm_mday = getfield(T, "day", -1);
        ts.tm_mon = getfield(T, "month", -1) - 1;
        ts.tm_year = getfield(T, "year", -1) - 1900;
        ts.tm_isdst = getboolfield(T, "isdst");
        t = mktime(&ts);
    }
    tea_push_number(T, (double)t);
}

static void time_diff(tea_State* T)
{
    tea_push_number(T, difftime((time_t)(tea_check_number(T, 0)),
                                (time_t)(tea_opt_number(T, 1, 0))));
}

/* ------------------------------------------------------------------------ */

static const tea_Reg time_module[] = {
    { "sleep", time_sleep, 1 },
    { "clock", time_clock, 0 },
    { "format", time_format, -2 },
    { "time", time_time, -1 },
    { "diff", time_diff, -2 },
    { NULL, NULL }
};

TEAMOD_API void tea_import_time(tea_State* T)
{
    tea_create_module(T, TEA_MODULE_TIME, time_module);
}