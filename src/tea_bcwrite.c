/*
** tea_bcwrite.c
** Teascript bytecode writer
*/

#define tea_bcwrite_c
#define TEA_CORE

#include "tea_arch.h"
#include "tea_bcdump.h"
#include "tea_buf.h"
#include "tea_vm.h"

/* Context for bytecode writer */
typedef struct BCWriteCtx
{
    tea_State* T;
    SBuf sb;  /* Output buffer */
    GCproto* pt;  /* Root prototype */
    tea_Writer writer;   /* Writer callback */
    void* data;   /* Writer data */
    uint32_t flags; /* BCDUMP_F_* flags */
    int status; /* Status for writer callback */
} BCWriteCtx;

#ifdef TEA_USE_ASSERT
#define tea_assertBCW(c, ...)   tea_assertT_(ctx->T, (c), __VA_ARGS__)
#else
#define tea_assertBCW(c, ...)   ((void)ctx)
#endif

/* -- Bytecode writer -------------------------------------------------- */

/* Write ULEB128 to buffer */
static char* bcwrite_wuleb128(char* p, uint32_t v)
{
    for(; v >= 0x80; v >>= 7)
        *p++ = (char)((v & 0x7f) | 0x80);
    *p++ = (char)v;
    return p;
}

/* Write number from constants */
static void bcwrite_knum(BCWriteCtx* ctx, TValue* v)
{
    char* p = tea_buf_more(ctx->T, &ctx->sb, 10);
    double num = numV(v);
    NumberBits x;
    x.n = num;
    p = bcwrite_wuleb128(p, 1+(2*x.u32.lo | (x.u32.lo & 0x80000000u)));
    if(x.u32.lo >= 0x80000000u)
	    p[-1] = (p[-1] & 7) | ((x.u32.lo>>27) & 0x18);
    p = bcwrite_wuleb128(p, x.u32.hi);
    ctx->sb.w = p;
}

/* Write constants of a prototype */
static void bcwrite_kgc(BCWriteCtx* ctx, GCproto* pt)
{
    for(int i = 0; i < pt->sizek; i++)
    {
        TValue* o = proto_kgc(pt, i);
        size_t type = 0;
        size_t need = 1;
        char* p;
        
        /* Determine constant type and needed size */
        if(tvisstr(o))
        {
            GCstr* str = strV(o);
            type = BCDUMP_KGC_STR + str->len;
            need = 5 + str->len;
        }
        else if(tvisproto(o))
        {
            type = BCDUMP_KGC_FUNC;
        }
        else if(tvisnum(o))
        {
            type = BCDUMP_KGC_NUM;
        }

        /* Write constant type */
        p = tea_buf_more(ctx->T, &ctx->sb, need);
        p = bcwrite_wuleb128(p, type);

        /* Write constant data */
        if(type >= BCDUMP_KGC_STR)
        {
            GCstr* str = strV(o);
            p = tea_buf_wmem(p, str_data(str), str->len);
        }
        ctx->sb.w = p;
        if(type == BCDUMP_KGC_NUM)
        {
            bcwrite_knum(ctx, o);
        }
    }
}

/* Write bytecode instructions */
static char* bcwrite_bytecode(BCWriteCtx* ctx, GCproto* pt, char* p)
{
    UNUSED(ctx);
    p = tea_buf_wmem(p, proto_bc(pt), pt->sizebc);
    return p;
}

/* Write prototype */
static void bcwrite_proto(BCWriteCtx* ctx, GCproto* pt)
{
    size_t sizedbg = 0;
    size_t len = pt->name->len;
    char* p;

    /* Recursively write children of prototype */
    for(int i = pt->sizek - 1; i >= 0; i--)
    {
        TValue* o = proto_kgc(pt, i);
        if(tvisproto(o))
        {
            bcwrite_proto(ctx, protoV(o));
        }    
    }

    /* Start writing the prototype into the buffer */
    p = tea_buf_need(ctx->T, &ctx->sb, 5+(5+len)+6+2*5+(pt->sizebc-1));
    p += 5; /* Leave room for final size */

    /* Write prototype name */
    p = bcwrite_wuleb128(p, len);
    p = tea_buf_wmem(p, str_data(pt->name), len);

    /* Write prototype header */
    *p++ = pt->numparams;
    *p++ = pt->numopts;
    *p++ = pt->flags;
    *p++ = pt->max_slots;
    *p++ = pt->sizeuv;
    p = bcwrite_wuleb128(p, pt->sizebc);
    p = bcwrite_wuleb128(p, pt->sizek);
    if(!(ctx->flags & BCDUMP_F_STRIP))
    {
        if(pt->lineinfo)
            sizedbg = pt->sizept - (size_t)((char*)pt->lineinfo - (char*)pt);
        p = bcwrite_wuleb128(p, sizedbg);
        if(sizedbg)
        {
            p = bcwrite_wuleb128(p, pt->firstline);
            p = bcwrite_wuleb128(p, pt->numline);
        }
    }

    /* Write bytecode instructions */
    p = bcwrite_bytecode(ctx, pt, p);
    ctx->sb.w = p;

    /* Write constants */
    bcwrite_kgc(ctx, pt);

    /* Write debug info, if not stripped */
    if(sizedbg)
    {
        p = tea_buf_more(ctx->T, &ctx->sb, sizedbg);
        p = tea_buf_wmem(p, pt->lineinfo, sizedbg);
        ctx->sb.w = p;
    }

    /* Pass buffer to writer function prototype */
    if(ctx->status == TEA_OK)
    {
        size_t n = sbuf_len(&ctx->sb) - 5;
        size_t nn = (tea_fls(n)+8)*9 >> 6;
        char* q = ctx->sb.b + (5 - nn);
        p = bcwrite_wuleb128(q, n); /* Fill in final size */
        tea_assertBCW(p == ctx->sb.b + 5, "bad ULEB128 write");
        ctx->status = ctx->writer(ctx->T, ctx->data, q, nn+n);
    }
}

/* Write header of bytecode dump */
static void bcwrite_header(BCWriteCtx* ctx)
{
    char* p = tea_buf_need(ctx->T, &ctx->sb, 5);
    *p++ = BCDUMP_HEAD1;
    *p++ = BCDUMP_HEAD2;
    *p++ = BCDUMP_HEAD3;
    *p++ = BCDUMP_HEAD4;
    *p++ = BCDUMP_VERSION;
    *p++ = (ctx->flags & BCDUMP_F_STRIP) +
        TEA_BE * BCDUMP_F_BE;
    ctx->status = ctx->writer(ctx->T, ctx->data, 
                            ctx->sb.b, (size_t)(p - ctx->sb.b));
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

/* Protected callback for bytecode writer */
static void f_writer(tea_State* T, void* ud)
{
    BCWriteCtx* ctx = (BCWriteCtx*)ud;
    tea_buf_need(T, &ctx->sb, 1024);  /* Avoids resize for most prototypes */
    bcwrite_header(ctx);
    bcwrite_proto(ctx, ctx->pt);
    bcwrite_footer(ctx);
}

/* Write bytecode for a prototype */
int tea_bcwrite(tea_State* T, GCproto* pt, tea_Writer writer, void* data, uint32_t flags)
{
    BCWriteCtx ctx;
    ctx.T = T;
    ctx.pt = pt;
    ctx.writer = writer;
    ctx.data = data;
    ctx.flags = flags;
    ctx.status = TEA_OK;
    tea_buf_init(&ctx.sb);
    int status = tea_vm_pcall(T, f_writer, &ctx, stack_save(T, T->top));
    if(status == TEA_OK)
        status = ctx.status;
    tea_buf_free(T, &ctx.sb);
    return status;
}