/*
** lib_buffer.c
** Buffer class
*/

#define lib_buffer_c
#define TEA_LIB

#include "tealib.h"

#include "tea_obj.h"
#include "tea_buf.h"
#include "tea_strfmt.h"
#include "tea_err.h"

/* -- Helper functions ---------------------------------------------------- */

static void buffer_free(void* p)
{
    SBufExt* sbx = (SBufExt*)p;
    tea_bufx_free(sbx->T, sbx);
}

static SBufExt* buffer_getp(tea_State* T)
{
    tea_check_type(T, 0, TEA_TYPE_INSTANCE);
    tea_get_attr(T, 0, "BUFFER*");
    SBufExt* sbx = (SBufExt*)tea_get_userdata(T, -1);
    T->top--;
    return sbx;
}

/* -- Buffer methods ------------------------------------------------------ */

static void buffer_len(tea_State* T)
{
    if(tea_get_top(T) != 1) tea_error(T, "readonly property");
    SBufExt* sbx = buffer_getp(T);
    tea_push_number(T, sbufx_len(sbx));
}

static void buffer_constructor(tea_State* T)
{
    tea_check_instance(T, 0);
    SBufExt* sbx = (SBufExt*)tea_new_userdata(T, sizeof(SBufExt));
    tea_set_finalizer(T, buffer_free);
    tea_bufx_init(sbx);
    sbx->T = T;
    tea_set_attr(T, -2, "BUFFER*");
}

static void buffer_skip(tea_State* T)
{
    SBufExt* sbx = buffer_getp(T);
    size_t n = (size_t)tea_check_integer(T, 1);
    size_t len = sbufx_len(sbx);
    if(n < len)
    {
        sbx->r += n;
    }
    else
    {
        sbx->r = sbx->w = sbx->b;
    }
    T->top = T->base + 1;   /* Chain buffer */
}

static void buffer_reset(tea_State* T)
{
    SBufExt* sbx = buffer_getp(T);
    tea_bufx_reset(sbx);
    T->top = T->base + 1;   /* Chain buffer */
}

static void buffer_put(tea_State* T)
{
    SBufExt* sbx = buffer_getp(T);
    ptrdiff_t arg, narg = T->top - T->base;
    for(arg = 1; arg < narg; arg++)
    {
        cTValue* o = &T->base[arg];
        if(tvisstr(o))
        {
            tea_buf_putstr(T, (SBuf*)sbx, strV(o));
        }
        else if(tvisnumber(o))
        {
            tea_strfmt_putfnum(T, (SBuf*)sbx, STRFMT_G14, numberV(o));
        }
        else
        {
            tea_err_argtype(T, (int)arg, "string or number");
        }
    }
    T->top = T->base + 1;   /* Chain buffer */
}

static void buffer_format(tea_State* T)
{
    SBufExt* sbx = buffer_getp(T);
    tea_strfmt_putarg(T, (SBuf*)sbx, 1, 2);
    T->top = T->base + 1;   /* Chain buffer */
}

static void buffer_tostring(tea_State* T)
{
    SBufExt* sbx = buffer_getp(T);
    setstrV(T, T->top - 1, tea_str_new(T, sbx->r, sbufx_len(sbx)));
}

/* ------------------------------------------------------------------------ */

static const tea_Class buffer_class[] = {
    { "len", "property", buffer_len, TEA_VARARGS },
    { "constructor", "method", buffer_constructor, 1 },
    { "skip", "method", buffer_skip, 2 },
    { "reset", "method", buffer_reset, 1 },
    { "put", "method", buffer_put, TEA_VARARGS },
    { "format", "method", buffer_format, TEA_VARARGS },
    { "tostring", "method", buffer_tostring, 1 },
    { NULL, NULL, NULL }
};

void tea_open_buffer(tea_State* T)
{
    tea_create_class(T, TEA_CLASS_BUFFER, buffer_class);
    tea_set_global(T, TEA_CLASS_BUFFER);
    tea_push_null(T);
}