#include <stdio.h>
#include <stdlib.h>

#include "json/jsonParseLib.h"
#include "json/jsonBuilderLib.h"

#include "tea_util.h"
#include "tea_module.h"

static TeaValue parse(TeaState* state, json_value* json)
{
    switch(json->type)
    {
        case json_none:
        case json_null:
            return NULL_VAL;
        case json_object:
        {
            TeaObjectMap* map = tea_new_map(state);

            for(unsigned int i = 0; i < json->u.object.length; i++)
            {
                TeaValue value = parse(state, json->u.object.values[i].value);
                TeaValue key = OBJECT_VAL(tea_copy_string(state, json->u.object.values[i].name, json->u.object.values[i].name_length));
                tea_map_set(state, map, key, value);
            }
            return OBJECT_VAL(map);
        }
        case json_array:
        {
            TeaObjectList* list = tea_new_list(state);

            for(unsigned int i = 0; i < json->u.array.length; i++)
            {
                TeaValue value = parse(state, json->u.array.values[i]);
                tea_write_value_array(state, &list->items, value);
            }
            return OBJECT_VAL(list);
        }
        case json_integer:
            return NUMBER_VAL(json->u.integer);
        case json_double:
            return NUMBER_VAL(json->u.dbl);
        case json_string:
            return OBJECT_VAL(tea_copy_string(state, json->u.string.ptr, json->u.string.length));
        case json_boolean:
            return BOOL_VAL(json->u.boolean);
    }
    return EMPTY_VAL;
}

static TeaValue parse_json(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "parse() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    if(IS_FILE(args[0]))
    {
        return EMPTY_VAL;
    }

    if(!IS_STRING(args[0]))
    {
        tea_runtime_error(vm, "parse() argument must be a string");
        return EMPTY_VAL;
    }

    TeaObjectString* string = AS_STRING(args[0]);
    json_value* json = json_parse(string->chars, string->length);

    if(json == NULL)
    {
        tea_runtime_error(vm, "Invalid JSON object");
        return EMPTY_VAL;
    }

    TeaValue value = parse(vm->state, json);

    if(IS_EMPTY(value))
    {
        tea_runtime_error(vm, "Invalid JSON object");
        return EMPTY_VAL;
    }

    json_value_free(json);

    return value;
}

static json_value* dump(TeaState* state, TeaValue value)
{
    if(IS_NULL(value))
        return json_null_new();
    else if(IS_BOOL(value))
        return json_boolean_new(AS_BOOL(value));
    else if(IS_NUMBER(value))
    {
        double number = AS_NUMBER(value);
        if((int)number == number)
        {
            return json_integer_new((int)number);
        }
        return json_double_new(number);
    }
    else if(IS_OBJECT(value))
    {
        switch(OBJECT_TYPE(value))
        {
            case OBJ_STRING:
                return json_string_new(AS_CSTRING(value));
            case OBJ_LIST:
            {
                TeaObjectList* list = AS_LIST(value);
                json_value* json = json_array_new(list->items.count);

                for(int i = 0; i < list->items.count; i++)
                {
                    json_array_push(json, dump(state, list->items.values[i]));
                }
                return json;
            }
            case OBJ_MAP:
            {
                TeaObjectMap* map = AS_MAP(value);
                json_value* json = json_object_new(map->count);

                for(int i = 0; i < map->capacity; i++)
                {
                    TeaMapItem* item = &map->items[i];
                    if(item->empty)
                    {
                        continue;
                    }

                    char* key;

                    if(IS_STRING(item->key))
                    {
                        TeaObjectString* s = AS_STRING(item->key);
                        key = s->chars;
                    } 
                    else
                    {
                        key = tea_value_tostring(state, item->key);
                    }

                    json_object_push(json, key, dump(state, item->value));
                }
                return json;
            }
            default:{}
        }
    }

    return NULL;
}

static TeaValue dump_json(TeaVM* vm, int count, TeaValue* args)
{
    if(count < 1 || count > 3)
    {
        tea_runtime_error(vm, "dump() expected either 1 or 2 to 3 arguments (got %d)", count);
        return EMPTY_VAL;
    }

    int indent = 4;
    int line = json_serialize_mode_single_line;

    if(count == 3)
    {
        if(!IS_FILE(args[1]))
        {
            tea_runtime_error(vm, "dump() takes a file as second argument");
            return EMPTY_VAL;
        }
        if(!IS_NUMBER(args[2])) 
        {
            tea_runtime_error(vm, "dump() takes a number as third argument");
            return EMPTY_VAL;
        }

        return EMPTY_VAL;
    }

    if(count == 2) 
    {
        if(!IS_NUMBER(args[1])) 
        {
            tea_runtime_error(vm, "dump() takes a number as second argument");
            return EMPTY_VAL;
        }

        line = json_serialize_mode_multiline;
        indent = AS_NUMBER(args[1]);
    }

    json_value* json = dump(vm->state, args[0]);

    if(json == NULL) 
    {
        tea_runtime_error(vm, "Object is not serializable");
        return EMPTY_VAL;
    }

    json_serialize_opts default_opts =
    {
        line,
        json_serialize_opt_pack_brackets,
        indent
    };

    int length = json_measure_ex(json, default_opts);
    char* buffer = ALLOCATE(vm->state, char, length);
    json_serialize_ex(buffer, json, default_opts);
    int real = strlen(buffer);

    // json_measure_ex can produce a length larger than the actual string returned
    if(real != length) 
    {
        buffer = GROW_ARRAY(vm->state, char, buffer, length, real + 1);
    }

    TeaObjectString* string = tea_take_string(vm->state, buffer, real);
    json_builder_free(json);

    return OBJECT_VAL(string);
}

TeaValue tea_import_json(TeaVM* vm)
{
    TeaObjectString* name = tea_copy_string(vm->state, TEA_JSON_MODULE, 4);
    TeaObjectModule* module = tea_new_module(vm->state, name);

    tea_native_function(vm, &module->values, "parse", parse_json);
    tea_native_function(vm, &module->values, "dump", dump_json);

    return OBJECT_VAL(module);
}