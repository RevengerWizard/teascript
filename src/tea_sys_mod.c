// tea_sys.c
// Teascript sys module

#include <stdlib.h>
#include <math.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <unistd.h>
#include <sys/utsname.h>
#endif

#include "tea.h"

#include "tea_module.h"
#include "tea_core.h"

static void sys_exit(TeaState* T)
{
    int count = tea_get_top(T);
    count == 0 ? exit(1) : exit(tea_check_number(T, 0));
    tea_push_null(T);
}

static void sys_sleep(TeaState* T)
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

    // 1000000 = 1 second
    usleep(fmod(stop, 1) * 1000000);
#endif

    tea_push_null(T);
}

static void init_argv(TeaState* T)
{
    int argc;
    const char** argv = tea_get_argv(T, &argc);

    tea_push_list(T);

    for(int i = 1; i < argc; i++)
    {
        tea_push_string(T, argv[i]);
        tea_add_item(T, 1);
    }
    tea_set_key(T, 0, "argv");
}

static void set_version(TeaState* T)
{
    tea_push_map(T);

    tea_push_number(T, TEA_VERSION_MAJOR);
    tea_set_key(T, 1, "major");
    tea_push_number(T, TEA_VERSION_MINOR);
    tea_set_key(T, 1, "minor");
    tea_push_number(T, TEA_VERSION_PATCH);
    tea_set_key(T, 1, "patch");

    tea_set_key(T, 0, "version");
}

static const char* byteorder()
{
    int x = 1;
    bool order = *(char*)&x;
    if(order)
    {
        return "little";
    }
    return "big";
}

static const TeaModule sys_module[] = {
    { "sleep", sys_sleep },
    { "exit", sys_exit },
    { "argv", NULL },
    { "version", NULL },
    { "byteorder", NULL },
    { NULL, NULL }
};

void tea_import_sys(TeaState* T)
{
    tea_create_module(T, TEA_SYS_MODULE, sys_module);
    tea_push_string(T, byteorder());
    tea_set_key(T, 0, "byteorder");
    init_argv(T);
    set_version(T);
}