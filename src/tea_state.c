#include <stdlib.h>
#include <string.h>

#include "tea_config.h"
#include "tea_state.h"
#include "tea_scanner.h"
#include "tea_compiler.h"
#include "tea_vm.h"

TeaState* tea_init_state()
{
    TeaState* state = (TeaState*)malloc(sizeof(TeaState));

    state->bytes_allocated = 0;
    state->next_gc = 1024 * 1024;

    state->scanner = (TeaScanner*)malloc(sizeof(TeaScanner));
    state->compiler = (TeaCompiler*)malloc(sizeof(TeaCompiler));
    state->vm = (TeaVM*)malloc(sizeof(TeaVM));

    tea_init_vm(state, state->vm);

    return state;
}

void tea_free_state(TeaState* state)
{
    free(state->scanner);

    free(state->compiler);

    tea_free_vm(state->vm);
    free(state->vm);

    free(state);
}

TeaInterpretResult tea_interpret(TeaState* state, const char* module_name, const char* source)
{
    return tea_interpret_module(state, module_name, source);
}