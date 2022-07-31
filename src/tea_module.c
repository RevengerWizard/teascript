#include "tea_module.h"

TeaNativeModule modules[] = {
    { TEA_MATH_MODULE, tea_import_math },
    { TEA_TIME_MODULE, tea_import_time },
    //{ TEA_DATE_MODULE, tea_import_date },
    //{ TEA_JSON_MODULE, tea_import_json },
    //{ TEA_CSV_MODULE, tea_import_csv },
    //{ TEA_HTTP_MODULE, tea_import_http },
    //{ TEA_SOCKET_MODULE, tea_import_socket },
    //{ TEA_HASH_MODULE, tea_import_hash },
    //{ TEA_WEB_MODULE, tea_import_web },
    { TEA_OS_MODULE, tea_import_os },
    { TEA_SYS_MODULE, tea_import_sys },
    //{ TEA_PATH_MODULE, tea_import_path },
    { TEA_RANDOM_MODULE, tea_import_random },
    //{ TEA_FFI_MODULE, tea_import_ffi },
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