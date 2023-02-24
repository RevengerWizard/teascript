// tea_module.c
// Teascript module loading

#include "tea.h"

#include "tea_state.h"
#include "tea_import.h"

static const TeaReg modules[] = {
    { TEA_MATH_MODULE, tea_import_math },
    { TEA_TIME_MODULE, tea_import_time },
    { TEA_OS_MODULE, tea_import_os },
    { TEA_SYS_MODULE, tea_import_sys },
    { TEA_IO_MODULE, tea_import_io },
    { TEA_RANDOM_MODULE, tea_import_random },
    { NULL, NULL }
};

void tea_import_native_module(TeaState* T, int index)
{
    tea_push_cfunction(T, modules[index].fn);
    tea_call(T, 0);
}

int tea_find_native_module(char* name, int length)
{
    for(int i = 0; modules[i].name != NULL; i++) 
    {
        if(strncmp(modules[i].name, name, length) == 0 && length == strlen(modules[i].name)) 
        {
            return i;
        }
    }

    return -1;
}