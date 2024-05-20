/*
** tea_strfmt.c
** String formatting
**
** Major portions taken verbatim or adapted from LuaJIT by Mike Pall
*/

#define tea_strfmt_c
#define TEA_CORE

#include "tea_arch.h"

#include "tea_strfmt.h"
#include "tea_buf.h"
#include "tea_char.h"
#include "tea_err.h"
#include "tea_state.h"
#include "tea_vm.h"
#include "tea_tab.h"
#include "tea_lib.h"

/* -- Format parser ------------------------------------------------------- */

static const uint8_t strfmt_options[('x' - 'A') + 1] = {
    STRFMT_A, 0, 0, 0, STRFMT_E, STRFMT_F, STRFMT_G, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, STRFMT_X, 0, 0,
    0, 0, 0, 0, 0, 0,
    STRFMT_A, 0, STRFMT_C, STRFMT_D, STRFMT_E, STRFMT_F, STRFMT_G, 0, STRFMT_I, 0, 0, 0, 0,
    0, STRFMT_O, STRFMT_P, STRFMT_Q, 0, STRFMT_S, 0, STRFMT_U, 0, 0, STRFMT_X
};

static SFormat strfmt_parse(FormatState* fs)
{
    const uint8_t* p = fs->p, *e = fs->e;
    fs->str = (const char*)p;
    for(; p < e; p++)
    {
        /* Escape char? */
        if(*p == '%')
        {
            /* '%%'? */
            if(p[1] == '%')
            {
                fs->p = ++p+1;
                goto retlist;
            }
            else
            {
                SFormat sf = 0;
                uint32_t c;
                if(p != (const uint8_t*)fs->str)
                    break;
                for(p++; (uint32_t)*p - ' ' <= (uint32_t)('0' - ' '); p++)
                {
                    /* Parse flags */
                    if(*p == '-') sf |= STRFMT_F_LEFT;
                    else if(*p == '+') sf |= STRFMT_F_PLUS;
                    else if(*p == '0') sf |= STRFMT_F_ZERO;
                    else if(*p == ' ') sf |= STRFMT_F_SPACE;
                    else if(*p == '#') sf |= STRFMT_F_PLUS;
                    else break;
                }
                if((uint32_t)*p - '0' < 10)
                {
                    /* Parse width */
                    uint32_t width = (uint32_t)*p++ - '0';
                    if((uint32_t)*p - '0' < 10)
                        width = (uint32_t)*p++ - '0' + width * 10;
                    sf |= (width << STRFMT_SH_WIDTH);
                }
                if(*p == '.')
                {
                    /* Parse precision */
                    uint32_t prec = 0;
                    p++;
                    if((uint32_t)*p - '0' < 10)
                    {
                        prec = (uint32_t)*p++ - '0';
                        if((uint32_t)*p - '0' < 10)
                            prec = (uint32_t)*p++ - '0' + prec * 10;
                    }
                    sf |= ((prec + 1) << STRFMT_SH_PREC);
                }
                /* Parse conversion */
                c = (uint32_t)*p - 'A';
                if(TEA_LIKELY(c <= (uint32_t)('x' - 'A')))
                {
                    uint32_t sx = strfmt_options[c];
                    if(sx)
                    {
                        fs->p = p + 1;
                        return (sf | sx | ((c & 0x20) ? 0 : STRFMT_F_UPPER));
                    }
                }
                /* Return error location */
                if(*p >= 32) p++;
                fs->len = (size_t)(p - (const uint8_t*)fs->str);
                fs->p = fs->e;
                return STRFMT_ERR;
            }
        }
    }
    fs->p = p;
retlist:
    fs->len = (size_t)(p - (const uint8_t*)fs->str);
    return fs->len ? STRFMT_LIT : STRFMT_EOF;
}

/* -- Raw conversions ----------------------------------------------------- */

#define WINT_R(x, sh, sc) \
    { uint32_t d = (x * (((1 << sh) + sc - 1) / sc)) >> sh; x -= d * sc; *p++ = (char)('0' + d); }

/* Write integer to buffer */
char* tea_strfmt_wint(char* p, int32_t k)
{
    uint32_t u = (uint32_t)k;
    if(k < 0) { u = (uint32_t)-k; *p++ = '-'; }
    if(u < 10000)
    {
        if(u < 10) goto dig1;
        if(u < 100) goto dig2;
        if(u < 1000) goto dig3;
    } 
    else
    {
        uint32_t v = u / 10000; u -= v * 10000;
        if(v < 10000) 
        {
            if(v < 10) goto dig5;
            if(v < 100) goto dig6;
            if(v < 1000) goto dig7;
        } 
        else
        {
            uint32_t w = v / 10000; v -= w * 10000;
            if(w >= 10) WINT_R(w, 10, 10)
            *p++ = (char)('0'+w);
        }
        WINT_R(v, 23, 1000)
        dig7: WINT_R(v, 12, 100)
        dig6: WINT_R(v, 10, 10)
        dig5: *p++ = (char)('0'+v);
    }
    WINT_R(u, 23, 1000)
    dig3: WINT_R(u, 12, 100)
    dig2: WINT_R(u, 10, 10)
    dig1: *p++ = (char)('0'+u);
    return p;
}
#undef WINT_R

/* Write pointer to buffer */
static char* strfmt_wptr(char* p, const void* v)
{
    ptrdiff_t x = (ptrdiff_t)v;
    size_t i, n = STRFMT_MAXBUF_PTR;
    if(x == 0)
    {
        *p++ = 'N'; *p++ = 'U'; *p++ = 'L'; *p++ = 'L';
        return p;
    }

#if TEA_64
    /* Shorten output for 64 bit pointers */
    n = 2 + 2 * 4 + ((x >> 32) ? 2 + 2 * (tea_fls((uint32_t)(x >> 32)) >> 3) : 0);
#endif

    p[0] = '0';
    p[1] = 'x';
    for(i = n - 1; i >= 2; i--, x >>= 4)
        p[i] = "0123456789abcdef"[(x & 15)];
    return p + n;
}

/* Return string or write number to tmp buffer and return pointer to start */
const char* tea_strfmt_wstrnum(tea_State* T, cTValue* o, int* len)
{
    SBuf* sb;
    if(tvisstr(o))
    {
        GCstr* str = strV(o);
        *len = str->len;
        return str_data(str);
    }
    else if(tvisnumber(o))
    {
        sb = tea_strfmt_putfnum(T, tea_buf_tmp_(T), STRFMT_G14, numberV(o));
    }
    else
    {
        return NULL;
    }
    *len = sbuf_len(sb);
    return sb->b;
}

/* -- Unformatted conversions to buffer ----------------------------------- */

/* Add integer to buffer */
static SBuf* strfmt_putint(tea_State* T, SBuf* sb, int32_t k)
{
    sb->w = tea_strfmt_wint(tea_buf_more(T, sb, STRFMT_MAXBUF_INT), k);
    return sb;
}

/* Add pointer to buffer */
static SBuf* strfmt_putptr(tea_State* T, SBuf* sb, const void* v)
{
    sb->w = strfmt_wptr(tea_buf_more(T, sb, STRFMT_MAXBUF_PTR), v);
    return sb;
}

/* Add quoted string to buffer */
static SBuf* strfmt_putquotedlen(tea_State* T, SBuf* sb, const char* s, int len)
{
    tea_buf_putb(T, sb, '"');
    while(len--)
    {
        uint32_t c = (uint32_t)(uint8_t)*s++;
        char* w = tea_buf_more(T, sb, 4);
        if(c == '"' || c == '\\' || c == '\n')
        {
            *w++ = '\\';
        }
        else if(tea_char_iscntrl(c))
        {  /* This can only be 0-31 or 127 */
            uint32_t d;
            *w++ = '\\';
            if(c >= 100 || tea_char_isdigit((uint8_t)*s))
            {
                *w++ = (char)('0' + (c >= 100)); if (c >= 100) c -= 100;
                goto tens;
            }
            else if (c >= 10)
            {
            tens:
                d = (c * 205) >> 11; c -= d * 10; *w++ = (char)('0' + d);
            }
            c += '0';
        }
        *w++ = (char)c;
        sb->w = w;
    }
    tea_buf_putb(T, sb, '"');
    return sb;
}

/* -- Formatted conversions to buffer ---------------------------------------------- */

/* Add formatted char to buffer */
static SBuf* strfmt_putfchar(tea_State* T, SBuf* sb, SFormat sf, int32_t c)
{
    size_t width = STRFMT_WIDTH(sf);
    char *w = tea_buf_more(T, sb, width > 1 ? width : 1);
    if((sf & STRFMT_F_LEFT)) *w++ = (char)c;
    while(width-- > 1) *w++ = ' ';
    if(!(sf & STRFMT_F_LEFT)) *w++ = (char)c;
    sb->w = w;
    return sb;
}

/* Add formatted string to buffer */
static SBuf* strfmt_putfstrlen(tea_State* T, SBuf* sb, SFormat sf, const char* s, int len)
{
    size_t width = STRFMT_WIDTH(sf);
    char* w;
    if(len > STRFMT_PREC(sf)) len = STRFMT_PREC(sf);
    w = tea_buf_more(T, sb, width > len ? width : len);
    if((sf & STRFMT_F_LEFT)) w = tea_buf_wmem(w, s, len);
    while(width-- > len) *w++ = ' ';
    if(!(sf & STRFMT_F_LEFT)) w = tea_buf_wmem(w, s, len);
    sb->w = w;
    return sb;
}

/* Add formatted signed/unsigned integer to buffer */
static SBuf* strfmt_putfxint(tea_State* T, SBuf* sb, SFormat sf, uint64_t k)
{
    char buf[STRFMT_MAXBUF_XINT], *q = buf + sizeof(buf), *w;
#ifdef TEA_USE_ASSERT
    char* ws;
#endif
    size_t prefix = 0, len, prec, pprec, width, need;

    /* Figure out signed prefixes */
    if(STRFMT_TYPE(sf) == STRFMT_INT)
    {
        if((int64_t)k < 0)
        {
            k = (uint64_t)-(int64_t)k;
            prefix = 256 + '-';
        }
        else if((sf & STRFMT_F_PLUS))
        {
            prefix = 256 + '+';
        }
        else if((sf & STRFMT_F_SPACE)) 
        {
            prefix = 256 + ' ';
        }
    }

    /* Convert number and store to fixed-size buffer in reverse order */
    prec = STRFMT_PREC(sf);
    if((int32_t)prec >= 0) sf &= ~STRFMT_F_ZERO;
    /* Special-case zero argument */
    if(k == 0)
    {
        if(prec != 0 ||
        (sf & (STRFMT_T_OCT|STRFMT_F_ALT)) == (STRFMT_T_OCT|STRFMT_F_ALT))
        *--q = '0';
    } 
    else if(!(sf & (STRFMT_T_HEX|STRFMT_T_OCT)))
    {
        /* Decimal */
        uint32_t k2;
        while((k >> 32)) { *--q = (char)('0' + k % 10); k /= 10; }
        k2 = (uint32_t)k;
        do { *--q = (char)('0' + k2 % 10); k2 /= 10; } while(k2);
    } 
    else if((sf & STRFMT_T_HEX))
    {
        /* Hex */
        const char* hexdig = (sf & STRFMT_F_UPPER) ? "0123456789ABCDEF" :
                            "0123456789abcdef";
        do { *--q = hexdig[(k & 15)]; k >>= 4; } while(k);
        if((sf & STRFMT_F_ALT)) prefix = 512 + ((sf & STRFMT_F_UPPER) ? 'X' : 'x');
    } 
    else
    {
        /* Octal */
        do { *--q = (char)('0' + (uint32_t)(k & 7)); k >>= 3; } while(k);
        if((sf & STRFMT_F_ALT)) *--q = '0';
    }

    /* Calculate sizes */
    len = (size_t)(buf + sizeof(buf) - q);
    if((int32_t)len >= (int32_t)prec) prec = len;
    width = STRFMT_WIDTH(sf);
    pprec = prec + (prefix >> 8);
    need = width > pprec ? width : pprec;
    w = tea_buf_more(T, sb, need);

#ifdef TEA_USE_ASSERT
    ws = w;
#endif

    /* Format number with leading/trailing whitespace and zeros */
    if((sf & (STRFMT_F_LEFT|STRFMT_F_ZERO)) == 0)
        while (width-- > pprec) *w++ = ' ';
    if(prefix)
    {
        if((char)prefix >= 'X') *w++ = '0';
        *w++ = (char)prefix;
    }
    if((sf & (STRFMT_F_LEFT|STRFMT_F_ZERO)) == STRFMT_F_ZERO)
        while(width-- > pprec) *w++ = '0';
    while(prec-- > len) *w++ = '0';
    while(q < buf + sizeof(buf)) *w++ = *q++;  /* Add number itself */
    if((sf & STRFMT_F_LEFT))
        while(width-- > pprec) *w++ = ' ';

    tea_assertT(need == (size_t)(w - ws), "miscalculated format size");
    sb->w = w;
    return sb;
}

/* Add number formatted as signed integer to buffer */
static SBuf* strfmt_putfnum_int(tea_State* T, SBuf* sb, SFormat sf, double n)
{
    int64_t k = (int64_t)n;
    if(checki32(k) && sf == STRFMT_INT)
        return strfmt_putint(T, sb, (int32_t)k);  /* Shortcut for plain %d */
    else
        return strfmt_putfxint(T, sb, sf, (uint64_t)k);
}

/* Add number formatted as unsigned integer to buffer */
static SBuf* strfmt_putfnum_uint(tea_State* T, SBuf* sb, SFormat sf, double n)
{
    int64_t k;
    if(n >= 9223372036854775808.0)
        k = (int64_t)(n - 18446744073709551616.0);
    else
        k = (int64_t)n;
    return strfmt_putfxint(T, sb, sf, (uint64_t)k);
}

/* Format stack arguments to buffer */
int tea_strfmt_putarg(tea_State* T, SBuf* sb, int arg, int retry)
{
    int narg = (int)(T->top - T->base);
    GCstr* fmt = tea_lib_checkstr(T, arg);
    FormatState fs;
    SFormat sf;
    tea_strfmt_init(&fs, str_data(fmt), fmt->len);
    while((sf = strfmt_parse(&fs)) != STRFMT_EOF)
    {
        if(sf == STRFMT_LIT)
        {
            tea_buf_putmem(T, sb, fs.str, fs.len);
        }
        else if(sf == STRFMT_ERR)
        {
            tea_err_run(T, TEA_ERR_STRFMT, str_data(tea_str_new(T, fs.str, fs.len)));
        }
        else
        {
            TValue* o = &T->base[++arg];
            if(arg >= narg)
                tea_err_run(T, TEA_ERR_NOVAL);
            switch(STRFMT_TYPE(sf))
            {
                case STRFMT_INT:
                    strfmt_putfnum_int(T, sb, sf, tea_lib_checknumber(T, arg));
                    break;
                case STRFMT_UINT:
                    strfmt_putfnum_uint(T, sb, sf, tea_lib_checknumber(T, arg));
                    break;
                case STRFMT_NUM:
                    tea_strfmt_putfnum(T, sb, sf, tea_lib_checknumber(T, arg));
                    break;
                case STRFMT_STR:
                {
                    int len;
                    const char* s;
                    TValue* mo;
                    if(tvisinstance(o) && retry >= 0 &&
                    (mo = tea_tab_get(&instanceV(o)->klass->methods, mmname_str(T, MM_TOSTRING))) != NULL)
                    {
                        /* Call tostring method once */
                        setinstanceV(T, T->top++, instanceV(o));
                        copyTV(T, T->top++, mo);
                        copyTV(T, T->top++, o);
                        tea_vm_call(T, mo, 0);
                        o = &T->base[arg];  /* Stack may have been reallocated */
                        TValue* tv = --T->top;
                        if(!tvisstr(tv))
                            tea_err_run(T, TEA_ERR_TOSTR);
                        copyTV(T, o, tv); /* Replace inline for retry */
                        if(retry < 2)
                        {
                            /* Global buffer may have been overwritten */
                            retry = 1;
                            break;
                        }
                    }
                    if(TEA_LIKELY(tvisstr(o)))
                    {
                        GCstr* str = strV(o);
                        len = str->len;
                        s = str_data(str);
                    }
                    else
                    {
                        SBuf* strbuf = &T->strbuf;
                        tea_buf_reset(strbuf);
                        tea_strfmt_obj(T, strbuf, o, 0);
                        GCstr* str = tea_buf_str(T, strbuf);
                        len = str->len;
                        s = str_data(str);
                    }
                    if((sf & STRFMT_T_QUOTED))
                        strfmt_putquotedlen(T, sb, s, len);  /* No formatting */
                    else
	                    strfmt_putfstrlen(T, sb, sf, s, len);
                    break;
                }
                case STRFMT_CHAR:
                    strfmt_putfchar(T, sb, sf, tea_lib_checknumber(T, arg));
                    break;
                case STRFMT_PTR:
                    strfmt_putptr(T, sb, tea_obj_pointer(o));
	                break;
                default:
                    tea_assertT(T, "bad string format type");
                    break;
            }
        }
    }
    return retry;
}

/* -- Conversions to strings ---------------------------------------------- */

static void strfmt_list(tea_State* T, SBuf* sb, GClist* list, int depth)
{
    if(list->count == 0)
    {
        tea_buf_putlit(T, sb, "[]");
        return;
    }

    if(depth > TEA_MAX_TOSTR)
    {
        tea_buf_putlit(T, sb, "[...]");
        return;
    }
    
    tea_buf_putlit(T, sb, "[");

    TValue self;
    setlistV(T, &self, list);
    for(int i = 0; i < list->count; i++)
    {
        TValue* o = list_slot(list, i);

        if(tea_obj_rawequal(o, &self))
        {
            tea_buf_putlit(T, sb, "[...]");
        }
        else
        {
            tea_strfmt_obj(T, sb, o, depth);
        }

        if(i != list->count - 1)
        {
            tea_buf_putlit(T, sb, ", ");
        }
    }
    tea_buf_putlit(T, sb, "]");
}

static void strfmt_map(tea_State* T, SBuf* sb, GCmap* map, int depth)
{
    if(map->count == 0)
    {
        tea_buf_putlit(T, sb, "{}");
        return;
    }
    
    if(depth > TEA_MAX_TOSTR)
    {
        tea_buf_putlit(T, sb, "{...}");
        return;
    }

    int count = 0;

    tea_buf_putlit(T, sb, "{");

    TValue self;
    setmapV(T, &self, map);
    for(int i = 0; i < map->size; i++)
    {
        MapEntry* entry = &map->entries[i];
        if(entry->empty)
        {
            continue;
        }
        TValue* key = &entry->key;
        TValue* value = &entry->value;

        count++;

        if(tea_obj_rawequal(key, &self))
        {
            tea_buf_putlit(T, sb, "{...}");
        }
        else
        {
            if(!tvisstr(key))
            {
                tea_buf_putlit(T, sb, "[");
                tea_strfmt_obj(T, sb, key, depth);
                tea_buf_putlit(T, sb, "] = ");
            }
            else
            {
                tea_strfmt_obj(T, sb, key, depth);
                tea_buf_putlit(T, sb, " = ");
            }
        }

        if(tea_obj_rawequal(value, &self))
        {
            tea_buf_putlit(T, sb, "{...}");
        }
        else
        {
            tea_strfmt_obj(T, sb, value, depth);
        }

        if(count != map->count)
        {
            tea_buf_putlit(T, sb, ", ");
        }
    }
    tea_buf_putlit(T, sb, "}");
}

static void strfmt_func(tea_State* T, SBuf* sb, GCproto* proto)
{
    if(proto->type > PROTO_FUNCTION)
    {
        tea_buf_putmem(T, sb, str_data(proto->name), proto->name->len);
    }
    else
    {
        tea_buf_putlit(T, sb, "<function>");
    }
}

/* Conversion of object to string */
void tea_strfmt_obj(tea_State* T, SBuf* sb, cTValue* o, int depth)
{
    if(depth > TEA_MAX_TOSTR)
    {
        tea_buf_putlit(T, sb, "...");
        return;
    }
    depth++;
    switch(itype(o))
    {
        case TEA_TNIL:
            tea_buf_putlit(T, sb, "nil");
            break;
        case TEA_TBOOL:
        {
            if(boolV(o))
                tea_buf_putlit(T, sb, "true");
            else
                tea_buf_putlit(T, sb, "false");
            break;
        }
        case TEA_TNUMBER:
            tea_strfmt_putfnum(T, sb, STRFMT_G14, numberV(o));
            break;
        case TEA_TPOINTER:
            tea_buf_putlit(T, sb, "pointer");
            break;
        case TEA_TMETHOD:
            tea_buf_putlit(T, sb, "<method>");
            break;
        case TEA_TPROTO:
            strfmt_func(T, sb, protoV(o));
            break;
        case TEA_TFUNC:
        {
            GCfunc* func = funcV(o);
            if(iscfunc(func))
                tea_buf_putlit(T, sb, "<function>");
            else
                strfmt_func(T, sb, func->t.proto);
            break;
        }
        case TEA_TLIST:
            strfmt_list(T, sb, listV(o), depth);
            break;
        case TEA_TMAP:
            strfmt_map(T, sb, mapV(o), depth);
            break;
        case TEA_TRANGE:
        {
            GCrange* range = rangeV(o);
            tea_strfmt_putfnum(T, sb, STRFMT_G14, range->start);
            tea_buf_putlit(T, sb, "..");
            tea_strfmt_putfnum(T, sb, STRFMT_G14, range->end);
            tea_buf_putlit(T, sb, "..");
            tea_strfmt_putfnum(T, sb, STRFMT_G14, range->step);
            break;
        }
        case TEA_TMODULE:
        {
            GCmodule* module = moduleV(o);
            tea_buf_putlit(T, sb, "<");
            tea_buf_putmem(T, sb, str_data(module->name), module->name->len);
            tea_buf_putlit(T, sb, " module>");
            break;
        }
        case TEA_TCLASS:
        {
            GCclass* klass = classV(o);
            tea_buf_putlit(T, sb, "<");
            tea_buf_putmem(T, sb, str_data(klass->name), klass->name->len);
            tea_buf_putlit(T, sb, " class>");
            break;
        }
        case TEA_TINSTANCE:
        {
            GCstr* name = instanceV(o)->klass->name;
            tea_buf_putlit(T, sb, "<");
            tea_buf_putmem(T, sb, str_data(name), name->len);
            tea_buf_putlit(T, sb, ">");
            break;
        }
        case TEA_TSTRING:
            tea_buf_putstr(T, sb, strV(o));
            break;
        case TEA_TUPVALUE:
            tea_buf_putlit(T, sb, "<upvalue>");
            break;
        case TEA_TUSERDATA:
            tea_buf_putlit(T, sb, "<userdata>");
            break;
        default:
            tea_assertT(T, "unknown type");
            break;
    }
}

/* -- Internal string formatting ------------------------------------------ */

/*
** These functions are only used for tea_push_fstring(), tea_pushvfstring()
** and for internal string formatting (e.g. error messages). Caveat: unlike
** String.format(), only a limited subset of formats and flags are supported!
*/

/* Push formatted message as a string object to Teascript stack. va_list variant */
const char* tea_strfmt_pushvf(tea_State* T, const char* fmt, va_list argp)
{
    SBuf* sb = tea_buf_tmp_(T);
    FormatState fs;
    SFormat sf;
    GCstr* str;
    tea_strfmt_init(&fs, fmt, strlen(fmt));
    while((sf = strfmt_parse(&fs)) != STRFMT_EOF)
    {
        switch(STRFMT_TYPE(sf))
        {
            case STRFMT_LIT:
                tea_buf_putmem(T, sb, fs.str, fs.len);
                break;
            case STRFMT_INT:
                strfmt_putfxint(T, sb, sf, va_arg(argp, int32_t));
                break;
            case STRFMT_UINT:
                strfmt_putfxint(T, sb, sf, va_arg(argp, uint32_t));
                break;
            case STRFMT_NUM:
                tea_strfmt_putfnum(T, sb, STRFMT_G14, va_arg(argp, double));
                break;
            case STRFMT_STR:
            {
                const char* s = va_arg(argp, char*);
                if(s == NULL) s = "(nil)";
                tea_buf_putmem(T, sb, s, strlen(s));
                break;
            }
            case STRFMT_CHAR:
                tea_buf_putb(T, sb, va_arg(argp, int));
                break;
            case STRFMT_PTR:
                strfmt_putptr(T, sb, va_arg(argp, void*));
                break;
            case STRFMT_ERR:
            default:
                tea_buf_putb(T, sb, '?');
                tea_assertT(0, "bad string format near offset %d", fs.len);
                break;
        }
    }
    str = tea_buf_str(T, sb);
    setstrV(T, T->top, str);
    incr_top(T);
    return str_data(str);
}

/* Push formatted message as a string object to Teascript stack. Vararg variant */
const char* tea_strfmt_pushf(tea_State* T, const char* fmt, ...)
{
    const char* msg;
    va_list argp;
    va_start(argp, fmt);
    msg = tea_strfmt_pushvf(T, fmt, argp);
    va_end(argp);
    return msg;
}