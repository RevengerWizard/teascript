/*
** tea.c
** Teascript standalone interpreter
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>

#define tea_c

#include "tea.h"

#include "tea_arch.h"

#define PROMPT  "> "
#define PROMPT2  "... "
#define MAX_INPUT 512

static TeaState* global = NULL;

void tsignal(int id)
{
    tea_close(global);
    exit(0);
}

static void clear()
{
#if TEA_TARGET_POSIX
    system("clear");
#elif TEA_TARGET_WINDOWS
    system("cls");
#endif
}

static void print_usage()
{
    fputs("tea: ", stderr);
    fputs(
        "usage: tea [options] [script [args]]\n"
        "Available options are:\n"
        "  -e code    execute string 'code'\n"
        "  -i         enter interactive mode after executing 'script'\n"
        "  -v         show version information\n"
        "  --         stop handling options\n"
        "  -          stop handling options and execute stdin\n",
        stderr);
    fflush(stderr);
}

static void print_version()
{
    fputs("teascript v" TEA_VERSION "\n", stdout);
}

static void t_message(const char* msg)
{
    fputs("tea: ", stderr);
    fputs(msg, stderr);
    fputc('\n', stderr);
    fflush(stderr);
}

static void write_prompt(bool firstline)
{
    const char* prompt = firstline ? PROMPT : PROMPT2;
    fputs(prompt, stdout);
    fflush(stdout);
}

static bool match_braces(const char* line)
{
    int level = 0;

    for(int i = 0; line[i]; i++)
    {
        if(line[i] == '\0')
        {
            break;
        }
        else if (line[i] == '{')
        {
            level++;
        }
        else if (line[i] == '}')
        {
            level--;
        }

        if(level < 0)
        {
            return true; // closed brace before opening, end line now
        }
    }

    return level == 0;
}

static void repl(TeaState* T)
{
    global = T;
    signal(SIGINT, tsignal);
    tea_set_repl(T, true);

    const char* line;

    while(true)
    {
        line:
        write_prompt(true);

        tea_push_literal(T, "");

        char buffer[MAX_INPUT];
        while(fgets(buffer, MAX_INPUT, stdin) != NULL)
        {
            tea_push_string(T, buffer);
            tea_concat(T);
            line = tea_get_string(T, -1);

            if(!match_braces(line))
            {
                write_prompt(false);
                continue;
            }

            if(strlen(buffer) != MAX_INPUT - 1 || buffer[MAX_INPUT-2] == '\n')
            {
                break;
            }
        }

        line = tea_get_string(T, -1);

        if(line[0] == '\0')
        {
            break;
        }

        if(strcmp(line, "exit\n") == 0)
        {
            break;
        }

        if(strcmp(line, "clear\n") == 0)
        {
            clear();
            tea_pop(T, 1);
            goto line;
        }

        tea_interpret(T, "=<stdin>", line);
        tea_pop(T, 1);
        tea_pop(T, 1);
    }

    tea_set_top(T, 0);
    putchar('\n');
}

static int handle_script(TeaState* T, char** argv)
{
    char* path  = argv[0];
    int status = tea_dofile(T, path);
    tea_pop(T, 1);

    if(status == TEA_SYNTAX_ERROR)
    {
        return 65;
    }
    if(status == TEA_RUNTIME_ERROR)
    {
        return 70;
    }
    if(status == TEA_FILE_ERROR)
    {
        fputs("tea: ", stderr);
        fprintf(stderr, "Cannot open '%s': No such file or directory", path);
        fputc('\n', stderr);
        fflush(stderr);
        return 75;
    }
    return status;
}

#define notail(x)   { if ((x)[2] != '\0') return -1; }

#define FLAG_I 1
#define FLAG_V 2
#define FLAG_E 4

static int collect_args(char** argv, int* flags)
{
    int i;
    for(i = 1; argv[i] != NULL; i++)
    {
        if(argv[i][0] != '-')
            return i;
        switch(argv[i][1])
        {
            case '-':
                notail(argv[i])
                return i + 1;
            case '\0':
                return i;
            case 'i':
                notail(argv[i]);
                *flags |= FLAG_I;
            case 'v':
                notail(argv[i])
                *flags |= FLAG_V;
                break;
            case 'e':
                *flags |= FLAG_E;
                if (argv[i][2] == '\0')
                {
                    i++;
                    if(argv[i] == NULL)
                        return -1;
                }
                break;
            default:
                return -1;
        }
    }
    return i;
}

static int run_args(TeaState* T, char** argv, int n)
{
    int i;
    for(i = 1; i < n; i++)
    {
        switch(argv[i][1])
        {
            case 'e':
            {
                char* chunk = argv[i] + 2;
                if(*chunk == '\0')
                    chunk = argv[++i];

                int status = tea_interpret(T, "=<stdin>", chunk);
                tea_pop(T, 1);
                if(status == TEA_SYNTAX_ERROR)
                {
                    return 65;
                }
                if(status == TEA_RUNTIME_ERROR)
                {
                    return 70;
                }
                break;
            }
        }
    }
    return 0;
}

int main(int argc, char** argv)
{
    TeaState* T = tea_open();
    if(T == NULL)
    {
        t_message("Cannot create state: not enough memory");
        return EXIT_FAILURE;
    }

    int status = EXIT_SUCCESS;
    int flags = 0;
    int script = collect_args(argv, &flags);
    if(script < 0)
    {
        print_usage();
        goto finish;
    }

    tea_set_argv(T, argc, argv, script);

    if(flags & FLAG_V)
        print_version();
    if((status = run_args(T, argv, script)) > 0)
        goto finish;
    if(script < argc && (status = handle_script(T, argv + script)) != TEA_OK)
    {
        goto finish;
    }
    if(flags & FLAG_I)
        repl(T);
    else if(script == argc && !(flags & (FLAG_E | FLAG_V)))
    {
        print_version();
        repl(T);
    }

    finish:
    tea_close(T);

    return status;
}