/*
** tea_err.c
** Error handling
*/

#include <stdlib.h>

#define tea_err_c
#define TEA_CORE

#include "tea_def.h"
#include "tea_err.h"
#include "tea_buf.h"
#include "tea_str.h"
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
        if(T->panic)
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

/* Protected call */
int tea_vm_pcall(tea_State* T, tea_CPFunction func, void* u, ptrdiff_t old_top)
{
    int oldnccalls = T->nccalls;
    ptrdiff_t old_ci = ci_save(T, T->ci);
    ptrdiff_t old_base = stack_save(T, T->base);
    int status = tea_err_protected(T, func, u);
    if(status != TEA_OK)    /* An error occurred? */
    {
        TValue* old = stack_restore(T, old_top);
        tea_func_closeuv(T, old);   /* Close eventual pending closures */
        copyTV(T, old, T->top - 1);
        T->top = old + 1;
        T->nccalls = oldnccalls;
        T->ci = ci_restore(T, old_ci);
        T->base = stack_restore(T, old_base);
        /* Correct the stack */
        T->stack_max = T->stack + T->stack_size - 1;
        if(T->ci_size > TEA_MAX_CALLS)
        {
            int inuse = T->ci - T->ci_base;
            if(inuse + 1 < TEA_MAX_CALLS)
            {
                tea_state_reallocci(T, TEA_MAX_CALLS);
            }
        }
    }
    return status;
}

/* Return string object for error message */
TEA_NOINLINE GCstr* tea_err_str(tea_State* T, ErrMsg em)
{
    return tea_str_newlen(T, err2msg(em));
}

/* Stack overflow error */
void tea_err_stkov(tea_State* T)
{
    setstrV(T, T->top++, tea_err_str(T, TEA_ERR_STKOV));
    tea_err_run(T);
}

/* Out-of-memory error */
TEA_NOINLINE void tea_err_mem(tea_State* T)
{
    setstrV(T, T->top++, tea_err_str(T, TEA_ERR_MEM));
    tea_err_throw(T, TEA_ERROR_MEMORY);
}

/* Runtime error */
TEA_NOINLINE void tea_err_run(tea_State* T)
{
    SBuf* sb = &T->strbuf;
    tea_buf_reset(sb);
    GCstr* msg = strV(T->top - 1);
    tea_buf_putmem(T, sb, str_data(msg), msg->len);
    tea_buf_putlit(T, sb, "\n");
    /* Stack trace */
    for(CallInfo* ci = T->ci; ci > T->ci_base; ci--)
    {
        /* Skip stack trace for C functions */
        if(iscfunc(ci->func)) continue;

        GCproto* proto = ci->func->t.proto;
        size_t instruction = ci->ip - proto->bc - 1;
        tea_strfmt_pushf(T, "[line %d] in %s\n", 
            tea_func_getline(proto, instruction), str_data(proto->name));
        msg = strV(T->top - 1);
        tea_buf_putmem(T, sb, str_data(msg), msg->len);
        T->top--;
    }
    sb->w--;
    setstrV(T, T->top, tea_buf_str(T, sb));
    incr_top(T);
    tea_err_throw(T, TEA_ERROR_RUNTIME);
}

TEA_NORET TEA_NOINLINE static void err_msgv(tea_State* T, ErrMsg em, ...)
{
    va_list argp;
    va_start(argp, em);
    tea_strfmt_pushvf(T, err2msg(em), argp);
    va_end(argp);
    tea_err_run(T);
}

/* Non-vararg variant for better calling conventions */
TEA_NOINLINE void tea_err_msg(tea_State* T, ErrMsg em)
{
    err_msgv(T, em);
}

/* Lexer error */
TEA_NOINLINE void tea_err_lex(tea_State* T, const char* src, const char* tok, int line, ErrMsg em, va_list argp)
{
    const char* msg;
    msg = tea_strfmt_pushvf(T, err2msg(em), argp);
    msg = tea_strfmt_pushf(T, "File %s, [line %d]: %s", src, line, msg);
    if(tok)
        tea_strfmt_pushf(T, err2msg(TEA_ERR_XNEAR), msg, tok);
    tea_err_throw(T, TEA_ERROR_SYNTAX);
}

/* Typecheck error for binary operands */
TEA_NOINLINE void tea_err_bioptype(tea_State* T, cTValue* o1, cTValue* o2, MMS mm)
{
    const char* opname = str_data(mmname_str(T, mm));
    const char* t1 = tea_typename(o1);
    const char* t2 = tea_typename(o2);
    err_msgv(T, TEA_ERR_BIOP, opname, t1, t2);
}

/* Typecheck error for unary operands */
TEA_NOINLINE void tea_err_unoptype(tea_State* T, cTValue* o, MMS mm)
{
    const char* opname = str_data(mmname_str(T, mm));
    const char* tname = tea_typename(o);
    err_msgv(T, TEA_ERR_UNOP, opname, tname);
}

/* Error in context of caller */
TEA_NOINLINE void tea_err_callermsg(tea_State* T, const char* msg)
{
    tea_strfmt_pushf(T, msg);
    tea_err_run(T);
}

/* Formatted error in context of caller */
TEA_NOINLINE void tea_err_callerv(tea_State* T, ErrMsg em, ...)
{
    const char* msg;
    va_list argp;
    va_start(argp, em);
    msg = tea_strfmt_pushvf(T, err2msg(em), argp);
    va_end(argp);
    tea_err_callermsg(T, msg);
}

/* Error in context of caller */
TEA_NOINLINE void tea_err_caller(tea_State* T, ErrMsg em)
{
    tea_err_callermsg(T, err2msg(em));
}

/* Argument error message */
TEA_NORET TEA_NOINLINE static void err_argmsg(tea_State* T, int narg, const char* msg)
{
    if(narg < 0 && narg > TEA_REGISTRY_INDEX)
        narg = (int)(T->top - T->base) + narg;
    msg = tea_strfmt_pushf(T, err2msg(TEA_ERR_BADARG), narg + 1, msg);
    tea_err_callermsg(T, msg);
}

/* Argument error */
TEA_NOINLINE void tea_err_arg(tea_State* T, int narg, ErrMsg em)
{
    err_argmsg(T, narg, err2msg(em));
}

/* Typecheck error for arguments */
TEA_NOINLINE void tea_err_argtype(tea_State* T, int narg, const char* xname)
{
    const char* tname, *msg;
    TValue* o = narg < 0 ? T->top + narg : T->base + narg;
    tname = o < T->top ? tea_typename(o) : "no value";
    msg = tea_strfmt_pushf(T, err2msg(TEA_ERR_BADTYPE), xname, tname);
    err_argmsg(T, narg, msg);
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
    tea_err_callermsg(T, msg);
    return 0;   /* Unreachable */
}

TEA_API int tea_arglimit_error(tea_State* T, int narg, const char* msg)
{
    int limit = (int)(T->top - T->base);
    if(narg > limit)
        err_msgv(T, TEA_ERR_ARGS, limit, narg);
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