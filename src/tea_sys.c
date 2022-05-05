#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <ws2tcpip.h>
#define REMOVE remove
#define MKDIR(d, m) ((void)m, _mkdir(d))
#else
#include <unistd.h>
#include <sys/utsname.h>
#define HAS_ACCESS
#define REMOVE unlink
#define MKDIR(d, m) mkdir(d, m)
#endif

#include "tea_module.h"
#include "tea_core.h"

static TeaValue exit_sys(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(exit, 0);

    exit(0);

    return EMPTY_VAL;
}

static TeaValue sleep_sys(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(sleep, 1);

    if(!IS_NUMBER(args[0]))
    {
        NATIVE_ERROR("sleep() argument must be a number.");
    }

    double stop_time = AS_NUMBER(args[0]);

#ifdef _WIN32
    Sleep(stop_time * 1000);
#elif _POSIX_C_SOURCE >= 199309L
    struct timespec ts;
    ts.tv_sec = stop_time;
    ts.tv_nsec = fmod(stop_time, 1) * 1000000000;
    nanosleep(&ts, NULL);
#else
    if(stop_time >= 1)
        sleep(stop_time);

    // 1000000 = 1 second
    usleep(fmod(stop_time, 1) * 1000000);
#endif

    return EMPTY_VAL;
}

TeaValue tea_import_sys(TeaVM* vm)
{
    TeaObjectString* name = tea_copy_string(vm->state, TEA_SYS_MODULE, 3);
    TeaObjectModule* module = tea_new_module(vm->state, name);

    tea_native_function(vm, &module->values, "sleep", sleep_sys);
    tea_native_function(vm, &module->values, "exit", exit_sys);

    return OBJECT_VAL(module);
}