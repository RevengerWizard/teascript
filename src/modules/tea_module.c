#include "modules/tea_module.h"

TeaNativeModules modules[] = {
    { "math", 4, &tea_import_math },
    { "time", 4, &tea_import_time },
    { NULL, 0, NULL }
};

TeaValue tea_import_native_module(TeaVM* vm, int index)
{
    return modules[index].module(vm);
}

int tea_find_native_module(char* name, int length)
{
    for(int i = 0; modules[i].module != NULL; i++) 
    {
        if(strncmp(modules[i].name, name, length) == 0 && length == modules[i].length) 
        {
            return i;
        }
    }

    return -1;
}