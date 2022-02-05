#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "tea_common.h"
#include "parser/tea_parser.h"
#include "debug/tea_debug.h"
#include "vm/tea_object.h"
#include "memory/tea_memory.h"
#include "vm/tea_vm.h"
#include "vm/tea_native.h"

#include "types/tea_string.h"
#include "types/tea_list.h"

#include "modules/tea_module.h"

TeaVM vm;

static void reset_stack()
{
    vm.stack_top = vm.stack;
    vm.frame_count = 0;
    vm.open_upvalues = NULL;
}

void tea_runtime_error(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for(int i = vm.frame_count - 1; i >= 0; i--)
    {
        TeaCallFrame* frame = &vm.frames[i];
        TeaObjectFunction *function = frame->closure->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ", tea_get_line(&function->chunk, instruction));
        if(function->name == NULL)
        {
            fprintf(stderr, "script\n");
        }
        else
        {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    reset_stack();
}

void tea_init_vm()
{
    reset_stack();
    vm.objects = NULL;
    vm.bytes_allocated = 0;
    vm.next_GC = 1024 * 1024;

    vm.gray_count = 0;
    vm.gray_capacity = 0;
    vm.gray_stack = NULL;

    vm.last_module = NULL;
    tea_init_table(&vm.modules);
    tea_init_table(&vm.globals);
    tea_init_table(&vm.strings);

    vm.init_string = NULL;
    vm.init_string = tea_copy_string("init", 4);

    tea_define_natives(&vm);

    tea_init_table(&vm.string_methods);
    tea_init_table(&vm.list_methods);

    tea_define_string_methods(&vm);
    tea_define_list_methods(&vm);
}

void tea_free_vm()
{
    tea_free_table(&vm.modules);
    tea_free_table(&vm.globals);
    tea_free_table(&vm.strings);
    vm.init_string = NULL;
    tea_free_objects();

    tea_free_table(&vm.string_methods);
    tea_free_table(&vm.list_methods);
}

void tea_push(TeaValue value)
{
    *vm.stack_top = value;
    vm.stack_top++;
}

TeaValue tea_pop()
{
    vm.stack_top--;
    return *vm.stack_top;
}

static TeaValue peek(int distance)
{
    return vm.stack_top[-1 - distance];
}

static bool subscript(TeaValue index_value, TeaValue subscript_value)
{
    if(IS_OBJECT(subscript_value))
    {
        switch(OBJECT_TYPE(subscript_value))
        {
            case OBJ_LIST:
            {
                if(!IS_NUMBER(index_value)) 
                {
                    tea_runtime_error("List index must be a number.");
                    return false;
                }

                TeaObjectList* list = AS_LIST(subscript_value);
                int index = AS_NUMBER(index_value);

                // Allow negative indexes
                if(index < 0)
                {
                    index = list->items.count + index;
                }

                if(index >= 0 && index < list->items.count) 
                {
                    tea_pop();
                    tea_pop();
                    tea_push(list->items.values[index]);
                    return true;
                }

                tea_runtime_error("List index out of bounds.");
                return false;
            }
            case OBJ_MAP:
            {
                TeaObjectMap* map = AS_MAP(subscript_value);
                if(!IS_STRING(index_value))
                {
                    tea_runtime_error("Map key must be a string.");
                    return false;
                }

                TeaObjectString* key = AS_STRING(index_value);
                TeaValue value;
                tea_pop();
                tea_pop();
                if(tea_table_get(&map->items, key, &value))
                {
                    tea_push(value);
                    return true;
                }

                tea_runtime_error("Key does not exist within map.");
                return false;
            }
            case OBJ_STRING:
            {
                if(!IS_NUMBER(index_value)) 
                {
                    tea_runtime_error("List index must be a number.");
                    return false;
                }

                TeaObjectString* string = AS_STRING(subscript_value);
                int index = AS_NUMBER(index_value);

                // Allow negative indexes
                if(index < 0)
                {
                    index = string->length + index;
                }

                if(index >= 0 && index < string->length)
                {
                    tea_pop();
                    tea_pop();
                    tea_push(OBJECT_VAL(tea_copy_string(&string->chars[index], 1)));
                    return true;
                }

                tea_runtime_error("String index out of bounds.");
                return false;
            }
            default:
                break;
        }
    }
    
    tea_runtime_error("not subscriptable");
    return false;
}

static bool subscript_store(TeaValue item_value, TeaValue index_value, TeaValue subscript_value)
{
    if(IS_OBJECT(subscript_value))
    {
        switch(OBJECT_TYPE(subscript_value))
        {
            case OBJ_LIST:
            {
                if(!IS_NUMBER(index_value)) 
                {
                    tea_runtime_error("List index must be a number.");
                    return false;
                }

                TeaObjectList* list = AS_LIST(subscript_value);
                int index = AS_NUMBER(index_value);

                if(index < 0)
                {
                    index = list->items.count + index;
                }

                if(index >= 0 && index < list->items.count) 
                {
                    list->items.values[index] = item_value;
                    tea_pop();
                    tea_pop();
                    tea_pop();
                    tea_push(NULL_VAL);
                    return true;
                }

                tea_runtime_error("List index out of bounds.");
                return false;
            }
            case OBJ_MAP:
            {
                TeaObjectMap* map = AS_MAP(subscript_value);
                if(!IS_STRING(index_value))
                {
                    tea_runtime_error("Map key must be a string.");
                    return false;
                }

                TeaObjectString* key = AS_STRING(index_value);
                tea_table_set(&map->items, key, item_value);
                tea_pop();
                tea_pop();
                tea_pop();
                tea_push(NULL_VAL);
                
                return true;
            }
            default:
                break;
        }
    }

    tea_runtime_error("does not support item assignment");
    return false;
}

static bool call(TeaObjectClosure* closure, int arg_count)
{
    if(arg_count != closure->function->arity)
    {
        tea_runtime_error("Expected %d arguments but got %d.", closure->function->arity, arg_count);
        return false;
    }

    if(vm.frame_count == FRAMES_MAX)
    {
        tea_runtime_error("Stack overflow.");
        return false;
    }

    TeaCallFrame* frame = &vm.frames[vm.frame_count++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm.stack_top - arg_count - 1;

    return true;
}

static bool call_value(TeaValue callee, int arg_count)
{
    if(IS_OBJECT(callee))
    {
        switch(OBJECT_TYPE(callee))
        {
            case OBJ_BOUND_METHOD:
            {
                TeaObjectBoundMethod* bound = AS_BOUND_METHOD(callee);
                vm.stack_top[-arg_count - 1] = bound->receiver;
                return call(bound->method, arg_count);
            }
            case OBJ_CLASS:
            {
                TeaObjectClass* klass = AS_CLASS(callee);
                vm.stack_top[-arg_count - 1] = OBJECT_VAL(tea_new_instance(klass));
                if(!IS_NULL(klass->initializer)) 
                {
                    return call(AS_CLOSURE(klass->initializer), arg_count);
                }
                else if(arg_count != 0)
                {
                    tea_runtime_error("Expected 0 arguments but got %d.", arg_count);
                    return false;
                }
                return true;
            }
            case OBJ_CLOSURE:
                return call(AS_CLOSURE(callee), arg_count);
            case OBJ_NATIVE:
            {
                TeaNativeFunction native = AS_NATIVE(callee);
                TeaValue result = native(arg_count, vm.stack_top - arg_count);
                
                vm.stack_top -= arg_count + 1;
                tea_push(result);
                return true;
            }
            default:
                break; // Non-callable object type
        }
    }

    tea_runtime_error("Can only call functions and classes.");
    return false;
}

static bool call_native_method(TeaValue method, int arg_count)
{
    TeaNativeFunction native = AS_NATIVE(method);
    TeaValue result = native(arg_count, vm.stack_top - arg_count - 1);

    vm.stack_top -= arg_count + 1;
    tea_push(result);

    return true;
}

static bool invoke_from_class(TeaObjectClass* klass, TeaObjectString* name, int arg_count)
{
    TeaValue method;
    if(!tea_table_get(&klass->methods, name, &method))
    {
        tea_runtime_error("Undefined property '%s'.", name->chars);
        return false;
    }

    return call(AS_CLOSURE(method), arg_count);
}

static bool invoke(TeaValue receiver, TeaObjectString* name, int arg_count)
{
    if(IS_OBJECT(receiver))
    {
        switch(OBJECT_TYPE(receiver))
        {
            case OBJ_MODULE:
            {
                TeaObjectModule* module = AS_MODULE(receiver);

                TeaValue value;
                if(!tea_table_get(&module->values, name, &value)) 
                {
                    tea_runtime_error("Undefined property '%s' in '%s' module.", name->chars, module->name->chars);
                    return false;
                }

                return call_value(value, arg_count);
            }
            case OBJ_INSTANCE:
            {
                TeaObjectInstance* instance = AS_INSTANCE(receiver);

                TeaValue value;
                if(tea_table_get(&instance->klass->methods, name, &value)) 
                {
                    return call(AS_CLOSURE(value), arg_count);
                }

                if(tea_table_get(&instance->fields, name, &value))
                {
                    vm.stack_top[-arg_count - 1] = value;
                    return call_value(value, arg_count);
                }

                tea_runtime_error("Undefined property '%s'.", name->chars);
                return false;
            }
            case OBJ_STRING:
            {
                TeaValue value;
                if(tea_table_get(&vm.string_methods, name, &value)) 
                {
                    return call_native_method(value, arg_count);
                }

                tea_runtime_error("string has no method %s().", name->chars);
                return false;
            }
            case OBJ_LIST:
            {
                TeaValue value;
                if(tea_table_get(&vm.list_methods, name, &value)) 
                {
                    if(IS_NATIVE(value)) 
                    {
                        return call_native_method(value, arg_count);
                    }

                    tea_push(peek(0));

                    for(int i = 2; i <= arg_count + 1; i++) 
                    {
                        vm.stack_top[-i] = peek(i);
                    }

                    return call(AS_CLOSURE(value), arg_count + 1);
                }

                tea_runtime_error("list has no method %s().", name->chars);
                return false;
            }
            default:
                break;
        }
    }
    
    tea_runtime_error("Only instances have methods.");
    return false;
}

static bool bind_method(TeaObjectClass* klass, TeaObjectString* name)
{
    TeaValue method;
    if(!tea_table_get(&klass->methods, name, &method))
    {
        tea_runtime_error("Undefined property '%s'.", name->chars);
        return false;
    }

    TeaObjectBoundMethod* bound = tea_new_bound_method(peek(0), AS_CLOSURE(method));
    tea_pop();
    tea_push(OBJECT_VAL(bound));
    return true;
}

static bool get_property(TeaValue receiver, TeaObjectString* name)
{
    if(IS_OBJECT(receiver))
    {
        switch(OBJECT_TYPE(receiver))
        {
            case OBJ_INSTANCE:
            {
                TeaObjectInstance* instance = AS_INSTANCE(receiver);
                TeaValue value;

                if(tea_table_get(&instance->fields, name, &value))
                {
                    tea_pop(); // Instance.
                    tea_push(value);
                    return true;
                }

                if(!bind_method(instance->klass, name))
                {
                    return false;
                }

                tea_runtime_error("'%s' instance has no property: '%s'.", instance->klass->name->chars, name->chars);
                return false;
            }
            case OBJ_MODULE:
            {
                TeaObjectModule* module = AS_MODULE(receiver);
                TeaValue value;

                if(tea_table_get(&module->values, name, &value)) 
                {
                    tea_pop(); // Module.
                    tea_push(value);
                    return true;
                }

                tea_runtime_error("'%s' module has no property: '%s'.", module->name->chars, name->chars);
                return false;
            }
            case OBJ_MAP:
            {
                TeaObjectMap* map = AS_MAP(receiver);
                TeaValue value;

                if(tea_table_get(&map->items, name, &value))
                {
                    tea_pop();
                    tea_push(value);
                    return true;
                }
                
                tea_runtime_error("Map has no property");
                return false;
            }
            default:
                break;
        }
    }

    tea_runtime_error("Only instances have properties.");
    return false;
}

static bool set_property(TeaObjectString* name, TeaValue receiver)
{
    if(IS_OBJECT(receiver))
    {
        switch(OBJECT_TYPE(receiver))
        {
            case OBJ_INSTANCE:
            {
                TeaObjectInstance* instance = AS_INSTANCE(receiver);
                tea_table_set(&instance->fields, name, peek(0));
                tea_pop();
                tea_pop();
                tea_push(NULL_VAL);
                return true;
            }
            case OBJ_MAP:
            {
                TeaObjectMap* map = AS_MAP(receiver);
                tea_table_set(&map->items, name, peek(0));
                tea_pop();
                tea_pop();
                tea_pop();
                tea_push(NULL_VAL);
                return true;
            }
            default:
                break;
        }
    }

    tea_runtime_error("Can not set property on type");
    return false;
}

static TeaObjectUpvalue* capture_upvalue(TeaValue* local)
{
    TeaObjectUpvalue* prev_upvalue = NULL;
    TeaObjectUpvalue* upvalue = vm.open_upvalues;
    while(upvalue != NULL && upvalue->location > local)
    {
        prev_upvalue = upvalue;
        upvalue = upvalue->next;
    }

    if(upvalue != NULL && upvalue->location == local)
    {
        return upvalue;
    }

    TeaObjectUpvalue* created_upvalue = tea_new_upvalue(local);
    created_upvalue->next = upvalue;

    if(prev_upvalue == NULL)
    {
        vm.open_upvalues = created_upvalue;
    }
    else
    {
        prev_upvalue->next = created_upvalue;
    }

    return created_upvalue;
}

static void close_upvalues(TeaValue* last)
{
    while(vm.open_upvalues != NULL && vm.open_upvalues->location >= last)
    {
        TeaObjectUpvalue* upvalue = vm.open_upvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm.open_upvalues = upvalue->next;
    }
}

static void define_method(TeaObjectString* name)
{
    TeaValue method = peek(0);
    TeaObjectClass* klass = AS_CLASS(peek(1));
    tea_table_set(&klass->methods, name, method);
    if(name == vm.init_string) klass->initializer = method;
    tea_pop();
}

static void concatenate()
{
    TeaObjectString* b = AS_STRING(peek(0));
    TeaObjectString* a = AS_STRING(peek(1));

    int length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    TeaObjectString* result = tea_take_string(chars, length);
    tea_pop();
    tea_pop();
    tea_push(OBJECT_VAL(result));
}

static InterpretResult run()
{
    TeaCallFrame* frame = &vm.frames[vm.frame_count - 1];

    register uint8_t* ip = frame->ip;

#define STORE_FRAME frame->ip = ip
#define READ_BYTE() (*ip++)
#define READ_SHORT() \
    (ip += 2, \
    (uint16_t)((ip[-2] << 8) | ip[-1]))

#define READ_CONSTANT() \
    (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())

#define OP_ERROR(op) 0

#define BINARY_OP(value_type, op)                        \
    do                                                  \
    {                                                   \
        if(!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) \
        {   \
            STORE_FRAME;                             \
            tea_runtime_error("Operands must be numbers.");  \
            return INTERPRET_RUNTIME_ERROR;             \
        }                                               \
        double b = AS_NUMBER(tea_pop());                    \
        double a = AS_NUMBER(tea_pop());                    \
        tea_push(value_type(a op b));                        \
    } \
    while(false)

#define RUNTIME_ERROR(...) \
    do \
    { \
        STORE_FRAME; \
        tea_runtime_error(__VA_ARGS__); \
        return INTERPRET_RUNTIME_ERROR; \
    } \
    while(false)

#ifdef DEBUG_TRACE_EXECUTION
    #define DEBUG() \
        do \
        { \
            printf("          "); \
            for(TeaValue* slot = vm.stack; slot < vm.stack_top; slot++) \
            { \
                printf("[ "); \
                tea_print_value(*slot); \
                printf(" ]"); \
            } \
            printf("\n"); \
            tea_disassemble_instruction(&frame->closure->function->chunk, (int)(frame->ip - frame->closure->function->chunk.code)); \
        } \
        while(false)
#else
    #define DEBUG() do { } while (false)
#endif

#ifdef COMPUTED_GOTO

    static void* dispatch_table[] = {
        #define OPCODE(name) &&OP_##name
        #include "vm/tea_opcodes.h"
        #undef OPCODE
    };

    #define DISPATCH() \
        do \
        { \
            DEBUG(); \
            goto *dispatch_table[instruction = READ_BYTE()]; \
        } \
        while(false)

    #define INTREPRET_LOOP  DISPATCH();
    #define CASE_CODE(name) OP_##name

#else

    #define INTREPRET_LOOP \
        loop: \
            DEBUG(); \
            switch(instruction = READ_BYTE())

    #define DISPATCH() goto loop

    #define CASE_CODE(name) case OP_##name

#endif

    while(true)
    {
        uint8_t instruction;
        INTREPRET_LOOP
        {
            CASE_CODE(CONSTANT):
            {
                TeaValue constant = READ_CONSTANT();
                tea_push(constant);
                DISPATCH();
            }
            CASE_CODE(CONSTANT_LONG):
                DISPATCH();
            CASE_CODE(NULL):
                tea_push(NULL_VAL);
                DISPATCH();
            CASE_CODE(TRUE):
                tea_push(BOOL_VAL(true));
                DISPATCH();
            CASE_CODE(FALSE):
                tea_push(BOOL_VAL(false));
                DISPATCH();
            CASE_CODE(DUP): 
                tea_push(peek(0)); 
                DISPATCH();
            CASE_CODE(POP):
                tea_pop();
                DISPATCH();
            CASE_CODE(GET_LOCAL):
            {
                uint8_t slot = READ_BYTE();
                tea_push(frame->slots[slot]);
                DISPATCH();
            }
            CASE_CODE(SET_LOCAL):
            {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(0);
                DISPATCH();
            }
            CASE_CODE(GET_GLOBAL):
            {
                TeaObjectString* name = READ_STRING();
                TeaValue value;
                if(!tea_table_get(&vm.globals, name, &value))
                {
                    RUNTIME_ERROR("Undefined variable '%s'.", name->chars);
                }
                tea_push(value);
                DISPATCH();
            }
            CASE_CODE(DEFINE_GLOBAL):
            {
                TeaObjectString* name = READ_STRING();
                tea_table_set(&vm.globals, name, peek(0));
                tea_pop();
                DISPATCH();
            }
            CASE_CODE(SET_GLOBAL):
            {
                TeaObjectString* name = READ_STRING();
                if(tea_table_set(&vm.globals, name, peek(0)))
                {
                    tea_table_delete(&vm.globals, name);
                    RUNTIME_ERROR("Undefined variable '%s'.", name->chars);
                }
                DISPATCH();
            }
            CASE_CODE(GET_UPVALUE):
            {
                uint8_t slot = READ_BYTE();
                tea_push(*frame->closure->upvalues[slot]->location);
                DISPATCH();
            }
            CASE_CODE(SET_UPVALUE):
            {
                uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->location = peek(0);
                DISPATCH();
            }
            CASE_CODE(GET_PROPERTY):
            {
                TeaValue receiver = peek(0);
                TeaObjectString* name = READ_STRING();
                STORE_FRAME;
                if(!get_property(receiver, name))
                {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frame_count - 1];
                ip = frame->ip;
                DISPATCH();
            }
            CASE_CODE(SET_PROPERTY):
            {
                TeaObjectString* name = READ_STRING();
                TeaValue receiver = peek(1);
                STORE_FRAME;
                if(!set_property(name, receiver))
                {
                    return INTERPRET_RUNTIME_ERROR;
                }
                DISPATCH();
            }
            CASE_CODE(GET_SUPER):
            {
                TeaObjectString* name = READ_STRING();
                TeaObjectClass* superclass = AS_CLASS(tea_pop());

                if(!bind_method(superclass, name))
                {
                    return INTERPRET_RUNTIME_ERROR;
                }
                DISPATCH();
            }
            CASE_CODE(LIST):
            {
                // Stack before: [item1, item2, ..., itemN] and after: [list]
                uint8_t item_count = READ_BYTE();
                TeaObjectList* list = tea_new_list();

                // Add item to list
                tea_push(OBJECT_VAL(list)); // So list isn't sweeped by GC when appending the list
                for(int i = item_count; i > 0; i--)
                {
                    tea_write_value_array(&list->items, peek(i));
                }
                
                // Pop items from stack
                vm.stack_top -= item_count + 1;

                tea_push(OBJECT_VAL(list));
                DISPATCH();
            }
            CASE_CODE(MAP):
            {
                uint8_t item_count = READ_BYTE();
                TeaObjectMap* map = tea_new_map();

                tea_push(OBJECT_VAL(map));

                for(int i = item_count * 2; i > 0; i -= 2)
                {
                    tea_table_set(&map->items, AS_STRING(peek(i)), peek(i - 1));
                }

                vm.stack_top -= item_count * 2 + 1;

                tea_push(OBJECT_VAL(map));
                DISPATCH();
            }
            CASE_CODE(SUBSCRIPT):
            {
                // Stack before: [list, index] and after: [index(list, index)]
                TeaValue index = peek(0);
                TeaValue list = peek(1);
                STORE_FRAME;
                if(!subscript(index, list))
                {
                    return INTERPRET_RUNTIME_ERROR;
                }
                DISPATCH();
            }
            CASE_CODE(SUBSCRIPT_STORE):
            {
                // Stack before list: [list, index, item] and after: [item]
                TeaValue item = peek(0);
                TeaValue index = peek(1);
                TeaValue list = peek(2);
                STORE_FRAME;
                if(!subscript_store(item, index, list))
                {
                    return INTERPRET_RUNTIME_ERROR;
                }
                DISPATCH();
            }
            CASE_CODE(EQUAL):
            {
                TeaValue b = tea_pop();
                TeaValue a = tea_pop();
                tea_push(BOOL_VAL(tea_values_equal(a, b)));
                DISPATCH();
            }
            CASE_CODE(GREATER):
                BINARY_OP(BOOL_VAL, >);
                DISPATCH();
            CASE_CODE(LESS):
                BINARY_OP(BOOL_VAL, <);
                DISPATCH();
            CASE_CODE(ADD):
            {
                if(IS_STRING(peek(0)) && IS_STRING(peek(1)))
                {
                    concatenate();
                }
                else if(IS_NUMBER(peek(0)) && IS_NUMBER(peek(1)))
                {
                    double b = AS_NUMBER(tea_pop());
                    double a = AS_NUMBER(tea_pop());
                    tea_push(NUMBER_VAL(a + b));
                }
                else
                {
                    RUNTIME_ERROR("Operands must be two numbers or two strings.");
                }
                DISPATCH();
            }
            /*case OP_SUBTRACT:
                BINARY_OP(NUMBER_VAL, -);
                DISPATCH();*/
            CASE_CODE(MULTIPLY):
                BINARY_OP(NUMBER_VAL, *);
                DISPATCH();
            CASE_CODE(DIVIDE):
                BINARY_OP(NUMBER_VAL, /);
                DISPATCH();
            CASE_CODE(NOT):
                tea_push(BOOL_VAL(tea_is_falsey(tea_pop())));
                DISPATCH();
            CASE_CODE(NEGATE):
                if(!IS_NUMBER(peek(0)))
                {
                    RUNTIME_ERROR("Operand must be a number.");
                }
                tea_push(NUMBER_VAL(-AS_NUMBER(tea_pop())));
                DISPATCH();
            CASE_CODE(JUMP):
            {
                uint16_t offset = READ_SHORT();
                ip += offset;
                DISPATCH();
            }
            CASE_CODE(JUMP_IF_FALSE):
            {
                uint16_t offset = READ_SHORT();
                if(tea_is_falsey(peek(0)))
                    ip += offset;
                DISPATCH();
            }
            CASE_CODE(LOOP):
            {
                uint16_t offset = READ_SHORT();
                ip -= offset;
                DISPATCH();
            }
            CASE_CODE(CALL):
            {
                int arg_count = READ_BYTE();
                STORE_FRAME;
                if(!call_value(peek(arg_count), arg_count))
                {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frame_count - 1];
                ip = frame->ip;
                DISPATCH();
            }
            CASE_CODE(INVOKE):
            {
                TeaObjectString* method = READ_STRING();
                int arg_count = READ_BYTE();
                TeaValue receiver = peek(arg_count);
                STORE_FRAME;
                if(!invoke(receiver, method, arg_count))
                {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frame_count - 1];
                ip = frame->ip;
                DISPATCH();
            }
            CASE_CODE(SUPER):
            {
                TeaObjectString* method = READ_STRING();
                int arg_count = READ_BYTE();
                STORE_FRAME;
                TeaObjectClass* superclass = AS_CLASS(tea_pop());
                if(!invoke_from_class(superclass, method, arg_count))
                {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frame_count - 1];
                ip = frame->ip;
                DISPATCH();
            }
            CASE_CODE(CLOSURE):
            {
                TeaObjectFunction* function = AS_FUNCTION(READ_CONSTANT());
                TeaObjectClosure* closure = tea_new_closure(function);
                tea_push(OBJECT_VAL(closure));
                for(int i = 0; i < closure->upvalue_count; i++)
                {
                    uint8_t isLocal = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if(isLocal)
                    {
                        closure->upvalues[i] = capture_upvalue(frame->slots + index);
                    }
                    else
                    {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }
                DISPATCH();
            }
            CASE_CODE(CLOSE_UPVALUE):
                close_upvalues(vm.stack_top - 1);
                tea_pop();
                DISPATCH();
            CASE_CODE(RETURN):
            {
                TeaValue result = tea_pop();
                close_upvalues(frame->slots);
                vm.frame_count--;
                if(vm.frame_count == 0)
                {
                    tea_pop();
                    return INTERPRET_OK;
                }

                vm.stack_top = frame->slots;
                tea_push(result);
                frame = &vm.frames[vm.frame_count - 1];
                ip = frame->ip;
                DISPATCH();
            }
            CASE_CODE(CLASS):
                tea_push(OBJECT_VAL(tea_new_class(READ_STRING())));
                DISPATCH();
            CASE_CODE(INHERIT):
            {
                TeaValue superclass = peek(1);
                if(!IS_CLASS(superclass))
                {
                    tea_runtime_error("Superclass must be a class.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                TeaObjectClass* subclass = AS_CLASS(peek(0));
                tea_table_add_all(&AS_CLASS(superclass)->methods, &subclass->methods);
                tea_pop(); // Subclass
                DISPATCH();
            }
            CASE_CODE(METHOD):
                define_method(READ_STRING());
                DISPATCH();
            CASE_CODE(IMPORT):
            {
                DISPATCH();
            }
            CASE_CODE(IMPORT_NATIVE):
            {
                int index = READ_BYTE();
                TeaObjectString* file_name = READ_STRING();

                TeaValue module_val;
                // If the module is already imported, skip
                if(tea_table_get(&vm.modules, file_name, &module_val))
                {
                    vm.last_module = AS_MODULE(module_val);
                    tea_push(module_val);
                    DISPATCH();
                }

                TeaValue module = tea_import_native_module(&vm, index);

                tea_push(module);

                if(IS_CLOSURE(module)) 
                {
                    STORE_FRAME;
                    call(AS_CLOSURE(module), 0);
                    frame = &vm.frames[vm.frame_count - 1];
                    ip = frame->ip;

                    tea_table_get(&vm.modules, file_name, &module);
                    vm.last_module = AS_MODULE(module);
                }

                DISPATCH();
            }
            CASE_CODE(IMPORT_NATIVE_VARIABLE):
            {
                TeaObjectString* file_name = READ_STRING();
                int var_count = READ_BYTE();

                TeaValue module_val;;
                TeaObjectModule* module;

                if(tea_table_get(&vm.modules, file_name, &module_val)) 
                {
                    module = AS_MODULE(module_val);
                } 
                else 
                {
                    RUNTIME_ERROR("ERROR!!");
                }

                for(int i = 0; i < var_count; i++) 
                {
                    TeaValue module_variable;
                    TeaObjectString* variable = READ_STRING();

                    if(!tea_table_get(&module->values, variable, &module_variable)) 
                    {
                        RUNTIME_ERROR("%s can't be found in module %s", variable->chars, module->name->chars);
                    }

                    tea_push(module_variable);
                }

                DISPATCH();
            }
            CASE_CODE(FILE_OPEN):
            {
                DISPATCH();
            }
            CASE_CODE(FILE_CLOSE):
            {
                DISPATCH();
            }
        }
    }

#undef STORE_FRAME
#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
#undef RUNTIME_ERROR
}

void hack(bool b)
{
    run();
    if(b)
        hack(false);
}

InterpretResult tea_interpret(const char* source)
{
    TeaObjectFunction* function = tea_compile(source);
    if(function == NULL)
        return INTERPRET_COMPILE_ERROR;

    tea_push(OBJECT_VAL(function));
    TeaObjectClosure* closure = tea_new_closure(function);
    tea_pop();
    tea_push(OBJECT_VAL(closure));
    call(closure, 0);

    return run();
}

bool tea_is_falsey(TeaValue value)
{
    return  IS_NULL(value) || 
            (IS_BOOL(value) && !AS_BOOL(value)) || 
            (IS_NUMBER(value) && AS_NUMBER(value) == 0) || 
            (IS_STRING(value) && AS_CSTRING(value)[0] == '\0') || 
            (IS_LIST(value) && AS_LIST(value)->items.count == 0) ||
            (IS_MAP(value) && AS_MAP(value)->items.count == 0);
}