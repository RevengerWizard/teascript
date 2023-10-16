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
        "  -e chunk   execute string 'chunk'\n"
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

static void repl(TeaState* T)
{
    global = T;
    signal(SIGINT, tsignal);
    tea_set_repl(T, true);

    char line[1024];
    while(true)
    {
        line:
        fwrite("> ", sizeof(char), 2, stdout);

        if(!fgets(line, sizeof(line), stdin))
        {
            putchar('\n');
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

        tea_interpret(T, "=<stdin>", line);
    }
}

static int handle_script(TeaState* T, char** argv)
{
    char* path  = argv[0];
    TeaStatus status = tea_dofile(T, path);

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

#define flag_i 1
#define flag_v 2
#define flag_e 4

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
                *flags |= flag_i;
            case 'v':
                notail(argv[i])
                *flags |= flag_v;
                break;
            case 'e':
                *flags |= flag_e;
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
    return 1;
}

int main(int argc, char** argv)
{
    TeaState* T = tea_open();
    if(T == NULL)
    {
        fprintf(stderr, "Cannot create state: not enough memory");
        return EXIT_FAILURE;
    }

    int status;
    int flags = 0;
    int script = collect_args(argv, &flags);
    if(script < 0)
    {
        print_usage();
        return 0;
    }

    tea_set_argv(T, argc, argv, script);

    if(flags & flag_v)
        print_version();
    if((status = run_args(T, argv, script)) > 1)
        return status;
    if(script < argc && (status = handle_script(T, argv + script)) != TEA_OK)
    {
        return status;
    }
    if(flags & flag_i)
        repl(T);
    else if(script == argc && !(flags & (flag_e | flag_v)))
    {
        print_version();
        repl(T);
    }

    tea_close(T);

    return EXIT_SUCCESS;
}