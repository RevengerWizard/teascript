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
void tea_strfmt_putarg(tea_State* T, SBuf* sb, int arg)
{
    int narg = (int)(T->top - T->base);
    tea_check_string(T, arg); GCstr* fmt = strV(T->base + arg);
    FormatState fs;
    SFormat sf;
    tea_strfmt_init(&fs, fmt->chars, fmt->len);
    while((sf = strfmt_parse(&fs)) != STRFMT_EOF)
    {
        if(sf == STRFMT_LIT)
        {
            tea_buf_putmem(T, sb, fs.str, fs.len);
        }
        else if(sf == STRFMT_ERR)
        {
            tea_err_run(T, TEA_ERR_STRFMT, tea_str_copy(T, fs.str, fs.len)->chars);
        }
        else
        {
            TValue* o = &T->base[++arg];
            if(arg >= narg)
                tea_err_run(T, TEA_ERR_NOVAL);
            switch(STRFMT_TYPE(sf))
            {
                case STRFMT_INT:
                    strfmt_putfnum_int(T, sb, sf, tea_check_number(T, arg));
                    break;
                case STRFMT_UINT:
                    strfmt_putfnum_uint(T, sb, sf, tea_check_number(T, arg));
                    break;
                case STRFMT_NUM:
                    tea_strfmt_putfnum(T, sb, sf, tea_check_number(T, arg));
                    break;
                case STRFMT_STR:
                {
                    int len;
                    const char* s;
                    if(TEA_LIKELY(tvisstr(o)))
                    {
                        GCstr* str = strV(o);
                        len = str->len;
                        s = str->chars;
                    }
                    else
                    {
                        GCstr* str = tea_strfmt_obj(T, o, 0);
                        len = str->len;
                        s = str->chars;
                        sb = tea_buf_tmp_(T);   /* Global buffer may have been overwritten */
                    }
                    if((sf & STRFMT_T_QUOTED))
                        strfmt_putquotedlen(T, sb, s, len);  /* No formatting */
                    else
	                    strfmt_putfstrlen(T, sb, sf, s, len);
                    break;
                }
                case STRFMT_CHAR:
                    strfmt_putfchar(T, sb, sf, tea_check_number(T, arg));
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
}

/* -- Conversions to strings ---------------------------------------------- */

#define TEA_MAX_TOSTR 16

static GCstr* strfmt_list(tea_State* T, GClist* list, int depth)
{
    if(list->count == 0)
        return tea_str_newlit(T, "[]");

    if(depth > TEA_MAX_TOSTR)
        return tea_str_newlit(T, "[...]");

    int size = 50;

    char* string = tea_mem_new(T, char, size);
    memcpy(string, "[", 1);
    int len = 1;

    TValue v;
    for(int i = 0; i < list->count; i++)
    {
        TValue* value = list->items + i;

        char* element;
        int element_size;

        setlistV(T, &v, list);
        if(tea_obj_rawequal(value, &v))
        {
            element = "[...]";
            element_size = 5;
        }
        else
        {
            GCstr* s = tea_strfmt_obj(T, value, depth);
            element = s->chars;
            element_size = s->len;
        }

        if(element_size > (size - len - 6))
        {
            int old_size = size;
            if(element_size > size)
            {
                size = size + element_size * 2 + 6;
            }
            else
            {
                size = size * 2 + 6;
            }

            string = tea_mem_reallocvec(T, char, string, old_size, size);
        }

        memcpy(string + len, element, element_size);
        len += element_size;

        if(i != list->count - 1)
        {
            memcpy(string + len, ", ", 2);
            len += 2;
        }
    }

    memcpy(string + len, "]", 1);
    len += 1;
    string[len] = '\0';

    string = tea_mem_reallocvec(T, char, string, size, len + 1);

    return tea_str_take(T, string, len);
}

static GCstr* strfmt_map(tea_State* T, GCmap* map, int depth)
{
    if(map->count == 0)
        return tea_str_newlit(T, "{}");
    
    if(depth > TEA_MAX_TOSTR)
        return tea_str_newlit(T, "{...}");

    int count = 0;
    int size = 50;

    char* string = tea_mem_new(T, char, size);
    memcpy(string, "{", 1);
    int len = 1;

    TValue v;
    for(int i = 0; i < map->size; i++)
    {
        MapEntry* entry = &map->entries[i];
        if(entry->empty)
        {
            continue;
        }

        count++;

        char* key;
        int key_size;

        setmapV(T, &v, map);
        if(tea_obj_rawequal(&entry->key, &v))
        {
            key = "{...}";
            key_size = 5;
        }
        else
        {
            GCstr* s = tea_strfmt_obj(T, &entry->key, depth);
            key = s->chars;
            key_size = s->len;
        }

        if(key_size > (size - len - key_size - 4))
        {
            int old_size = size;
            if(key_size > size)
            {
                size += key_size * 2 + 4;
            }
            else
            {
                size *= 2 + 4;
            }

            string = tea_mem_reallocvec(T, char, string, old_size, size);
        }

        if(!tvisstr(&entry->key))
        {
            memcpy(string + len, "[", 1);
            memcpy(string + len + 1, key, key_size);
            memcpy(string + len + 1 + key_size, "] = ", 4);
            len += 5 + key_size;
        }
        else
        {
            memcpy(string + len, key, key_size);
            memcpy(string + len + key_size, " = ", 3);
            len += 3 + key_size;
        }

        char* element;
        int element_size;

        setmapV(T, &v, map);
        if(tea_obj_rawequal(&entry->value, &v))
        {
            element = "{...}";
            element_size = 5;
        }
        else
        {
            GCstr* s = tea_strfmt_obj(T, &entry->value, depth);
            element = s->chars;
            element_size = s->len;
        }

        if(element_size > (size - len - element_size - 6))
        {
            int old_size = size;
            if(element_size > size)
            {
                size += element_size * 2 + 6;
            }
            else
            {
                size = size * 2 + 6;
            }

            string = tea_mem_reallocvec(T, char, string, old_size, size);
        }

        memcpy(string + len, element, element_size);
        len += element_size;

        if(count != map->count)
        {
            memcpy(string + len, ", ", 2);
            len += 2;
        }
    }

    memcpy(string + len, "}", 1);
    len += 1;
    string[len] = '\0';

    string = tea_mem_reallocvec(T, char, string, size, len + 1);

    return tea_str_take(T, string, len);
}

static GCstr* strfmt_instance(tea_State* T, GCinstance* instance)
{
    GCstr* _tostring = T->opm_name[MM_TOSTRING];
    TValue* tostring = tea_tab_get(&instance->klass->methods, _tostring);
    if(tostring)
    {
        setinstanceV(T, T->top++, instance);
        tea_vm_call(T, tostring, 0);

        TValue* result = T->top--;
        if(!tvisstr(result))
        {
            tea_err_run(T, TEA_ERR_TOSTR);
        }

        return strV(result);
    }

    const char* s = tea_strfmt_pushf(T, "<%s instance>", instance->klass->name->chars);
    GCstr* str = tea_str_new(T, s);
    T->top--;
    return str;
}

static GCstr* strfmt_func(tea_State* T, GCproto* proto)
{
    if(proto->type > PROTO_FUNCTION) 
        return proto->name;
    return tea_str_newlit(T, "<function>");
}

/* Conversion of object to string */
GCstr* tea_strfmt_obj(tea_State* T, const TValue* o, int depth)
{
    if(depth > TEA_MAX_TOSTR)
        return tea_str_newlit(T, "...");
    depth++;
    switch(itype(o))
    {
        case TEA_TNULL:
            return tea_str_newlit(T, "null");
        case TEA_TBOOL:
            return boolV(o) ? tea_str_newlit(T, "true") : tea_str_newlit(T, "false");
        case TEA_TNUMBER:
            return tea_strfmt_num(T, o);
        case TEA_TPOINTER:
            return tea_str_newlit(T, "pointer");
        case TEA_TFILE:
            return tea_str_newlit(T, "<file>");
        case TEA_TMETHOD:
            return tea_str_newlit(T, "<method>");
        case TEA_TPROTO:
            return strfmt_func(T, protoV(o));
        case TEA_TFUNC:
        {
            GCfunc* func = funcV(o);
            if(iscfunc(func))
                return tea_str_newlit(T, "<function>");
            else
                return strfmt_func(T, func->t.proto);
        }
        case TEA_TLIST:
            return strfmt_list(T, listV(o), depth);
        case TEA_TMAP:
            return strfmt_map(T, mapV(o), depth);
        case TEA_TRANGE:
        {
            GCrange* range = rangeV(o);
            const char* s = tea_strfmt_pushf(T, "%g..%g", range->start, range->end);
            GCstr* str = tea_str_new(T, s);
            T->top--;
            return str;
        }
        case TEA_TMODULE:
        {
            GCmodule* module = moduleV(o);
            const char* s = tea_strfmt_pushf(T, "<%s module>", module->name->chars);
            GCstr* str = tea_str_new(T, s);
            T->top--;
            return str;
        }
        case TEA_TCLASS:
        {
            GCclass* klass = classV(o);
            const char* s = tea_strfmt_pushf(T, "<%s>", klass->name->chars);
            GCstr* str = tea_str_new(T, s);
            T->top--;
            return str;
        }
        case TEA_TINSTANCE:
            return strfmt_instance(T, instanceV(o));
        case TEA_TSTRING:
            return strV(o);
        case TEA_TUPVALUE:
            return tea_str_newlit(T, "<upvalue>");
        default:
            tea_assertT(T, "unknown type");
            break;
    }
    return tea_str_newlit(T, "unknown");
}

/* -- Internal string formatting ------------------------------------------ */

/*
** These functions are only used for tea_push_fstring(), tea_pushvfstring()
** and for internal string formatting (e.g. error messages). Caveat: unlike
** String.format(), only a limited subset of formats and flags are supported!
*/

/* Push formatted message as a string object to Lua stack. va_list variant */
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
                if(s == NULL) s = "(null)";
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
    return str->chars;
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