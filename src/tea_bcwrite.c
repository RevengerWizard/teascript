/*
** tea_bcwrite.c
** Teascript bytecode writer
*/

#define tea_bcwrite_c
#define TEA_CORE

#include "tea_bcdump.h"
#include "tea_buf.h"
#include "tea_vm.h"

/* Context for bytecode writer */
typedef struct BCWriteCtx
{
    tea_State* T;
    SBuf sbuf;  /* Output buffer */
    GCproto* f;  /* Root prototype */
    tea_Writer writer;   /* Writer callback */
    void* data;   /* Writer data */
    int status; /* Status for writer callback */
} BCWriteCtx;

/* Write ULEB128 to buffer */
static char* bcwrite_wuleb128(char* p, uint32_t v)
{
    for(; v >= 0x80; v >>= 7)
        *p++ = (char)((v & 0x7f) | 0x80);
    *p++ = (char)v;
    return p;
}

/* Write number from constants */
static void bcwrite_knum(BCWriteCtx* ctx, Value v)
{
    char* p = tea_buf_more(ctx->T, &ctx->sbuf, 10);
    double num = AS_NUMBER(v);

    union
    {
        double x;
        struct 
        {
            uint32_t lo;
            uint32_t hi;
        } u32;
    } n;

    n.x = num;

    p = bcwrite_wuleb128(p, 1+(2*n.u32.lo | (n.u32.lo & 0x80000000u)));
    if(n.u32.lo >= 0x80000000u)
	    p[-1] = (p[-1] & 7) | ((n.u32.lo>>27) & 0x18);
    p = bcwrite_wuleb128(p, n.u32.hi);

    ctx->sbuf.w = p;
}

/* Write constants of a prototype */
static void bcwrite_kgc(BCWriteCtx* ctx, GCproto* pt)
{
    for(int i = 0; i < pt->k_count; i++)
    {
        Value v = pt->k[i];
        size_t type = 0;
        size_t need = 1;
        char* p;
        
        /* Determine constant type and needed size */
        if(IS_STRING(v))
        {
            GCstr* str = AS_STRING(v);
            type = BCDUMP_KGC_STR + str->len;
            need = 5 + str->len;
        }
        else if(IS_PROTO(v))
        {
            type = BCDUMP_KGC_FUNC;
        }
        else if(IS_NUMBER(v))
        {
            type = BCDUMP_KGC_NUM;
        }

        /* Write constant type */
        p = tea_buf_more(ctx->T, &ctx->sbuf, need);
        p = bcwrite_wuleb128(p, type);

        /* Write constant data */
        if(type >= BCDUMP_KGC_STR)
        {
            GCstr* str = AS_STRING(v);
            p = tea_buf_wmem(p, str->chars, str->len);
        }
        ctx->sbuf.w = p;
        if(type == BCDUMP_KGC_NUM)
        {
            bcwrite_knum(ctx, v);
        }
    }
}

/* Write bytecode instructions */
static char* bcwrite_bytecode(BCWriteCtx* ctx, GCproto* pt, char* p)
{
    UNUSED(ctx);
    p = tea_buf_wmem(p, pt->code, pt->count);
    return p;
}

/* Write prototype */
static void bcwrite_proto(BCWriteCtx* ctx, GCproto* pt)
{
    /* Recursively write children of prototype */
    for(int i = pt->k_count - 1; i >= 0; i--)
    {
        Value v = pt->k[i];
        if(IS_PROTO(v))
        {
            bcwrite_proto(ctx, AS_PROTO(v));
        }    
    }

    size_t len = pt->name->len;

    /* Start writing the prototype into the buffer */
    char* p = tea_buf_need(ctx->T, &ctx->sbuf, 5+(5+len)+6+2*5+(pt->count-1));
    p += 5; /* Leave room for final size */

    /* Write prototype name */
    p = bcwrite_wuleb128(p, len);
    p = tea_buf_wmem(p, pt->name->chars, len);

    /* Write prototype header */
    *p++ = pt->arity;
    *p++ = pt->arity_optional;
    *p++ = pt->variadic;
    *p++ = pt->max_slots;
    *p++ = pt->upvalue_count;
    *p++ = pt->type;
    p = bcwrite_wuleb128(p, pt->count);
    p = bcwrite_wuleb128(p, pt->k_count);

    /* Write bytecode instructions */
    p = bcwrite_bytecode(ctx, pt, p);
    ctx->sbuf.w = p;

    /* Write constants */
    bcwrite_kgc(ctx, pt);

    /* Pass buffer to writer function prototype */
    if(ctx->status == TEA_OK)
    {
        size_t n = sbuf_len(&ctx->sbuf) - 5;
        size_t nn = (tea_fls(n)+8)*9 >> 6;
        char* q = ctx->sbuf.b + (5 - nn);
        p = bcwrite_wuleb128(q, n); /* Fill in final size */
        ctx->status = ctx->writer(ctx->T, ctx->data, q, nn+n);
    }
}

/* Write header of bytecode dump */
static void bcwrite_header(BCWriteCtx* ctx)
{
    char* p = tea_buf_need(ctx->T, &ctx->sbuf, 5);
    *p++ = BCDUMP_HEAD1;
    *p++ = BCDUMP_HEAD2;
    *p++ = BCDUMP_HEAD3;
    *p++ = BCDUMP_HEAD4;
    *p++ = BCDUMP_VERSION;

    ctx->status = ctx->writer(ctx->T, ctx->data, ctx->sbuf.b, (size_t)(p - ctx->sbuf.b));
}

/* Write footer of bytecode dump */
static void bcwrite_footer(BCWriteCtx* ctx)
{
    if(ctx->status == TEA_OK)
    {
        uint8_t zero = 0;
        ctx->status = ctx->writer(ctx->T, ctx->data, &zero, 1);
    }
}

static void f_writer(tea_State* T, void* ud)
{
    BCWriteCtx* ctx = (BCWriteCtx*)ud;
    UNUSED(T);
    tea_buf_need(T, &ctx->sbuf, 1024);  /* Avoid resize */
    bcwrite_header(ctx);
    bcwrite_proto(ctx, ctx->f);
    bcwrite_footer(ctx);
}

/* Write bytecode for a prototype */
int tea_bcwrite(tea_State* T, GCproto* proto, tea_Writer writer, void* data)
{
    BCWriteCtx ctx;
    ctx.T = T;
    ctx.f = proto;
    ctx.writer = writer;
    ctx.data = data;
    ctx.status = TEA_OK;

    tea_buf_init(&ctx.sbuf);
    int status = tea_vm_pcall(T, f_writer, &ctx, stacksave(T, T->top));
    if(status == TEA_OK)
        status = ctx.status;
    tea_buf_free(T, &ctx.sbuf);
    return status;
}