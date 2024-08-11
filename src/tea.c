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

#define TEA_PROMPT1  "> "
#define TEA_PROMPT2  "... "
#define TEA_MAX_INPUT 512

static tea_State* globalT = NULL;
static char* empty_argv[2] = { NULL, NULL };

static void taction(int id)
{
    signal(id, SIG_DFL);
    tea_error(globalT, "Interrupted");
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
    fputs(msg, stderr);
    fputc('\n', stderr);
    fflush(stderr);
}

static int report(tea_State* T, int status)
{
    if(status && !tea_is_none(T, -1))
    {
        const char* msg = tea_to_string(T, -1);
        t_message(msg);
        tea_pop(T, 2);  /* error + tostring result */
    }
    switch(status)
    {
        case TEA_ERROR_SYNTAX:
            return 65;
        case TEA_ERROR_RUNTIME:
            return 70;
        case TEA_ERROR_FILE:
            return 75;
        default:
            return EXIT_SUCCESS;
    }
}

static int docall(tea_State* T, int narg)
{
    int status;
    signal(SIGINT, taction);
    status = tea_pcall(T, 0);
    signal(SIGINT, SIG_DFL);
    /* Force a complete garbage collection in case of errors */
    if(status != TEA_OK) tea_gc(T);
    return status;
}

static int dofile(tea_State* T, const char* name)
{
    int status = tea_load_file(T, name, NULL) || docall(T, 0);
    return report(T, status);
}

static int dostring(tea_State* T, const char* s, const char* name)
{
    int status = tea_load_buffer(T, s, strlen(s), name) || docall(T, 0);
    return report(T, status);
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
    fgets(b, TEA_MAX_INPUT, stdin) != NULL)     /* Get line */
#define t_saveline(T, line) { (void)T; (void)line; }
#define t_freeline(T, b) { (void)T; (void)b; }
#endif

static const char* get_prompt(bool firstline)
{
    return firstline ? TEA_PROMPT1 : TEA_PROMPT2;
}

static bool incomplete(tea_State* T, int status)
{
    if(status == TEA_ERROR_SYNTAX)
    {
        size_t lmsg;
        const char* msg = tea_to_lstring(T, -1, &lmsg);
        const char* tp = msg + lmsg - (sizeof(TEA_QL("<eof>")) - 1);
        if(strstr(msg, TEA_QL("<eof>")) == tp)
        {
            tea_pop(T, 2);
            return true;
        }
        tea_pop(T, 1);
    }
    return false;  /* Else... */
}

static bool pushline(tea_State* T, bool firstline)
{
    char buffer[TEA_MAX_INPUT];
    char* b = buffer;
    size_t len;
line:
    const char* prompt = get_prompt(firstline);
    if(!t_readline(T, b, prompt))
        return false;   /* No input */
    len = strlen(b);
    if(len > 0 && b[len - 1] == '\n')   /* Line ends with newline? */
        b[len - 1] = '\0';    /* Remove it */
    if(strcmp(b, "exit") == 0)
        return false;
    if(strcmp(b, "clear") == 0)
    {
#if TEA_TARGET_POSIX
        system("clear");
#elif TEA_TARGET_WINDOWS
        system("cls");
#endif
        goto line;
    }
    tea_push_string(T, b);
    t_freeline(T, b);
    return true;
}

static int loadline(tea_State* T)
{
    int status;
    const char* line;
    size_t len;
    tea_set_top(T, 0);
    if(!pushline(T, true))
        return -1;  /* No input */
    /* Repeat until gets a complete line */
    while(true)
    {
        line = tea_get_lstring(T, -1, &len);
        status = tea_load_buffer(T, line, len, "=<stdin>");
        if(!incomplete(T, status))
            break;  /* Cannot try to add lines? */
        if(!pushline(T, false))
            return -1;  /* No more input? */
        tea_push_literal(T, "\n");  /* Add new line... */
        tea_insert(T, -2);  /* ...between the two lines */
        tea_concat(T, 3);   /* Join them */
    }
    t_saveline(T, line);
    tea_remove(T, 0);   /* Remove line */
    return status;
}

static void dotty(tea_State* T)
{
    t_initreadline(T);
    int status;
    while((status = loadline(T)) != -1)
    {
        if(status == TEA_OK)
            status = docall(T, 0);
        report(T, status);
    }
    tea_set_top(T, 0);  /* Clear stack */
    fputs("\n", stdout);
    fflush(stdout);
}

static int handle_script(tea_State* T, char** argx, const char* name)
{
    int status;
    const char* path = argx[0];
    if(strcmp(path, "-") == 0 && strcmp(argx[-1], "--") != 0)
        path = NULL; /* stdin */

    status = tea_load_file(T, path, name);
    if(status == TEA_OK)
    {
        status = docall(T, 0);
    }
    return report(T, status);
}

/* Check that argument has no extra characters at the end */
#define notail(x)   { if((x)[2] != '\0') return -1; }

#define FLAG_INTERACTIVE 1
#define FLAG_VERSION 2
#define FLAG_EXEC 4

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
                *flags |= FLAG_INTERACTIVE;
            case 'v':
                notail(argv[i])
                *flags |= FLAG_VERSION;
                break;
            case 'e':
                *flags |= FLAG_EXEC;
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
                int status = dostring(T, code, "=<stdin>");
                if(status)
                    return status;
                break;
            }
        }
    }
    return 0;
}

static struct Smain
{
    char** argv;
    int argc;
    int status;
} smain;

static void pmain(tea_State* T)
{
    struct Smain* s = &smain;
    char** argv = s->argv;
    int argc = s->argc;
    globalT = T;
    
    int flags = 0;
    int script = collect_args(argv, &flags);
    if(script < 0)
    {
        print_usage();
        return;
    }

    tea_set_argv(T, argc, argv, script);

    if(flags & FLAG_VERSION)
        print_version();

    s->status = run_args(T, argv, script);
    if(s->status != TEA_OK)
        return;
    
    if(argc > script)
    {
        const char* name = (flags & FLAG_INTERACTIVE) ? "=<stdin>" : NULL;
        s->status = handle_script(T, argv + script, name);
        if(s->status != TEA_OK)
            return;
    }

    if((flags & FLAG_INTERACTIVE))
    {
        dotty(T);
    }
    else if(argc == script && !(flags & (FLAG_EXEC | FLAG_VERSION)))
    {
        if(stdin_is_tty())
        {
            print_version();
            dotty(T);
        }
        else
        {
            dofile(T, NULL); /* Executes stdin as a file */
        }
    }
}

int main(int argc, char** argv)
{
    int status;
    tea_State* T;
    if(!argv[0]) argv = empty_argv;
    T = tea_open();
    if(T == NULL)
    {
        t_message("Cannot create state: not enough memory");
        return EXIT_FAILURE;
    }
    smain.argc = argc;
    smain.argv = argv;
    status = tea_pccall(T, pmain, NULL);
    report(T, status);
    tea_close(T);
    return smain.status;
}