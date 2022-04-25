#include "tea_module.h"

TeaNativeModules modules[] = {
    { "math", tea_import_math },
    { "time", tea_import_time },
    //{ "date", tea_import_date },
    //{ "json", tea_import_json },
    //{ "csv", tea_import_csv },
    //{ "log", tea_import_log },
    //{ "palette", tea_import_palette },
    //{ "http", tea_import_http },
    //{ "socket", tea_import_socket },
    //{ "process", tea_import_process },
    //{ "hash", tea_import_hash },
    //{ "web", tea_import_web },
    //{ "os", tea_import_os },
    //{ "sys", tea_import_sys },
    //{ "path", tea_import_path },
    //{ "random", tea_import_random },
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
        if(strncmp(modules[i].name, name, length) == 0 && length == strlen(modules[i].name)) 
        {
            return i;
        }
    }

    return -1;
}