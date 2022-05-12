#ifndef TEA_STATE_H
#define TEA_STATE_H

#include "tea_predefines.h"
#include "tea_common.h"
#include "tea_object.h"

typedef struct TeaState
{
    size_t bytes_allocated;
    size_t next_gc;

    bool repl;

    struct TeaScanner* scanner;
    struct TeaCompiler* compiler;
    struct TeaVM* vm;

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