#include "modules/tea_module.h"

TeaNativeModules modules[] = {
    { "math", &tea_import_math },
    { "time", &tea_import_time },
    { NULL, NULL }
};

TeaValue tea_import_native_module(TeaVM* vm, int index)
{
    return modules[index].module(vm);
}

int tea_find_native_module(char* name, int length)
{
    for(int i = 0; modules[i].module != NULL; i++) 
    {
        if(strncmp(modules[i].name, name, length) == 0) 
        {
            return i;
        }
    }

    return -1;
}