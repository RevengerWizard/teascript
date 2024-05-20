/*
** lib_io.c
** Teascript io module
*/

#include <stdio.h>

#define lib_io_c
#define TEA_LIB

#include "tea.h"
#include "tealib.h"

#include "tea_arch.h"
#include "tea_str.h"
#include "tea_import.h"
#include "tea_vm.h"
#include "tea_gc.h"
#include "tea_buf.h"
#include "tea_lib.h"
#include "tea_strfmt.h"
#include "tea_udata.h"

/* Userdata payload for I/O file */
typedef struct IOFileUD
{
    FILE* fp;   /* File handle */
    uint32_t type;  /* File type */
} IOFileUD;

#define IOFILE_TYPE_FILE 0  /* Regular file */
#define IOFILE_TYPE_PIPE 1  /* Pipe */
#define IOFILE_TYPE_STDF 2  /* Standard file handle */
#define IOFILE_TYPE_MASK 3

#define IOFILE_TYPE_CLOSE 4

/* -- Open/close helpers -------------------------------------------------- */

static void io_file_free(void* p)
{
    IOFileUD* iof = (IOFileUD*)p;
    if(iof->fp != NULL && (iof->type & IOFILE_TYPE_MASK) != IOFILE_TYPE_STDF)
    {
        fclose(iof->fp);
    }
}

static IOFileUD* io_get_filep(tea_State* T)
{
    tea_check_type(T, 0, TEA_TYPE_INSTANCE);
    tea_get_attr(T, 0, "FILE*");
    IOFileUD* iof = (IOFileUD*)tea_get_userdata(T, -1);
    T->top--;
    return iof;
}

static IOFileUD* io_get_file(tea_State* T)
{
    IOFileUD* iof = io_get_filep(T);
    if(iof->type & IOFILE_TYPE_CLOSE)
    {
        tea_error(T, "Attempt to use a closed file");
    }
    return iof;
}

static void io_file_new(tea_State* T, FILE* fp, uint32_t type)
{
    tea_push_value(T, tea_upvalue_index(0));
    GCclass* klass = classV(T->top - 1);
    GCinstance* instance = tea_instance_new(T, klass);
    setinstanceV(T, T->top++, instance);
    IOFileUD* iof = (IOFileUD*)tea_new_userdata(T, sizeof(IOFileUD));
    tea_set_finalizer(T, io_file_free);
    iof->fp = fp;
    iof->type = type;
    tea_set_attr(T, -2, "FILE*");
}

/* -- Read/write helpers -------------------------------------------------- */

static void io_file_write(tea_State* T, FILE* fp, int start)
{
    cTValue* tv;
    int status = 1;
    for(tv = T->base + start; tv < T->top; tv++)
    {
        int len;
        const char* p = tea_strfmt_wstrnum(T, tv, &len);
        if(!p)
            tea_err_argt(T, (int)(tv - T->base), TEA_TYPE_STRING);
        status = status && (fwrite(p, 1, len, fp) == len);
    }
    if(!status)
        tea_lib_fileresult(T, NULL);
}

static void io_file_readall(tea_State* T, FILE* fp)
{
    size_t m, n;
    for(m = TEA_BUFFER_SIZE, n = 0;; m += m)
    {
        char* buf = tea_buf_tmp(T, m);
        n += fread(buf + n, 1, m - n, fp);
        if(n != m)
        {
            setstrV(T, T->top++, tea_str_new(T, buf, n));
            return;
        }
    }
}

static int io_file_readlen(tea_State* T, FILE* fp, size_t m)
{
    if(m)
    {
        char* buf = tea_buf_tmp(T, m);
        size_t n = fread(buf, 1, m, fp);
        setstrV(T, T->top++, tea_str_new(T, buf, (size_t)n));
        return n > 0;
    }
    else
    {
        int c = getc(fp);
        ungetc(c, fp);
        setstrV(T, T->top++, &T->strempty);
        return (c != EOF);
    }
}

static int io_file_readline(tea_State* T, FILE* fp)
{
    size_t m = TEA_BUFFER_SIZE, n = 0, ok = 0;
    char* buf;
    while(true)
    {
        buf = tea_buf_tmp(T, m);
        if(fgets(buf + n, m - n, fp) == NULL)
            break;
        n += strlen(buf + n);
        ok |= n;
        if(n && buf[n - 1] == '\n') { n -= 1; break; }
        if(n >= m - 64) m += m;
    }
    setstrV(T, T->top++, tea_str_new(T, buf, n));
    return (int)ok;
}

/* -- I/O file methods ---------------------------------------------------- */

static void file_write(tea_State* T)
{
    IOFileUD* iof = io_get_file(T);
    io_file_write(T, iof->fp, 1);
    tea_push_nil(T);
}

static void file_read(tea_State* T)
{
    int count = tea_get_top(T);
    tea_check_args(T, count > 2, "Expected 0 or 1 arguments, got %d", count);

    IOFileUD* iof = io_get_file(T);

    if(count == 2)
        io_file_readlen(T, iof->fp, tea_check_integer(T, 1));
    else
        io_file_readall(T, iof->fp);
}

static void file_readline(tea_State* T)
{
    IOFileUD* iof = io_get_file(T);
    int ok = io_file_readline(T, iof->fp);
    if(!ok)
        setnilV(T->top - 1);
}

static void file_seek(tea_State* T)
{
    int count = tea_get_top(T);
    tea_check_args(T, count < 2 || count > 3, "Expected 1 or 2 arguments, got %d", count);

    static const int mode[] = { SEEK_SET, SEEK_CUR, SEEK_END };
    static const char* const modenames[] = { "set", "cur", "end", NULL };
    IOFileUD* iof = io_get_file(T);
    int opt = tea_check_option(T, 2, "cur", modenames);

    int offset = tea_check_number(T, 1);

    if(fseek(iof->fp, offset, mode[opt]))
    {
        tea_error(T, "Unable to seek file");
    }

    tea_push_nil(T);
}

static void file_flush(tea_State* T)
{
    IOFileUD* iof = io_get_file(T);
    fflush(iof->fp);
    tea_push_nil(T);
}

static void file_setvbuf(tea_State* T)
{
    static const int mode[] = { _IONBF, _IOFBF, _IOLBF };
    static const char* const modenames[] = { "no", "full", "line", NULL };
    IOFileUD* iof = io_get_file(T);
    int opt = tea_check_option(T, 1, NULL, modenames);
    setvbuf(iof->fp, NULL, mode[opt], TEA_BUFFER_SIZE);
    tea_push_nil(T);
}

static void file_close(tea_State* T)
{
    IOFileUD* iof = io_get_file(T);
    if((iof->type & IOFILE_TYPE_MASK) == IOFILE_TYPE_STDF)
    {
        tea_error(T, "Cannot close standard file");
    }
    fclose(iof->fp);
    iof->fp = NULL;
    iof->type = IOFILE_TYPE_CLOSE;
    tea_push_nil(T);
}

static void file_tostring(tea_State* T)
{
    IOFileUD* iof = io_get_filep(T);
    if(iof->type & IOFILE_TYPE_CLOSE)
        tea_push_literal(T, "<file (closed)>");
    else
        tea_push_literal(T, "<file>");
}

static void file_iterate(tea_State* T)
{
    file_readline(T);
}

static void file_iteratorvalue(tea_State* T) {}

/* ------------------------------------------------------------------------ */

static void io_open(tea_State* T)
{
    int count = tea_get_top(T);
    tea_check_args(T, count < 1 || count > 2, "Expected 1 or 2 arguments, got %d", count);

    const char* path = tea_check_string(T, 0);
    const char* mode = tea_opt_string(T, 1, "r");

    FILE* fp = fopen(path, mode);
    if(fp == NULL)
    {
        tea_error(T, "Unable to open file " TEA_QS, path);
    }

    io_file_new(T, fp, IOFILE_TYPE_FILE);
}

static void io_popen(tea_State* T)
{
#if TEA_TARGET_POSIX || TEA_TARGET_WINDOWS
    int count = tea_get_top(T);
    tea_check_args(T, count < 1 || count > 2, "Expected 1 or 2 arguments, got %d", count);

    const char* path = tea_check_string(T, 0);
    const char* mode = tea_opt_string(T, 1, "r");
    FILE* fp;

#if TEA_TARGET_POSIX
    fp = fopen(path, mode);
#else
    fp = _popen(path, mode);
#endif
    if(fp == NULL)
    {
        tea_error(T, "Unable to open " TEA_QS, path);
    }

    io_file_new(T, fp, IOFILE_TYPE_PIPE);
#else
    tea_error(T, TEA_QL("popen") " not supported");
#endif
}

static void io_stdfile(tea_State* T, FILE* fp, const char* name, const char* mode)
{
    GCclass* klass = classV(T->base + 1);
    GCinstance* instance = tea_instance_new(T, klass);
    setinstanceV(T, T->top++, instance);
    IOFileUD* iof = (IOFileUD*)tea_new_userdata(T, sizeof(IOFileUD));
    tea_set_finalizer(T, io_file_free);
    iof->fp = fp;
    iof->type = IOFILE_TYPE_STDF;
    tea_set_attr(T, -2, "FILE*");
    tea_set_attr(T, 0, name);
}

static const tea_Class file_class[] = {
    { "write", "method", file_write, TEA_VARARGS },
    { "read", "method", file_read, TEA_VARARGS },
    { "readline", "method", file_readline, 1 },
    { "seek", "method", file_seek, TEA_VARARGS },
    { "flush", "method", file_flush, 1 },
    { "setvbuf", "method", file_setvbuf, 2 },
    { "close", "method", file_close, 1 },
    { "tostring", "method", file_tostring, 1 },
    { "iterate", "method", file_iterate, 2 },
    { "iteratorvalue", "method", file_iteratorvalue, 2 },
    { NULL, NULL, NULL }
};

static const tea_Module io_module[] = {
    { "open", NULL },
    { "popen", NULL },
    { "stdin", NULL },
    { "stdout", NULL },
    { "stderr", NULL },
    { NULL, NULL }
};

TEAMOD_API void tea_import_io(tea_State* T)
{
    tea_create_module(T, TEA_MODULE_IO, io_module);
    tea_create_class(T, "File", file_class);
    tea_push_value(T, -1);
    tea_push_cclosure(T, io_open, TEA_VARARGS, 1);
    tea_set_attr(T, 0, "open");

    tea_push_value(T, -1);
    tea_push_cclosure(T, io_popen, TEA_VARARGS, 1);
    tea_set_attr(T, 0, "popen");

    io_stdfile(T, stdout, "stdout", "w");
    io_stdfile(T, stdin, "stdin", "r");
    io_stdfile(T, stderr, "stderr", "w");
    tea_set_attr(T, 0, "File");
}