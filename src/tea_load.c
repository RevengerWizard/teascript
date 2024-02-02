/*
** tea_load.c
** Load and dump code from strings and files
*/

#define tea_load_c
#define TEA_CORE

#include "tea.h"

#include "tea_buf.h"
#include "tea_import.h"
#include "tea_obj.h"
#include "tea_func.h"
#include "tea_str.h"
#include "tea_err.h"
#include "tea_vm.h"
#include "tea_bcdump.h"

static void parser_f(tea_State* T, void* ud)
{
    Lexer* lex = (Lexer*)ud;

    bool bc = tea_lex_init(T, lex);
    if(lex->mode && !strchr(lex->mode, bc ? 'b' : 't'))
    {
        tea_err_throw(T, TEA_ERROR_SYNTAX);
    }

    GCproto* proto = bc ? tea_bcread(lex) : tea_parse(lex);
    GCfuncT* func = tea_func_newT(T, proto);
    tea_vm_push(T, OBJECT_VAL(func));
}

TEA_API int tea_loadx(tea_State* T, tea_Reader reader, void* data, const char* name, const char* mode)
{
    GCstr* mname = tea_str_new(T, name);
    tea_vm_push(T, OBJECT_VAL(mname));
    GCmodule* module = tea_obj_new_module(T, mname);
    tea_vm_pop(T, 1);
    if(T->last_module != NULL && T->last_module->name == module->name)
    {
        /* Already found the path */
    }
    else
    {
        char c = name[0];
        tea_vm_push(T, OBJECT_VAL(module));
        if(c != '<' && c != '?' && c != '=')
        {
            module->path = tea_imp_getdir(T, (char*)name);
        }
        else
        {
            module->path = tea_str_lit(T, ".");
        }
        tea_vm_pop(T, 1);
    }

    Lexer lex;
    lex.module = module;
    lex.reader = reader;
    lex.data = data;
    lex.mode = mode;

    tea_buf_init(&lex.sbuf);
    int status = tea_vm_pcall(T, parser_f, &lex, stacksave(T, T->top));
    tea_buf_free(T, &lex.sbuf);
    return status;
}

typedef struct FileReaderCtx
{
    FILE* f;
    char buf[TEA_BUFFER_SIZE];
} FileReaderCtx;

static const char* reader_file(tea_State* T, void* ud, size_t* size)
{
    FileReaderCtx* ctx = (FileReaderCtx*)ud;
    UNUSED(T);
    if(feof(ctx->f))
        return NULL;
    *size = fread(ctx->buf, 1, sizeof(ctx->buf), ctx->f);
    return (*size > 0) ? ctx->buf : NULL;
}

TEA_API int tea_load_filex(tea_State* T, const char* filename, const char* mode)
{
    FileReaderCtx ctx;
    const char* name;
    if(filename)
    {
        ctx.f = fopen(filename, "rb");
        if(ctx.f == NULL)
            return TEA_ERROR_FILE;
        name = filename;
    }
    else
    {
        ctx.f = stdin;
        name = "=<stdin>";
    }
    int status = tea_loadx(T, reader_file, &ctx, name, mode);
    if(ferror(ctx.f))
    {
        if(filename)
            fclose(ctx.f);
        return TEA_ERROR_FILE;
    }
    if(filename)
    {
        fclose(ctx.f);
    }
    return status;
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

TEA_API int tea_load_string(tea_State* T, const char* s)
{
    return tea_load_buffer(T, s, strlen(s), "?<string>");
}

TEA_API int tea_dump(tea_State* T, tea_Writer writer, void* data)
{
    Value o = tea_vm_peek(T, 0);
    if(IS_FUNC(o))
    {
        return tea_bcwrite(T, AS_FUNC(o)->proto, writer, data);
    }
    else
    {
        return TEA_ERROR_SYNTAX;
    }
}