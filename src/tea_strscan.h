/*
** tea_strscan.h
** String scanning
*/

#ifndef _TEA_STRSCAN_H
#define _TEA_STRSCAN_H

#include "tea_obj.h"

/* Options for accepted/returned formats */
#define STRSCAN_OPT_TONUM	0x02  /* Always convert to double */

/* Returned format */
typedef enum
{
    STRSCAN_ERROR,
    STRSCAN_NUM, STRSCAN_INT
} StrScanFmt;

TEA_FUNC StrScanFmt tea_strscan_scan(const uint8_t* p, size_t len, TValue* o, uint32_t opt);
TEA_FUNC bool tea_strscan_num(GCstr* str, TValue* o);

#endif