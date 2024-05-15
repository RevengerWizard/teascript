/*
** tea_bcread.h
** Teascript bytecode reader
*/

#include <stdlib.h>

#define tea_bcread_c
#define TEA_CORE

#include "tea_def.h"
#include "tea_bcdump.h"
#include "tea_err.h"
#include "tea_func.h"
#include "tea_vm.h"

/* -- Input buffer handling -------------------------------------------------- */

/* Throw reader error */
static TEA_NOINLINE void bcread_error(Lexer* lex, const char* msg)
{
    tea_State* T = lex->T;
    fputs(msg, stderr);
    tea_err_throw(T, TEA_ERROR_SYNTAX);
}

/* Refill buffer */
static TEA_NOINLINE void bcread_fill(Lexer* lex, size_t len, bool need)
{
    tea_assertLS(len != 0, "empty refill");
    if(len > TEA_MAX_BUF || lex->c < 0)
        bcread_error(lex, "Malformed bytecode");
    do
    {
        const char* buf;
        size_t size;
        char* p = lex->sb.b;
        size_t n = (size_t)(lex->pe - lex->p);

        /* Copy remainder to buffer */
        if(n) 
        {
            /* Move down in buffer */
            if(sbuf_len(&lex->sb))
            {
                tea_assertLS(lex->pe == lex->sb.w, "bad buffer pointer");
                if(lex->p != p) 
                    memmove(p, lex->p, n);
            }
            else 
            {
                /* Copy from buffer provided by reader */
                p = tea_buf_need(lex->T, &lex->sb, len);
                memcpy(p, lex->p, n);
            }
            lex->p = p;
            lex->pe = p + n;
        }

        lex->sb.w = p + n;
        buf = lex->reader(lex->T, lex->data, &size);  /* Get more data from reader */
        if(buf == NULL || size == 0)
        {
            if(need) 
                bcread_error(lex, "Malformed bytecode");
            lex->c = -1;
            break;
        }
        
        if(n) 
        {
            /* Append to buffer */
            n += size;
            p = tea_buf_need(lex->T, &lex->sb, n < len ? len : n);
            memcpy(lex->sb.w, buf, size);
            lex->sb.w = p + n;
            lex->p = p;
            lex->pe = p + n;
        } 
        else 
        {
            /* Return buffer provided by reader */
            lex->p = buf;
            lex->pe = buf + size;
        }
    } 
    while((size_t)(lex->pe - lex->p) < len);
}

/* Need a certain number of bytes */
static TEA_AINLINE void bcread_need(Lexer* lex, size_t len)
{
    if(TEA_UNLIKELY((size_t)(lex->pe - lex->p) < len))
        bcread_fill(lex, len, true);
}

/* Want to read up to a certain number of bytes, but may need less */
static TEA_AINLINE void bcread_want(Lexer* lex, size_t len)
{
    if(TEA_UNLIKELY((size_t)(lex->pe - lex->p) < len))
        bcread_fill(lex, len, false);
}

/* Return memory block from buffer */
static TEA_AINLINE uint8_t* bcread_mem(Lexer* lex, size_t len)
{
    uint8_t* p = (uint8_t*)lex->p;
    lex->p += len;
    tea_assertLS(lex->p <= lex->pe, "buffer read overflow");
    return p;
}

/* Copy memory block from buffer */
static void bcread_block(Lexer* lex, void* q, size_t len)
{
    memcpy(q, bcread_mem(lex, len), len);
}

/* Read byte from buffer */
static TEA_AINLINE uint32_t bcread_byte(Lexer* lex)
{
    tea_assertLS(lex->p < lex->pe, "buffer read overflow");
    return (uint32_t)(uint8_t)*lex->p++;
}

/* Read ULEB128 from buffer */
static TEA_AINLINE uint32_t bcread_uleb128(Lexer* lex)
{
    uint32_t v = tea_buf_ruleb128(&lex->p);
    tea_assertLS(lex->p <= lex->pe, "buffer read overflow");
    return v;
}

/* Read top 32 bits of 33 bit ULEB128 value from buffer */
static uint32_t bcread_uleb128_33(Lexer* lex)
{
    const uint8_t* p = (const uint8_t*)lex->p;
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
    lex->p = (char*)p;
    tea_assertLS(lex->p <= lex->pe, "buffer read overflow");
    return v;
}

/* -- Bytecode reader -------------------------------------------------- */

/* Read number from constants */
static double bcread_knum(Lexer* lex)
{
    NumberBits x;
    
    uint32_t lo = bcread_uleb128_33(lex);
    x.u32.lo = lo;
    x.u32.hi = bcread_uleb128(lex);

    return x.n;
}

/* Read GC constants from function prototype */
static void bcread_kgc(Lexer* lex, GCproto* pt, size_t count)
{
    pt->k = tea_mem_newvec(lex->T, TValue, count);
    pt->k_count = count;
    pt->k_size = count;

    for(int i = 0; i < count; i++)
    {
        size_t type = bcread_uleb128(lex);
        if(type >= BCDUMP_KGC_STR)
        {
            int len = type - BCDUMP_KGC_STR;
            const char* p = (const char*)bcread_mem(lex, len);
            GCstr* str = tea_str_new(lex->T, p, len);
            setstrV(lex->T, proto_kgc(pt, i), str);
        }
        else if(type == BCDUMP_KGC_NUM)
        {
            double num = bcread_knum(lex);
            setnumberV(proto_kgc(pt, i), num);
        }
        else
        {
            tea_assertLS(type == BCDUMP_KGC_FUNC, "bad constant type %d", type);
            lex->T->top--;
            TValue* v = lex->T->top;
            copyTV(lex->T, proto_kgc(pt, i), v);
        }
    }
}

/* Read bytecode instructions */
static void bcread_bytecode(Lexer* lex, GCproto* pt, size_t count)
{
    pt->bc = tea_mem_newvec(lex->T, BCIns, count);
    pt->bc_count = count;
    pt->bc_size = count;
    bcread_block(lex, pt->bc, pt->bc_count);
}

/* Read prototype */
static GCproto* bcread_proto(Lexer* lex)
{
    GCproto* pt;
    uint8_t arity, arity_optional, variadic, max_slots, upvalue_count, type;
    int count, k_count;

    /* Read prototype name */
    size_t len = bcread_uleb128(lex);
    const char* name = (const char*)bcread_mem(lex, len);

    /* Read prototype header */
    arity = bcread_byte(lex);
    arity_optional = bcread_byte(lex);
    variadic = bcread_byte(lex);
    max_slots = bcread_byte(lex);
    upvalue_count = bcread_byte(lex);
    type = bcread_byte(lex);

    count = bcread_uleb128(lex);
    k_count = bcread_uleb128(lex);

    /* Allocate prototype and initialize its fields */
    pt = tea_func_newproto(lex->T, type, max_slots);
    pt->name = tea_str_new(lex->T, name, len);
    pt->arity = arity;
    pt->arity_optional = arity_optional;
    pt->variadic = variadic;
    pt->upvalue_count = upvalue_count;
    pt->bc_count = count;
    pt->k_count = k_count;

    /* Read bytecode instructions */
    bcread_bytecode(lex, pt, count);

    /* Read constants */
    bcread_kgc(lex, pt, k_count);

    return pt;
}

/* Read and check header of bytecode dump */
static bool bcread_header(Lexer* lex)
{
    bcread_want(lex, 4);
    if(bcread_byte(lex) != BCDUMP_HEAD2 ||
       bcread_byte(lex) != BCDUMP_HEAD3 ||
       bcread_byte(lex) != BCDUMP_HEAD4 ||
       bcread_byte(lex) != BCDUMP_VERSION) return false;
    return true;
}

/* Read a bytecode dump */
GCproto* tea_bcread(Lexer* lex)
{
    tea_State* T = lex->T;
    tea_assertLS(lex->c == BCDUMP_HEAD1, "bad bytecode header");
    tea_buf_reset(&lex->sb);
    /* Check for a valid bytecode dump header */
    if(!bcread_header(lex))
        bcread_error(lex, "Invalid bytecode header");

    /* Process all functions in the bytecode dump */
    while(true)
    {
        GCproto* pt;
        size_t len;
        const char* startp;

        /* Read length */
        if(lex->p < lex->pe && lex->p[0] == 0)
        {
            /* Shortcut EOF */
            lex->p++;
            break;
        }
        bcread_want(lex, 5);
        len = bcread_uleb128(lex);
        if(!len) break; /* EOF */
        bcread_need(lex, len);
        startp = lex->p;

        pt = bcread_proto(lex);
        if(lex->p != startp + len)
        {
            bcread_error(lex, "Malformed bytecode");
        }
        setprotoV(T, T->top, pt);
        T->top++;
    }
    /* Pop off last prototype */
    T->top--;
    return protoV(T->top);
}