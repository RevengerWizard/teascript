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
    if(!(T->base < T->top && tvisudata(T->base) &&
        udataV(T->base)->udtype == UDTYPE_IOFILE))
        tea_err_argtype(T, 0, "FILE*");
    return (IOFileUD*)ud_data(udataV(T->base));
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
    GCudata* ud = tea_udata_new(T, sizeof(IOFileUD), 0);
    ud->udtype = UDTYPE_IOFILE;
    ud->klass = klass;
    ud->fd = io_file_free;
    setudataV(T, T->top++, ud);
    IOFileUD* iof = (IOFileUD*)ud_data(ud);
    iof->fp = fp;
    iof->type = type;
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
    FILE* fp = io_get_file(T)->fp;
    int opt = tea_lib_checkopt(T, 1, 1, "\3set\3cur\3end");
    int64_t ofs = 0;
    cTValue* o;
    int res;
    if(opt == 0) opt = SEEK_SET;
    else if(opt == 1) opt = SEEK_CUR;
    else if(opt == 2) opt = SEEK_END;

    o = T->base + 2;
    if(o < T->top)
    {
        if(!tvisnum(o))
            tea_err_argt(T, 3, TEA_TYPE_NUMBER);
        ofs = (int64_t)numV(o);
    }

#if TEA_TARGET_POSIX
    res = fseeko(fp, ofs, opt);
#elif _MSC_VER >= 1400
    res = _fseeki64(fp, ofs, opt);
#elif defined(__MINGW32__)
    res = fseeko64(fp, ofs, opt);
#else
    res = fseek(fp, (long)ofs, opt);
#endif

    if(res) tea_lib_fileresult(T, NULL);

#if TEA_TARGET_POSIX
    ofs = ftello(fp);
#elif _MSC_VER >= 1400
    ofs = _ftelli64(fp);
#elif defined(__MINGW32__)
    ofs = ftello64(fp);
#else
    ofs = (int64_t)ftell(fp);
#endif
    setint64V(T->top - 1, ofs);
}

static void file_flush(tea_State* T)
{
    IOFileUD* iof = io_get_file(T);
    fflush(iof->fp);
    tea_push_nil(T);
}

static void file_setvbuf(tea_State* T)
{
    IOFileUD* iof = io_get_file(T);
    int opt = tea_lib_checkopt(T, 1, -1, "\4full\4line\2no");
    size_t size = (size_t)tea_lib_optint(T, 2, TEA_BUFFER_SIZE);
    if(opt == 0) opt = _IOFBF;
    else if(opt == 1) opt = _IOLBF;
    else if(opt == 2) opt = _IONBF;
    setvbuf(iof->fp, NULL, opt, size);
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
    const char* path = tea_check_string(T, 0);
    const char* mode = tea_opt_string(T, 1, "r");
    FILE* fp;

#if TEA_TARGET_POSIX
    fp = popen(path, mode);
#else
    fp = _popen(path, mode);
#endif
    if(fp == NULL)
    {
        tea_err_callerv(T, TEA_ERR_OPEN, path);
    }

    io_file_new(T, fp, IOFILE_TYPE_PIPE);
#else
    tea_error(T, TEA_QL("popen") " not supported");
#endif
}

static void io_stdfile(tea_State* T, FILE* fp, const char* name, const char* mode)
{
    GCclass* klass = classV(T->top - 1);
    GCudata* ud = tea_udata_new(T, sizeof(IOFileUD), 0);
    ud->udtype = UDTYPE_IOFILE;
    ud->klass = klass;
    ud->fd = io_file_free;
    setudataV(T, T->top++, ud);
    IOFileUD* iof = (IOFileUD*)ud_data(ud);
    iof->fp = fp;
    iof->type = IOFILE_TYPE_STDF;
    tea_set_attr(T, 0, name);
}

static const tea_Methods file_class[] = {
    { "write", "method", file_write, TEA_VARG, 0 },
    { "read", "method", file_read, 1, 1 },
    { "readline", "method", file_readline, 1, 0 },
    { "seek", "method", file_seek, 1, 0 },
    { "flush", "method", file_flush, 1, 0 },
    { "setvbuf", "method", file_setvbuf, 1, 1 },
    { "close", "method", file_close, 1, 0 },
    { "tostring", "method", file_tostring, 1, 0 },
    { "iterate", "method", file_iterate, 2, 0 },
    { "iteratorvalue", "method", file_iteratorvalue, 2, 0 },
    { NULL, NULL, NULL }
};

static const tea_Reg io_module[] = {
    { "open", io_open, 1, 1 },
    { "popen", io_popen, 1, 1 },
    { "stdin", NULL },
    { "stdout", NULL },
    { "stderr", NULL },
    { NULL, NULL }
};

TEAMOD_API void tea_import_io(tea_State* T)
{
    tea_new_module(T, TEA_MODULE_IO);
    tea_create_class(T, "File", file_class);
    tea_push_value(T, -1);
    tea_set_attr(T, 0, "File");
    tea_set_funcs(T, io_module, 1);
    tea_get_attr(T, 0, "File");

    io_stdfile(T, stdout, "stdout", "w");
    io_stdfile(T, stdin, "stdin", "r");
    io_stdfile(T, stderr, "stderr", "w");
    T->top--;
}