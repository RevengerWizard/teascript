#include <stdlib.h>
#include <math.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <winsock2.h>
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
#include "tea_config.h"

static TeaValue exit_sys(TeaVM* vm, int count, TeaValue* args)
{
    if(count == 0) exit(0);
    
    if(!IS_NUMBER(args[0]))
    {
        tea_runtime_error(vm, "exit() takes a number value as argument");
        return EMPTY_VAL;
    }

    exit(NUMBER_VAL(args[0]));

    return NULL_VAL;
}

static TeaValue sleep_sys(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "sleep() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_NUMBER(args[0]))
    {
        tea_runtime_error(vm, "sleep() argument must be a number");
        return EMPTY_VAL;
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

    return NULL_VAL;
}

static void init_argv(TeaState* state, TeaTable* table)
{
    int argc = state->argc;
    const char** argv = state->argv;

    TeaObjectList* list = tea_new_list(state);

    for(int i = 1; i < argc; i++)
    {
        TeaValue arg = OBJECT_VAL(tea_copy_string(state, argv[i], strlen(argv[i])));
        tea_write_value_array(state, &list->items, arg);
    }

    tea_native_value(state->vm, table, "argv", OBJECT_VAL(list));
}

static void set_version(TeaState* state, TeaTable* table)
{
    TeaObjectMap* version = tea_new_map(state);

    TeaObjectString* major = tea_copy_string(state, "major", 5);
    TeaObjectString* minor = tea_copy_string(state, "minor", 5);
    TeaObjectString* patch = tea_copy_string(state, "patch", 5);
    
    tea_map_set(state, version, OBJECT_VAL(major), NUMBER_VAL(TEA_VERSION_MAJOR));
    tea_map_set(state, version, OBJECT_VAL(minor), NUMBER_VAL(TEA_VERSION_MINOR));
    tea_map_set(state, version, OBJECT_VAL(patch), NUMBER_VAL(TEA_VERSION_PATCH));

    tea_native_value(state->vm, table, "version", OBJECT_VAL(version));
}

static TeaValue byteorder(TeaState* state)
{
    int x = 1;
    bool order = *(char*)&x;

    if(order)
    {
        return OBJECT_VAL(tea_copy_string(state, "little", 6));
    }
    
    return OBJECT_VAL(tea_copy_string(state, "big", 3));
}

TeaValue tea_import_sys(TeaVM* vm)
{
    TeaObjectString* name = tea_copy_string(vm->state, TEA_SYS_MODULE, 3);
    TeaObjectModule* module = tea_new_module(vm->state, name);

    init_argv(vm->state, &module->values);
    set_version(vm->state, &module->values);

    tea_native_value(vm, &module->values, "byteorder", byteorder(vm->state));

    tea_native_function(vm, &module->values, "sleep", sleep_sys);
    tea_native_function(vm, &module->values, "exit", exit_sys);

    return OBJECT_VAL(module);
}