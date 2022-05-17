#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tea_common.h"
#include "tea_chunk.h"
#include "tea_debug.h"
#include "tea_state.h"
#include "tea_config.h"

static void clear()
{
    #if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
        system("clear");
    #endif

    #if defined(_WIN32) || defined(_WIN64)
        system("cls");
    #endif
}

static void repl(TeaState* state)
{
    char line[1024];
    while(true)
    {
        line:
        printf("> ");

        if(!fgets(line, sizeof(line), stdin))
        {
            printf("\n");
            break;
        }

        if(strcmp(line, "exit\n") == 0)
        {
            break;
        }
        
        if(strcmp(line, "clear\n") == 0)
        {
            clear();
            goto line;
        }

        tea_interpret(state, "repl", line);
    }
}

static char* read_file(const char* path)
{
    FILE* file = fopen(path, "rb");
    if(file == NULL)
    {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(file_size + 1);
    if(buffer == NULL)
    {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }

    size_t bytes_read = fread(buffer, sizeof(char), file_size, file);
    if(bytes_read < file_size)
    {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }

    buffer[bytes_read] = '\0';

    fclose(file);

    return buffer;
}

static void run_file(TeaState* state, const char* path)
{
    char* source = read_file(path);

    TeaInterpretResult result = tea_interpret(state, path, source);
    free(source);

    if(result == INTERPRET_COMPILE_ERROR)
        exit(65);
    if(result == INTERPRET_RUNTIME_ERROR)
        exit(70);
}

int main(int argc, const char* argv[])
{
    TeaState* state = tea_init_state();
    state->argc = argc;
    state->argv = argv;

    if(argc == 1)
    {
        state->repl = true;
        printf("%s\n", TEA_RELEASE);
        repl(state);
    }
    else if(argc >= 2)
    {
        state->repl = false;
        run_file(state, argv[1]);
    }
    else
    {
        fprintf(stderr, "Usage: tea [path]\n");
        exit(64);
    }

    tea_free_state(state);

    return 0;
}