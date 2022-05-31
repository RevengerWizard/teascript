#include <time.h>

#include "tea_module.h"
#include "tea_core.h"

static TeaValue isleap_date(TeaVM* vm, int count, TeaValue* args)
{
    if(count < 0 || count > 1)
    {
        tea_runtime_error(vm, "isleap() expected either 0 or 1 arguments (got %d)", count);
        return EMPTY_VAL;
    }

    int year;
    if(count == 0)
    {
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        year = tm.tm_year + 1900;
    }
    else if(count == 1 && IS_NUMBER(args[0]))
    {
        year = AS_NUMBER(args[0]);
    }

    return BOOL_VAL(year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

TeaValue tea_import_date(TeaVM* vm)
{
    TeaObjectString* name = tea_copy_string(vm->state, TEA_DATE_MODULE, 4);
    TeaObjectModule* module = tea_new_module(vm->state, name);

    tea_native_function(vm, &module->values, "isleap", isleap_date);

    return OBJECT_VAL(module);

}