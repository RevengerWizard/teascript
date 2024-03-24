/*
** tea.c
** Teascript standalone interpreter
*/

#define tea_c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>

#include "tea.h"
#include "tea_obj.h"

#include "tea_arch.h"

#if TEA_TARGET_POSIX
#include <unistd.h>
#define stdin_is_tty()  isatty(0)
#elif TEA_TARGET_WINDOWS
#include <io.h>
#ifdef __BORLANDC__
#define stdin_is_tty()  isatty(_fileno(stdin))
#else
#define stdin_is_tty()  _isatty(_fileno(stdin))
#endif
#else
#define stdin_is_tty()  1
#endif

#define PROMPT1  "> "
#define PROMPT2  "... "
#define MAX_INPUT 512

static tea_State* global = NULL;
static char* empty_argv[2] = { NULL, NULL };

void tsignal(int id)
{
    signal(id, SIG_DFL);
    tea_error(global, "Interrupted");
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

static int interpret(tea_State* T, const char* s)
{
    if(tea_load_buffer(T, s, strlen(s), "=<stdin>") != TEA_OK)
    {
        return TEA_ERROR_SYNTAX;
    }
    int status;
    signal(SIGINT, tsignal);
    status = tea_pcall(T, 0);
    signal(SIGINT, SIG_DFL);
    return status;
}

#if defined(TEA_USE_READLINE)
#include <readline/readline.h>
#include <readline/history.h>
#define t_initreadline(T) ((void)T, rl_readline_name = "tea")
#define t_readline(T, b, p) ((void)T, ((b) = readline(p)) != NULL)
#define t_saveline(T, line) ((void)T, add_history(line))
#define t_freeline(T, b) ((void)T, free(b))
#else
#define t_initreadline(T) ((void)T)
#define t_readline(T, b, p) \
    ((void)T, fputs(p, stdout), fflush(stdout), /* Show prompt */ \
    fgets(b, MAX_INPUT, stdin) != NULL)     /* Get line */
#define t_saveline(T, line) { (void)T; (void)line; }
#define t_freeline(T, b) { (void)T; (void)b; }
#endif

static const char* get_prompt(bool firstline)
{
    return firstline ? PROMPT1 : PROMPT2;
}

static bool multiline(const char* line)
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
            return true; /* Closed brace before opening, end line now */
        }
    }

    return level == 0;
}

static void repl(tea_State* T)
{
    global = T;
    tea_set_repl(T, true);

    t_initreadline(T);
    const char* line;

    while(true)
    {
        line:
        tea_push_literal(T, "");

        const char* prompt = get_prompt(true);

        char buffer[MAX_INPUT];
        char* b = buffer;
        while(t_readline(T, b, prompt))
        {
            tea_push_string(T, b);
            tea_concat(T);
            line = tea_get_string(T, -1);

            t_saveline(T, line);

            if(!multiline(line))
            {
                prompt = get_prompt(false);
                continue;
            }

            if(strlen(b) != MAX_INPUT - 1 || b[MAX_INPUT - 2] == '\n')
            {
                t_freeline(T, b);
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

        interpret(T, line);
        tea_pop(T, 2);  /* Result of interpret + input string */
    }

    tea_set_top(T, 0);
    putchar('\n');
}

static int report_status(int status, const char* path)
{
    switch(status)
    {
        case TEA_ERROR_SYNTAX:
            return 65;
        case TEA_ERROR_RUNTIME:
            return 70;
        case TEA_ERROR_FILE:
        {
            fputs("tea: ", stderr);
            fprintf(stderr, "Cannot open '%s': No such file or directory", path);
            fputc('\n', stderr);
            fflush(stderr);
            return 75;
        }
        default:
            break;
    }
    return status;
}

static int do_file(tea_State* T, const char* name)
{
    int status = tea_load_file(T, name, NULL);
    if(status != TEA_OK)
    {
        tea_pop(T, 1);
        return report_status(status, (char*)name);
    }

    status = tea_pcall(T, 0);
    tea_pop(T, 1);

    return report_status(status, (char*)name);
}

static int handle_script(tea_State* T, char** argx, const char* name)
{
    int status;
    const char* path = argx[0];
    if(strcmp(path, "-") == 0 && strcmp(argx[-1], "--") != 0)
        path = NULL; /* stdin */
    
    status = tea_load_file(T, path, name);

    if(status != TEA_OK)
    {
        tea_pop(T, 1);
        return report_status(status, path);
    }

    status = tea_pcall(T, 0);
    tea_pop(T, 1);

    return report_status(status, path);
}

/* Check that argument has no extra characters at the end */
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

static int run_args(tea_State* T, char** argv, int n)
{
    int i;
    for(i = 1; i < n; i++)
    {
        if(argv[i] == NULL)
            continue;
        switch(argv[i][1])
        {
            case 'e':
            {
                const char* code = argv[i] + 2;
                if(*code == '\0')
                    code = argv[++i];

                int status = interpret(T, code);
                tea_pop(T, 1);

                return report_status(status, NULL);
            }
        }
    }
    return 0;
}

int main(int argc, char** argv)
{
    if(!argv[0])
        argv = empty_argv;
    tea_State* T = tea_open();
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

    status = run_args(T, argv, script);
    if(status != TEA_OK)
        goto finish;
    
    if(argc > script)
    {
        status = handle_script(T, argv + script, (flags & FLAG_I) ? "=<stdin>" : NULL);
        if(status != TEA_OK)
            goto finish;
    }

    if((flags & FLAG_I))
    {
        repl(T);
    }
    else if(argc == script && !(flags & (FLAG_E | FLAG_V)))
    {
        if(stdin_is_tty())
        {
            print_version();
            repl(T);
        }
        else
        {
            do_file(T, NULL); /* Executes stdin as a file */
        }
    }

finish:
    tea_close(T);

    return status;
}