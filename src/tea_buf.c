/*
** tea_buf.c
** Buffer handling
*/

#define tea_buf_c
#define TEA_CORE

#include "tea_buf.h"
#include "tea_state.h"
#include "tea_gc.h"
#include "tea_err.h"

/* -- Buffer management -------------------------------------------------- */

static void buf_grow(tea_State* T, SBuf* sb, size_t size)
{
    size_t old_size = sbuf_size(sb), len = sbuf_len(sb), new_size = old_size;

    if(new_size < TEA_MIN_SBUF) new_size = TEA_MIN_SBUF;
    while(new_size < size) new_size += new_size;

    char* b = (char*)tea_mem_realloc(T, sb->b, old_size, new_size);

    if(sbuf_isext(sb))
    {
        SBufExt* sbx = (SBufExt*)sb;
        sbx->r = sbx->r - sb->b + b;  /* Adjust read pointer, too */
    }

    /* Adjust buffer pointers */
    sb->b = b;
    sb->w = b + len;
    sb->e = b + new_size;
}

char* tea_buf_need2(tea_State* T, SBuf* sb, size_t size)
{
    tea_assertT(size > sbuf_left(sb), "SBuf overflow");
    if(TEA_UNLIKELY(size > TEA_MAX_BUF))
        tea_err_mem(T);
    buf_grow(T, sb, size);
    return sb->b;
}

char* tea_buf_more2(tea_State* T, SBuf* sb, size_t size)
{
    if(sbuf_isext(sb))
    {
        SBufExt* sbx = (SBufExt*)sb;
        size_t len = sbufx_len(sbx);
        if(TEA_UNLIKELY(size > TEA_MAX_BUF || len + size > TEA_MAX_BUF))
            tea_err_mem(T);
        if(len + size > sbuf_size(sbx))  /* Must grow */
            buf_grow(T, (SBuf*)sbx, len + size);
        if (sbx->r != sbx->b)
        { 
            /* Compact by moving down */
            memmove(sbx->b, sbx->r, len);
            sbx->r = sbx->b;
            sbx->w = sbx->b + len;
            tea_assertT(len + size <= sbuf_size(sbx), "bad SBuf compact");
        }
    }
    else
    {
        size_t len = sbuf_len(sb);
        tea_assertT(size > sbuf_left(sb), "SBuf overflow");
        if(TEA_UNLIKELY(size > TEA_MAX_BUF || len + size > TEA_MAX_BUF))
            tea_err_mem(T);
        buf_grow(T, sb, len + size);
    }
    return sb->w;
}

void tea_buf_shrink(tea_State* T, SBuf* sb)
{
    char* b = sb->b;
    size_t old_size = (size_t)(sb->e - b);
    if(old_size > 2 * TEA_MIN_SBUF)
    {
        size_t n = (size_t)(sb->w - b);
        b = tea_mem_realloc(T, b, old_size, (old_size >> 1));
        sb->b = b;
        sb->w = b + n;
        sb->e = b + (old_size >> 1);
    }
}

SBuf* tea_buf_putmem(tea_State* T, SBuf* sb, const void* q, size_t len)
{
    char* w = tea_buf_more(T, sb, len);
    w = tea_buf_wmem(w, q, len);
    sb->w = w;
    return sb;
}

SBuf* tea_buf_putstr(tea_State* T, SBuf* sb, GCstr* str)
{
    size_t len = str->len;
    char* w = tea_buf_more(T, sb, len);
    w = tea_buf_wmem(w, str_data(str), len);
    sb->w = w;
    return sb;
}

/* Read ULEB128 from buffer */
uint32_t tea_buf_ruleb128(const char** pp)
{
    const uint8_t* w = (const uint8_t*)*pp;
    uint32_t v = *w++;
    if(TEA_UNLIKELY(v >= 0x80)) 
    {
        int sh = 0;
        v &= 0x7f;
        do { v |= ((*w & 0x7f) << (sh += 7)); } while(*w++ >= 0x80);
    }
    *pp = (const char*)w;
    return v;
}

char* tea_buf_tmp(tea_State* T, size_t size)
{
    SBuf* sb = &T->tmpbuf;
    return tea_buf_need(T, sb, size);
}

/* Concatenate two strings */
GCstr* tea_buf_cat2str(tea_State* T, GCstr* s1, GCstr* s2)
{
    size_t len1 = s1->len, len2 = s2->len;
    char* buf = tea_buf_tmp(T, len1 + len2);
    memcpy(buf, str_data(s1), len1);
    memcpy(buf + len1, str_data(s2), len2);
    return tea_str_new(T, buf, len1 + len2);
}