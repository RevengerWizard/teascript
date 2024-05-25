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
#include "tea_udata.h"
#include "tea_err.h"
#include "tea_meta.h"
#include "tea_vm.h"

/* -- Helper functions ---------------------------------------------------- */

static void buffer_free(void* p)
{
    SBufExt* sbx = (SBufExt*)p;
    tea_bufx_free(sbx->T, sbx);
}

/* Check that first argument is a string buffer */
static SBufExt* buffer_getp(tea_State* T)
{
    if(!(T->base < T->top && tvisbuf(T->base)))
        tea_err_argtype(T, 0, "buffer");
    return bufV(T->base);
}

/* -- Buffer methods ------------------------------------------------------ */

static void buffer_len(tea_State* T)
{
    if(tea_get_top(T) != 1) tea_error(T, "readonly property");
    SBufExt* sbx = buffer_getp(T);
    tea_push_number(T, sbufx_len(sbx));
}

static void buffer_init(tea_State* T)
{
    tea_check_type(T, 0, TEA_TYPE_CLASS);
    GCclass* klass = classV(T->base);
    GCudata* ud = tea_udata_new(T, sizeof(SBufExt));
    ud->klass = klass;
    ud->udtype = UDTYPE_BUFFER;
    ud->fd = buffer_free;
    setudataV(T, T->top++, ud);
    SBufExt* sbx = (SBufExt*)ud_data(ud);
    tea_bufx_init(sbx);
    sbx->T = T;
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
        TValue* mo = NULL;
retry:
        if(tvisstr(o))
        {
            tea_buf_putstr(T, (SBuf*)sbx, strV(o));
        }
        else if(tvisnum(o))
        {
            tea_strfmt_putfnum(T, (SBuf*)sbx, STRFMT_G14, numV(o));
        }
        else if(tvisbuf(o))
        {
            SBufExt* sbx2 = bufV(o);
            if(sbx2 == sbx) tea_err_arg(T, (int)(arg), TEA_ERR_BUFFER_SELF);
            tea_buf_putmem(T, (SBuf*)sbx, sbx2->r, sbufx_len(sbx2));
        }
        else if(!mo && (mo = tea_meta_lookup(T, o, MM_TOSTRING)) != NULL)
        {
            /* Call tostring method inline */
            copyTV(T, T->top++, o);
            copyTV(T, T->top++, mo);
            copyTV(T, T->top++, o);
            tea_vm_call(T, mo, 0);
            o = &T->base[arg];  /* Stack may have been reallocated */
            TValue* tv = --T->top;
            if(!tvisstr(tv))
                tea_err_run(T, TEA_ERR_TOSTR);
            copyTV(T, &T->base[arg], tv);
            T->top = T->base + narg;
            goto retry; /* Retry with result */
        }
        else
        {
            tea_err_argtype(T, (int)arg, "string, number or tostring");
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

static const tea_Methods buffer_class[] = {
    { "len", "property", buffer_len, TEA_VARARGS },
    { "init", "method", buffer_init, 1 },
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
    tea_push_nil(T);
}