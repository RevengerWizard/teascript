/*
** tea_bcread.h
** Teascript bytecode reader
*/

#include <stdlib.h>

#define tea_bcread_c
#define TEA_CORE

#include "tea_arch.h"
#include "tea_def.h"
#include "tea_bcdump.h"
#include "tea_strfmt.h"
#include "tea_err.h"

/* Reuse some lexer fields for our own purposes */
#define bcread_flags(ls)    ls->num_braces
#define bcread_swap(ls) \
    ((bcread_flags(ls) & BCDUMP_F_BE) != TEA_BE * BCDUMP_F_BE)
#define bcread_oldtop(L, ls)	stack_restore(L, ls->linenumber)
#define bcread_savetop(L, ls, top) \
    ls->linenumber = (BCLine)stack_save(L, (top))

/* -- Input buffer handling -------------------------------------------------- */

/* Throw reader error */
static TEA_NOINLINE void bcread_error(LexState* ls, ErrMsg em)
{
    tea_State* T = ls->T;
    const char* name = str_data(ls->module->name);
    tea_strfmt_pushf(T, "%s: %s", name, err2msg(em));
    tea_err_throw(T, TEA_ERROR_SYNTAX);
}

/* Refill buffer */
static TEA_NOINLINE void bcread_fill(LexState* ls, size_t len, bool need)
{
    tea_assertLS(len != 0, "empty refill");
    if(len > TEA_MAX_BUF || ls->c < 0)
        bcread_error(ls, TEA_ERR_BCBAD);
    do
    {
        const char* buf;
        size_t size;
        char* p = ls->sb.b;
        size_t n = (size_t)(ls->pe - ls->p);

        /* Copy remainder to buffer */
        if(n) 
        {
            /* Move down in buffer */
            if(sbuf_len(&ls->sb))
            {
                tea_assertLS(ls->pe == ls->sb.w, "bad buffer pointer");
                if(ls->p != p) 
                    memmove(p, ls->p, n);
            }
            else 
            {
                /* Copy from buffer provided by reader */
                p = tea_buf_need(ls->T, &ls->sb, len);
                memcpy(p, ls->p, n);
            }
            ls->p = p;
            ls->pe = p + n;
        }

        ls->sb.w = p + n;
        buf = ls->reader(ls->T, ls->rdata, &size);  /* Get more data from reader */
        if(buf == NULL || size == 0)
        {
            if(need) 
                bcread_error(ls, TEA_ERR_BCBAD);
            ls->c = -1;
            break;
        }
        if(size >= TEA_MAX_BUF - n)
            tea_err_mem(ls->T);
        if(n)
        {
            /* Append to buffer */
            n += size;
            p = tea_buf_need(ls->T, &ls->sb, n < len ? len : n);
            memcpy(ls->sb.w, buf, size);
            ls->sb.w = p + n;
            ls->p = p;
            ls->pe = p + n;
        } 
        else 
        {
            /* Return buffer provided by reader */
            ls->p = buf;
            ls->pe = buf + size;
        }
    } 
    while((size_t)(ls->pe - ls->p) < len);
}

/* Need a certain number of bytes */
static TEA_AINLINE void bcread_need(LexState* ls, size_t len)
{
    if(TEA_UNLIKELY((size_t)(ls->pe - ls->p) < len))
        bcread_fill(ls, len, true);
}

/* Want to read up to a certain number of bytes, but may need less */
static TEA_AINLINE void bcread_want(LexState* ls, size_t len)
{
    if(TEA_UNLIKELY((size_t)(ls->pe - ls->p) < len))
        bcread_fill(ls, len, false);
}

/* Return memory block from buffer */
static TEA_AINLINE uint8_t* bcread_mem(LexState* ls, size_t len)
{
    uint8_t* p = (uint8_t*)ls->p;
    ls->p += len;
    tea_assertLS(ls->p <= ls->pe, "buffer read overflow");
    return p;
}

/* Copy memory block from buffer */
static void bcread_block(LexState* ls, void* q, size_t len)
{
    memcpy(q, bcread_mem(ls, len), len);
}

/* Read byte from buffer */
static TEA_AINLINE uint32_t bcread_byte(LexState* ls)
{
    tea_assertLS(ls->p < ls->pe, "buffer read overflow");
    return (uint32_t)(uint8_t)*ls->p++;
}

/* Read ULEB128 from buffer */
static TEA_AINLINE uint32_t bcread_uleb128(LexState* ls)
{
    uint32_t v = tea_buf_ruleb128(&ls->p);
    tea_assertLS(ls->p <= ls->pe, "buffer read overflow");
    return v;
}

/* Read top 32 bits of 33 bit ULEB128 value from buffer */
static uint32_t bcread_uleb128_33(LexState* ls)
{
    const uint8_t* p = (const uint8_t*)ls->p;
    uint32_t v = (*p++ >> 1);
    if(v >= 0x40)
    {
        int sh = -1;
        v &= 0x3f;
        do 
        {
            v |= ((*p & 0x7f) << (sh += 7));
        } 
        while(*p++ >= 0x80);
    }
    ls->p = (char*)p;
    tea_assertLS(ls->p <= ls->pe, "buffer read overflow");
    return v;
}

/* -- Bytecode reader -------------------------------------------------- */

/* Read debug info of a prototype */
static void bcread_dbg(LexState* ls, GCproto *pt, size_t sizedbg)
{
    void* lineinfo = (void*)pt->lineinfo;
    bcread_block(ls, lineinfo, sizedbg);
    /* Swap lineinfo if the endianess differs */
    if(bcread_swap(ls) && pt->numline >= 256)
    {
        size_t i, n = pt->sizebc - 1;
        if(pt->numline < 65536)
        {
            uint16_t *p = (uint16_t*)lineinfo;
            for(i = 0; i < n; i++) p[i] = (uint16_t)((p[i] >> 8)|(p[i] << 8));
        } 
        else
        {
            uint32_t* p = (uint32_t*)lineinfo;
            for(i = 0; i < n; i++) p[i] = tea_bswap(p[i]);
        }
    }
}

/* Read number from constants */
static double bcread_knum(LexState* ls)
{
    NumberBits x;
    uint32_t lo = bcread_uleb128_33(ls);
    x.u32.lo = lo;
    x.u32.hi = bcread_uleb128(ls);
    return x.n;
}

/* Read GC constants from function prototype */
static void bcread_kgc(LexState* ls, GCproto* pt, size_t sizek)
{
    pt->sizek = sizek;
    for(int i = 0; i < sizek; i++)
    {
        size_t type = bcread_uleb128(ls);
        if(type >= BCDUMP_KGC_STR)
        {
            int len = type - BCDUMP_KGC_STR;
            const char* p = (const char*)bcread_mem(ls, len);
            GCstr* str = tea_str_new(ls->T, p, len);
            setstrV(ls->T, proto_kgc(pt, i), str);
        }
        else if(type == BCDUMP_KGC_NUM)
        {
            double num = bcread_knum(ls);
            setnumV(proto_kgc(pt, i), num);
        }
        else
        {
            tea_State* T = ls->T;
            tea_assertLS(type == BCDUMP_KGC_FUNC, "bad constant type %d", type);
            if(T->top <= bcread_oldtop(T, ls))  /* Stack underflow? */
	            bcread_error(ls, TEA_ERR_BCBAD);
            T->top--;
            TValue* v = T->top;
            copyTV(T, proto_kgc(pt, i), v);
        }
    }
}

/* Read bytecode instructions */
static void bcread_bytecode(LexState* ls, GCproto* pt, size_t sizebc)
{
    BCIns* bc = proto_bc(pt);
    bcread_block(ls, bc, sizebc);
    /* Swap bytecode instructions if the endianess differs */
    if(bcread_swap(ls))
    {
        for(int i = 0; i < sizebc; i++) bc[i] = tea_bswap(bc[i]);
    }
}

/* Read upvalue indexes */
static void bcread_uv(LexState* ls, GCproto* pt, size_t sizeuv)
{
    if(sizeuv)
    {
        uint16_t* uv = pt->uv;
        bcread_block(ls, uv, sizeuv*sizeof(uint16_t));
        /* Swap upvalue idexes if the endianess differs */
        if(bcread_swap(ls))
        {
            uint32_t i;
            for(i = 0; i < sizeuv; i++)
                uv[i] = (uint16_t)((uv[i] >> 8)|(uv[i] << 8));
        }
    }
}

/* Read prototype */
static GCproto* bcread_proto(LexState* ls)
{
    GCproto* pt;
    size_t numparams, numopts, flags, max_slots, sizeuv, sizebc, sizek, sizept;
    size_t ofsk, ofsuv, ofsdbg;
    size_t sizedbg = 0;
    BCLine firstline = 0, numline = 0;

    /* Read prototype name */
    size_t len = bcread_uleb128(ls);
    const char* name = (const char*)bcread_mem(ls, len);

    /* Read prototype header */
    numparams = bcread_byte(ls);
    numopts = bcread_byte(ls);
    flags = bcread_byte(ls);
    max_slots = bcread_byte(ls);
    sizeuv = bcread_byte(ls);
    sizebc = bcread_uleb128(ls);
    sizek = bcread_uleb128(ls);
    if(!(bcread_flags(ls) & BCDUMP_F_STRIP))
    {
        sizedbg = bcread_uleb128(ls);
        if(sizedbg)
        {
            firstline = bcread_uleb128(ls);
            numline = bcread_uleb128(ls);
        }
    }

    /* Calculate total size of prototype including all colocated arrays */
    sizept = sizeof(GCproto) + sizebc * sizeof(BCIns);
    ofsk = sizept; sizept += sizek * sizeof(TValue);
    ofsuv = sizept; sizept += sizeuv * sizeof(uint16_t);
    ofsdbg = sizept; sizept += sizedbg;

    /* Allocate prototype and initialize its fields */
    pt = (GCproto*)tea_mem_newgco(ls->T, sizept, TEA_TPROTO);
    pt->name = tea_str_new(ls->T, name, len);
    pt->max_slots = (uint8_t)max_slots;
    pt->numparams = (uint8_t)numparams;
    pt->numopts = (uint8_t)numopts;
    pt->flags = (uint8_t)flags;
    pt->sizeuv = (uint8_t)sizeuv;
    pt->sizebc = sizebc;
    pt->k = (TValue*)((char*)pt + ofsk);
    pt->uv = (uint16_t*)((char*)pt + ofsuv);
    pt->sizek = sizek;
    pt->sizept = sizept;

    /* Read bytecode instructions and upvalue indexes */
    bcread_bytecode(ls, pt, sizebc);
    bcread_uv(ls, pt, sizeuv);

    /* Read constants */
    bcread_kgc(ls, pt, sizek);
    
    /* Read and initialize debug info */
    pt->firstline = firstline;
    pt->numline = numline;
    if(sizedbg)
    {
        pt->lineinfo = (char*)pt + ofsdbg;
        bcread_dbg(ls, pt, sizedbg);
    }
    else
    {
        pt->lineinfo = NULL;
    }
    return pt;
}

/* Read module */
static void bcread_module(LexState* ls)
{
    tea_State* T = ls->T;
    GCmodule* mod = ls->module;
    bcread_want(ls, 5);
    size_t size = bcread_uleb128(ls);
    if(mod->size > 0)
    {
        tea_mem_freevec(T, TValue, mod->vars, mod->size);
        tea_mem_freevec(T, GCstr*, mod->varnames, mod->size);
    }
    mod->size = size;
    mod->vars = tea_mem_newvec(T, TValue, size);
    mod->varnames = tea_mem_newvec(T, GCstr*, size);
    for(int i = 0; i < size; i++)
    {
        bcread_want(ls, 5);
        size_t len = bcread_uleb128(ls);
        const char* s = (const char*)bcread_mem(ls, len);
        tea_assertT(s != NULL, "bad variable name");
        GCstr* name = tea_str_new(T, s, len);
        mod->varnames[i] = name;
        setnilV(&mod->vars[i]);
    }
}

/* Read and check header of bytecode dump */
static bool bcread_header(LexState* ls)
{
    uint32_t flags;
    bcread_want(ls, 4+5);
    if(bcread_byte(ls) != BCDUMP_HEAD2 ||
       bcread_byte(ls) != BCDUMP_HEAD3 ||
       bcread_byte(ls) != BCDUMP_HEAD4 ||
       bcread_byte(ls) != BCDUMP_VERSION) return false;
    bcread_flags(ls) = flags = bcread_uleb128(ls);
    return true;
}

/* Read a bytecode dump */
GCproto* tea_bcread(LexState* ls)
{
    tea_State* T = ls->T;
    tea_assertLS(ls->c == BCDUMP_HEAD1, "bad bytecode header");
    bcread_savetop(T, ls, T->top);
    tea_buf_reset(&ls->sb);
    /* Check for a valid bytecode dump header */
    if(!bcread_header(ls))
        bcread_error(ls, TEA_ERR_BCFMT);
    bcread_module(ls);

    /* Process all functions in the bytecode dump */
    while(true)
    {
        GCproto* pt;
        size_t len;
        const char* startp;

        /* Read length */
        if(ls->p < ls->pe && ls->p[0] == 0)
        {
            /* Shortcut EOF */
            ls->p++;
            break;
        }
        bcread_want(ls, 5);
        len = bcread_uleb128(ls);
        if(!len) break; /* EOF */
        bcread_need(ls, len);
        startp = ls->p;

        pt = bcread_proto(ls);
        if(ls->p != startp + len)
        {
            bcread_error(ls, TEA_ERR_BCBAD);
        }
        setprotoV(T, T->top, pt);
        T->top++;
    }
    if((ls->pe != ls->p && !ls->endmark) || T->top - 1 != bcread_oldtop(T, ls))
        bcread_error(ls, TEA_ERR_BCBAD);
    /* Pop off last prototype */
    T->top--;
    return protoV(T->top);
}