/*
** tea_load.c
** Load and dump code from strings and files
*/

#include <errno.h>

#define tea_load_c
#define TEA_CORE

#include "tea.h"

#include "tea_buf.h"
#include "tea_tab.h"
#include "tea_import.h"
#include "tea_obj.h"
#include "tea_func.h"
#include "tea_str.h"
#include "tea_err.h"
#include "tea_bcdump.h"
#include "tea_parse.h"
#include "tea_vm.h"

/* -- Load Teascript source code and bytecode -------------------------------------------------- */

static void parser_f(tea_State* T, void* ud)
{
    Lexer* lex = (Lexer*)ud;
    bool bc = tea_lex_setup(T, lex);
    if(lex->mode && !strchr(lex->mode, bc ? 'b' : 't'))
    {
        setstrV(T, T->top++, tea_err_str(T, TEA_ERR_XMODE));
        tea_err_throw(T, TEA_ERROR_SYNTAX);
    }
    GCproto* proto = bc ? tea_bcread(lex) : tea_parse(lex, lex->eval);
    GCfunc* func = tea_func_newT(T, proto, lex->module);
    setfuncV(T, T->top++, func);
}

TEA_API int tea_loadx(tea_State* T, tea_Reader reader, void* data, const char* name, const char* mode)
{
    GCstr* mname = tea_str_newlen(T, name);
    setstrV(T, T->top++, mname);
    GCmodule* module = tea_module_new(T, mname);
    tea_tab_merge(T, &T->globals, &module->vars);
    T->top--;
    if(T->last_module != NULL && T->last_module->name == module->name)
    {
        /* Already found the path */
    }
    else
    {
        char c = name[0];
        setmoduleV(T, T->top++, module);
        if(c != '<' && c != '?' && c != '=')
        {
            module->path = tea_imp_getdir(T, (char*)name);
        }
        else
        {
            module->path = tea_str_newlit(T, ".");
        }
        T->top--;
    }

    Lexer lex;
    lex.eval = false;
    lex.module = module;
    lex.reader = reader;
    lex.data = data;
    lex.mode = mode;
    tea_buf_init(&lex.sb);
    int status = tea_vm_pcall(T, parser_f, &lex, stack_save(T, T->top));
    tea_lex_cleanup(T, &lex);
    return status;
}

TEA_API int tea_load(tea_State* T, tea_Reader reader, void* data, const char* name)
{
    return tea_loadx(T, reader, data, name, NULL);
}

typedef struct FileReaderCtx
{
    FILE* fp;
    char buf[TEA_BUFFER_SIZE];
} FileReaderCtx;

static const char* reader_file(tea_State* T, void* ud, size_t* size)
{
    FileReaderCtx* ctx = (FileReaderCtx*)ud;
    UNUSED(T);
    if(feof(ctx->fp))
        return NULL;
    *size = fread(ctx->buf, 1, sizeof(ctx->buf), ctx->fp);
    return (*size > 0) ? ctx->buf : NULL;
}

TEA_API int tea_load_filex(tea_State* T, const char* filename, const char* name, const char* mode)
{
    FileReaderCtx ctx;
    int status;
    if(filename)
    {
        ctx.fp = fopen(filename, "rb");
        if(ctx.fp == NULL)
        {
            tea_push_fstring(T, "Cannot open %s: %s", filename, strerror(errno));
            return TEA_ERROR_FILE;
        }
        if(!name)
            name = filename;
    }
    else
    {
        ctx.fp = stdin;
        name = "=<stdin>";
    }
    status = tea_loadx(T, reader_file, &ctx, name, mode);
    if(ferror(ctx.fp))
    {
        tea_push_fstring(T, "Cannot read %s: %s", name, strerror(errno));
        if(filename)
            fclose(ctx.fp);
        return TEA_ERROR_FILE;
    }
    if(filename)
    {
        fclose(ctx.fp);
    }
    return status;
}

TEA_API int tea_load_file(tea_State* T, const char* filename, const char* name)
{
    return tea_load_filex(T, filename, name, NULL);
}

typedef struct StringReaderCtx
{
    const char* s;
    size_t size;
} StringReaderCtx;

static const char* reader_string(tea_State* T, void* ud, size_t* size)
{
    StringReaderCtx* ctx = (StringReaderCtx*)ud;
    UNUSED(T);
    if(ctx->size == 0)
        return NULL;
    *size = ctx->size;
    ctx->size = 0;
    return ctx->s;
}

TEA_API int tea_load_bufferx(tea_State* T, const char* buffer, size_t size, const char* name, const char* mode)
{
    StringReaderCtx ctx;
    ctx.s = buffer;
    ctx.size = size;
    return tea_loadx(T, reader_string, &ctx, name, mode);
}

TEA_API int tea_load_buffer(tea_State* T, const char* buffer, size_t size, const char* name)
{
    return tea_load_bufferx(T, buffer, size, name, NULL);
}

TEA_API int tea_eval(tea_State* T, const char* s)
{
    StringReaderCtx ctx;
    ctx.s = s;
    ctx.size = strlen(s);

    GCmodule* module = tea_module_new(T, tea_str_newlit(T, "?<eval>"));
    tea_tab_merge(T, &T->globals, &module->vars);
    module->path = tea_str_newlit(T, ".");

    Lexer lex;
    lex.eval = true;
    lex.module = module;
    lex.reader = reader_string;
    lex.data = &ctx;
    lex.mode = "t";
    tea_buf_init(&lex.sb);
    int status = tea_vm_pcall(T, parser_f, &lex, stack_save(T, T->top));
    tea_lex_cleanup(T, &lex);
    return status;
}

/* -- Dump bytecode -------------------------------------------------- */

TEA_API int tea_dump(tea_State* T, tea_Writer writer, void* data)
{
    cTValue* o = T->top - 1;
    tea_checkapi(T->top > T->base, "top slot empty");
    if(tvisfunc(o) && isteafunc(funcV(o)))
        return tea_bcwrite(T, funcV(o)->t.proto, writer, data);
    else
        return TEA_ERROR_SYNTAX;
}