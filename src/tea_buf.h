/*
** tea_buf.h
** Buffer handling
*/

#ifndef _TEA_BUF_H
#define _TEA_BUF_H

#include <string.h>

#include "tea_def.h"
#include "tea_obj.h"
#include "tea_gc.h"
#include "tea_str.h"

/* Resizable string buffers */

#define sbuf_size(sb)  ((size_t)((sb)->e - (sb)->b))
#define sbuf_len(sb)   ((size_t)((sb)->w - (sb)->b))
#define sbuf_left(sb)   ((size_t)((sb)->e - (sb)->w))

/* Buffer management */
TEA_FUNC char* tea_buf_need2(tea_State* T, SBuf* sb, size_t size);
TEA_FUNC char* tea_buf_more2(tea_State* T, SBuf* sb, size_t size);
TEA_FUNC SBuf* tea_buf_putmem(tea_State* T, SBuf* sb, const void* q, size_t len);
TEA_FUNC void tea_buf_shrink(tea_State* T, SBuf* sb);
TEA_FUNC char* tea_buf_tmp(tea_State* T, size_t size);
TEA_FUNC uint32_t tea_buf_ruleb128(const char** pp);
TEA_FUNC GCstr* tea_buf_cat2str(tea_State* T, GCstr* s1, GCstr* s2);

static TEA_AINLINE void tea_buf_init(SBuf* sb)
{
    sb->w = sb->e = sb->b = NULL;
}

static TEA_AINLINE void tea_buf_reset(SBuf* sb)
{
    sb->w = sb->b;
}

static TEA_AINLINE SBuf* tea_buf_tmp_(tea_State* T)
{
    SBuf* sb = &T->tmpbuf;
    tea_buf_reset(sb);
    return sb;
}

static TEA_AINLINE char* tea_buf_need(tea_State* T, SBuf* sb, size_t size)
{
    if(TEA_UNLIKELY(size > sbuf_size(sb)))
        return tea_buf_need2(T, sb, size);
    return sb->b;
}

static TEA_AINLINE char* tea_buf_more(tea_State* T, SBuf* sb, size_t size)
{
    if(TEA_UNLIKELY(size > sbuf_left(sb)))
        return tea_buf_more2(T, sb, size);
    return sb->w;
}

static TEA_AINLINE char* tea_buf_wmem(char* p, const void* q, size_t len)
{
    return (char*)memcpy(p, q, len) + len;
}

static TEA_AINLINE void tea_buf_putb(tea_State* T, SBuf* sb, int c)
{
    char* w = tea_buf_more(T, sb, 1);
    *w++ = (char)c;
    sb->w = w;
}

static TEA_AINLINE void tea_buf_free(tea_State* T, SBuf* sb)
{
    tea_mem_freevec(T, char, sb->b, sbuf_size(sb));
}

static TEA_AINLINE GCstr* tea_buf_str(tea_State* T, SBuf* sb)
{
    return tea_str_copy(T, sb->b, sbuf_len(sb));
}

#endif