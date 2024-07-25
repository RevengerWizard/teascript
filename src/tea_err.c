/*
** tea_err.c
** Error handling
*/

#include <stdlib.h>

#define tea_err_c
#define TEA_CORE

#include "tea_def.h"
#include "tea_err.h"
#include "tea_vm.h"
#include "tea_func.h"
#include "tea_strfmt.h"

struct tea_longjmp
{
    struct tea_longjmp* prev;
    jmp_buf buf;
    volatile int status;
};

/* -- Error messages -------------------------------------------------- */

/* Error message strings */
TEA_DATADEF const char* tea_err_allmsg =
#define ERRDEF(name, msg) msg "\0"
#include "tea_errmsg.h"
;

/* Throw error */
TEA_NOINLINE void tea_err_throw(tea_State* T, int code)
{
    if(T->error_jump)
    {
        T->error_jump->status = code;
        err_throw(T);
    }
    else
    {
        T->panic(T);
        exit(EXIT_FAILURE);
    }
}

/* Catch error and add to the error chain */
int tea_err_protected(tea_State* T, tea_CPFunction f, void* ud)
{
    struct tea_longjmp tj;
    tj.status = TEA_OK;
    tj.prev = T->error_jump;    /* Chain new error handler */
    T->error_jump = &tj;
    err_try(T, &tj,
        (*f)(T, ud);
    );
    T->error_jump = tj.prev;    /* Restore old error handler */
    return tj.status;
}

/* -- Error handling ------------------------------------------------------ */

TEA_NORET TEA_NOINLINE static void err_run(tea_State* T)
{
    for(CallInfo* ci = T->ci; ci > T->ci_base; ci--)
    {
        /* Skip stack trace for C functions */
        if(iscfunc(ci->func)) continue;

        GCproto* proto = ci->func->t.proto;
        size_t instruction = ci->ip - proto->bc - 1;
        fprintf(stderr, "[line %d] in ", tea_func_getline(proto, instruction));
        fprintf(stderr, "%s\n", str_data(proto->name));
    }

    tea_err_throw(T, TEA_ERROR_RUNTIME);
}

/* Out-of-memory error */
TEA_NOINLINE void tea_err_mem(tea_State* T)
{
    tea_err_throw(T, TEA_ERROR_MEMORY);
}

/* Runtime error */
TEA_NOINLINE void tea_err_run(tea_State* T, ErrMsg em, ...)
{
    va_list argp;
    va_start(argp, em);
    vfprintf(stderr, err2msg(em), argp);
    va_end(argp);
    fputc('\n', stderr);
    err_run(T);
}

/* Non-vararg variant for better calling conventions */
TEA_NOINLINE void tea_err_msg(tea_State* T, ErrMsg em)
{
    tea_err_run(T, em);
}

/* Lexer error */
TEA_NOINLINE void tea_err_lex(tea_State* T, const char* src, const char* tok, int line, ErrMsg em, va_list argp)
{
    fprintf(stderr, "File %s, [line %d] Error", src, line);

    if(tok != NULL)
    {
        fprintf(stderr, " at " TEA_QS ": ", tok);
    }
    else
    {
        fputs(": ", stderr);
    }

    vfprintf(stderr, err2msg(em), argp);
    fputc('\n', stderr);

    tea_err_throw(T, TEA_ERROR_SYNTAX);
}

/* Argument error message */
TEA_NORET TEA_NOINLINE static void err_argmsg(tea_State* T, int narg, const char* msg)
{
    if(narg < 0 && narg > TEA_REGISTRY_INDEX)
        narg = (int)(T->top - T->base) + narg;
    msg = tea_strfmt_pushf(T, err2msg(TEA_ERR_BADARG), narg, msg);
    fputs(msg, stderr);
    fputc('\n', stderr);
    err_run(T);
}

/* Argument error */
TEA_NOINLINE void tea_err_arg(tea_State* T, int narg, ErrMsg em)
{
    fputs(err2msg(em), stderr);
    fputc('\n', stderr);
    err_run(T);
}

/* Typecheck error for arguments */
TEA_NOINLINE void tea_err_argtype(tea_State* T, int narg, const char* xname)
{
    const char* tname;
    TValue* o = narg < 0 ? T->top + narg : T->base + narg;
    tname = o < T->top ? tea_typename(o) : "no value";
    fprintf(stderr, err2msg(TEA_ERR_BADTYPE), xname, tname);
    fputc('\n', stderr);
    err_run(T);
}

/* Typecheck error for arguments */
TEA_NOINLINE void tea_err_argt(tea_State* T, int narg, int tt)
{
    tea_err_argtype(T, narg, tea_obj_typenames[tt - 1]);
}

/* -- Public error handling API -------------------------------------------------- */

TEA_API tea_CFunction tea_atpanic(tea_State* T, tea_CFunction panicf)
{
    tea_CFunction old = T->panic;
    T->panic = panicf;
    return old;
}

TEA_API int tea_error(tea_State* T, const char* fmt, ...)
{
    const char* msg;
    va_list argp;
    va_start(argp, fmt);
    msg = tea_strfmt_pushvf(T, fmt, argp);
    va_end(argp);
    
    fputs(msg, stderr);
    fputc('\n', stderr);
    err_run(T);
    return 0;   /* Unreachable */
}

TEA_API int tea_arglimit_error(tea_State* T, int narg, const char* msg)
{
    int limit = (int)(T->top - T->base);
    if(narg > limit)
        tea_err_run(T, TEA_ERR_ARGS, limit, narg);
    return 0;   /* Unreachable */
}

TEA_API int tea_arg_error(tea_State* T, int narg, const char* msg)
{
    err_argmsg(T, narg, msg);
    return 0;   /* Unreachable */
}

TEA_API int tea_type_error(tea_State* T, int narg, const char* xname)
{
    tea_err_argtype(T, narg, xname);
    return 0;   /* Unreachable */
}