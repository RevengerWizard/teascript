/*
** tea_buf.c
** Buffer handling
*/

#define tea_buf_c
#define TEA_CORE

#include "tea_buf.h"
#include "tea_state.h"
#include "tea_gc.h"

/* -- Buffer management -------------------------------------------------- */

static void buf_grow(tea_State* T, SBuf* sb, size_t size)
{
    size_t old_size = sbuf_size(sb), len = sbuf_len(sb), new_size = old_size;

    if(new_size < TEA_MIN_BUF) new_size = TEA_MIN_BUF;
    while(new_size < size) new_size += new_size;

    char* b = (char*)tea_mem_realloc(T, sb->b, old_size, new_size);

    /* Adjust buffer pointers */
    sb->b = b;
    sb->w = b + len;
    sb->e = b + new_size;
}

char* tea_buf_need2(tea_State* T, SBuf* sb, size_t size)
{
    buf_grow(T, sb, size);
    return sb->b;
}

char* tea_buf_more2(tea_State* T, SBuf* sb, size_t size)
{
    size_t len = sbuf_len(sb);
    buf_grow(T, sb, len + size);
    return sb->w;
}

SBuf* tea_buf_putmem(tea_State* T, SBuf* sb, const void* q, size_t len)
{
    char* w = tea_buf_more(T, sb, len);
    w = tea_buf_wmem(w, q, len);
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
    memcpy(buf, s1->chars, len1);
    memcpy(buf + len1, s2->chars, len2);
    return tea_str_copy(T, buf, len1 + len2);
}