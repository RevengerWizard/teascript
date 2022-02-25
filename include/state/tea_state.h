#ifndef TEA_STATE_H
#define TEA_STATE_H

typedef struct TeaState
{
    struct TeaScanner* scanner;
    struct TeaCommpiler* compiler;
    struct TeaVM* vm;
} TeaState;

typedef enum
{
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} TeaInterpretResult;

TeaState* tea_init_state();
void tea_free_state();
TeaInterpretResult tea_interpret(TeaState* state, const char* module_name, const char* source);

#endif