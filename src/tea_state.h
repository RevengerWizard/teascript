#ifndef TEA_STATE_H
#define TEA_STATE_H

#include "tea.h"
#include "tea_predefines.h"
#include "tea_common.h"
#include "tea_object.h"

//#define TEA_MAX_TEMP_ROOTS 8

/*struct TeaHandle
{
    TeaValue value;
    TeaHandle* prev;
    TeaHandle* next;
};*/

typedef struct TeaState
{
    //TeaHandle* handles;
    //TeaValue* slots;
    //TeaObject* roots[TEA_MAX_TEMP_ROOTS];
    //int num_roots;

    size_t bytes_allocated;
    size_t next_gc;

    //TeaObject* objects;
    
    //int gray_count;
    //int gray_capacity;
    //TeaObject** gray_stack;

    bool repl;

    struct TeaScanner* scanner;
    struct TeaCompiler* compiler;
    struct TeaVM* vm;

    bool allow_gc;

    int argc;
    const char** argv;
} TeaState;

typedef enum TeaInterpretResult
{
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} TeaInterpretResult;

TeaState* tea_init_state();
void tea_free_state(TeaState* state);
TeaInterpretResult tea_interpret(TeaState* state, const char* module_name, const char* source);

#endif