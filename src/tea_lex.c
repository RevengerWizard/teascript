/*
** tea_lex.c
** Lexical analyzer
*/

#define tea_lex_c
#define TEA_CORE

#include "tea_def.h"
#include "tea_lex.h"
#include "tea_char.h"
#include "tea_obj.h"
#include "tea_str.h"
#include "tea_strscan.h"
#include "tea_strfmt.h"
#include "tea_parse.h"

/* Teascript lexer token names */
static const char* const lex_tokennames[] = {
#define TKSTR1(name) #name,
#define TKSTR2(name, sym) #sym,
    TKDEF(TKSTR1, TKSTR2)
#undef TKSTR
    NULL
};

/* -- Buffer handling -------------------------------------------------- */

#define LEX_EOF (-1)
#define lex_iseol(ls)  (ls->c == '\n' || ls->c == '\r')

/* Get more input from reader */
static TEA_NOINLINE LexChar lex_more(LexState* ls)
{
    size_t size;
    const char* p = ls->reader(ls->T, ls->rdata, &size);
    if(p == NULL || size == 0) 
        return LEX_EOF;
    if(size >= TEA_MAX_BUF)
    {
        if(size != ~(size_t)0)
            tea_err_mem(ls->T);
        size = ~(uintptr_t)0 - (uintptr_t)p;
        if(size >= TEA_MAX_BUF)
            size = TEA_MAX_BUF - 1;
        ls->endmark = true;
    }
    ls->pe = p + size;
    ls->p = p + 1;
    return (LexChar)(uint8_t)p[0];
}

/* Get next character */
static TEA_AINLINE LexChar lex_next(LexState* ls)
{
    return (ls->c = ls->p < ls->pe ? (LexChar)(uint8_t)*ls->p++ : lex_more(ls));
}

/* Lookahead one character and backtrack */
static LexChar lex_lookahead(LexState* ls)
{
    if(ls->p > ls->pe)
    {
        if(lex_more(ls) == LEX_EOF)
            return LEX_EOF;
        else
        {
            ls->p--;
        }
    }
    return (LexChar)(uint8_t)*ls->p;
}

/* Save character */
static TEA_AINLINE void lex_save(LexState* ls, LexChar c)
{
    tea_buf_putb(ls->T, &ls->sb, c);
}

/* Save previous character and get next character */
static TEA_AINLINE LexChar lex_savenext(LexState* ls)
{
    lex_save(ls, ls->c);
    return lex_next(ls);
}

/* Skip "\n", "\r", "\r\n" or "\n\r" line breaks  */
static void lex_newline(LexState* ls)
{
    LexChar old = ls->c;
    tea_assertLS(lex_iseol(ls), "bad usage");
    lex_next(ls);   /* Skip "\n" or "\r" */
    if(lex_iseol(ls) && ls->c != old)
        lex_next(ls);  /* Skip "\n\r" or "\r\n" */
    if(++ls->linenumber >= TEA_MAX_LINE)
        tea_lex_error(ls, 0, ls->linenumber, TEA_ERR_XLINES);
}

static Token lex_token(LexState* ls, LexToken type)
{
    Token tok;
    tok.t = type;
    tok.line = ls->linenumber;
    return tok;
}

/* -- Scanner for terminals ----------------------------------------------- */

/* Parse a number literal */
static Token lex_number(LexState* ls)
{
    TValue tv;
    StrScanFmt fmt;
    LexChar c, xp = 'e';
    bool und = false;

    tea_assertLS(tea_char_isdigit(ls->c), "bad usage");
    if((c = ls->c) == '0' && (lex_savenext(ls) | 0x20) == 'x')
        xp = 'p';
    while(tea_char_isident(ls->c) || ls->c == '.' ||
           ((ls->c == '-' || ls->c == '+') && (c | 0x20) == xp))
    {
        /* Ignore underscores */
        if(ls->c == '_')
        {
            if(und)
            {
                /* Do not allow double underscores */
                tea_lex_error(ls, TK_number, ls->linenumber, TEA_ERR_XNUMBER);
            }
            und = true;
            lex_next(ls);
            continue;
        }
        else
        {
            und = false;
        }
        /* Do not allow leading '.' numbers */
        if(ls->c == '.')
        {
            LexChar d = lex_lookahead(ls);
            if(d == '.') break;
            else if(!tea_char_isxdigit(d))
            {
                tea_lex_error(ls, TK_number, ls->linenumber, TEA_ERR_XNUMBER);
            }
        }
        c = ls->c;
        lex_savenext(ls);
    }
    /* Do not allow leading '_' */
    if(und)
    {
        tea_lex_error(ls, TK_number, ls->linenumber, TEA_ERR_XNUMBER);
    }
    lex_save(ls, '\0');

    fmt = tea_strscan_scan((const uint8_t*)ls->sb.b, sbuf_len(&ls->sb) - 1, &tv,
                          STRSCAN_OPT_TONUM);

    if(fmt == STRSCAN_NUM)
    {
        /* Already in correct format */
    }
    else
    {
        tea_assertLS(fmt == STRSCAN_ERROR, "unexpected number format %d", fmt);
        tea_lex_error(ls, TK_number, ls->linenumber, TEA_ERR_XNUMBER);
    }

    Token tok = lex_token(ls, TK_number);
    tv.tt = TEA_TNUM;
    copyTV(ls->T, &tok.tv, &tv);
	return tok;
}

/* Parse hex \xXX escape */
static LexChar lex_hex_escape(LexState* ls)
{
    LexChar c = (lex_next(ls) & 15u) << 4;
    if(!tea_char_isdigit(ls->c))
    {
        if(!tea_char_isxdigit(ls->c))
            return -1;
        c += 9 << 4;
    }
    c += (lex_next(ls) & 15u);
    if(!tea_char_isdigit(ls->c))
    {
        if(!tea_char_isxdigit(ls->c))
            return -1;
        c += 9;
    }
    return c;
}

/* Parse unicode \uXXXX or \UXXXXXXXX escapes */
static int lex_unicode_escape(LexState* ls, int len)
{
    LexChar c = 0;
    for(int i = 0; i < len; i++)
    {
        c = (c << 4) | (ls->c & 15u);
        if(!tea_char_isdigit(ls->c))
        {
            if(!tea_char_isxdigit(ls->c))
                return -1;
            c += 9;
        }
        if(c >= 0x110000)
            return -1;  /* Out of Unicode range */
        lex_next(ls);
    }
    if(c < 0x800)
    {
        if(c < 0x80)
            return c;
        lex_save(ls, 0xc0 | (c >> 6));
    }
    else
    {
        if(c >= 0x10000)
        {
            lex_save(ls, 0xf0 | (c >> 18));
            lex_save(ls, 0x80 | ((c >> 12) & 0x3f));
        }
        else
        {
            if(c >= 0xd800 && c < 0xe000)
                return -1; /* No surrogates */
            lex_save(ls, 0xe0 | (c >> 12));
        }
        lex_save(ls, 0x80 | ((c >> 6) & 0x3f));
    }
    c = 0x80 | (c & 0x3f);
    return c;
}

/* Parse a multi line string */
static Token lex_multistring(LexState* ls)
{
    lex_savenext(ls);
    lex_savenext(ls);

    if(lex_iseol(ls))
        lex_newline(ls);

    while(ls->c != ls->sc)
    {
        switch(ls->c)
        {
            case LEX_EOF:
            {
                tea_lex_error(ls, TK_string, ls->linenumber, TEA_ERR_XSTR);
                break;
            }
            case '\r':
            case '\n':
            {
                lex_save(ls, '\n');
                lex_newline(ls);
                break;
            }
            default:
            {
                lex_savenext(ls);
                break;
            }
        }
    }

    LexChar c = ls->c;
    LexChar c1 = lex_savenext(ls);
    LexChar c2 = lex_savenext(ls);
    lex_savenext(ls);

    if((c != ls->sc) || (c1 != ls->sc) || (c2 != ls->sc))
    {
        tea_lex_error(ls, TK_string, ls->linenumber, TEA_ERR_XSTR);
    }

    Token tok = lex_token(ls, TK_string);
    GCstr* str = tea_parse_keepstr(ls, ls->sb.b + 3, sbuf_len(&ls->sb) - 6);
    setstrV(ls->T, &tok.tv, str);
    return tok;
}

/* Parse a string */
static Token lex_string(LexState* ls)
{
    tea_State* T = ls->T;
    LexToken type = TK_string;
    LexChar sc = ls->sc;

    while(ls->c != sc)
    {
        if(ls->c == '$')
        {
            lex_next(ls);
            if(ls->c != '{')
            {
                lex_save(ls, '$');
                continue;
            }

            if(ls->num_braces >= 4)
            {
                tea_lex_error(ls, TK_string, ls->linenumber, TEA_ERR_XSFMT);
            }

            type = TK_interpolation;
            ls->braces[ls->num_braces] = 1;
            ls->str_braces[ls->num_braces] = sc;
            ls->num_braces++;
            break;
        }

        switch(ls->c)
        {
            case LEX_EOF:
            {
                tea_lex_error(ls, TK_string, ls->linenumber, TEA_ERR_XSTR);
                continue;
            }
            case '\n':
            case '\r':
            {
                tea_lex_error(ls, TK_string, ls->linenumber, TEA_ERR_XSTR);
                continue;
            }
            case '\\':
            {
                LexChar c = lex_next(ls);  /* Skip the '\\' */
                switch(ls->c)
                {
                    case LEX_EOF: continue; /* Will raise an error next loop */
                    case '\"': c = '\"'; break;
                    case '\'': c = '\''; break;
                    case '\\': c = '\\'; break;
                    case '`': c = '`'; break;
                    case '0': c = '\0'; break;
                    case '$': c = '$'; break;
                    case 'a': c = '\a'; break;
                    case 'b': c = '\b'; break;
                    case 'e': c = '\033'; break;
                    case 'f': c = '\f'; break;
                    case 'n': c = '\n'; break;
                    case 'r': c = '\r'; break;
                    case 't': c = '\t'; break;
                    case 'v': c = '\v'; break;
                    case 'x':   /* Hexadecimal escape '\xXX' */
                    {
                        c = lex_hex_escape(ls);
                        if(c == -1)
                        {
                            tea_lex_error(ls, TK_string, ls->linenumber, TEA_ERR_XHESC);
                        }
                        break;
                    }
                    case 'u':   /* Unicode escapes '\uXXXX' and '\uXXXXXXXX' */
                    case 'U':
                    {
                        int u = (ls->c == 'u') * 4 + (ls->c == 'U') * 8;
                        lex_next(ls);
                        c = lex_unicode_escape(ls, u);
                        if(c == -1)
                        {
                            tea_lex_error(ls, TK_string, ls->linenumber, TEA_ERR_XUESC);
                        }
                        lex_save(ls, c);
                        continue;
                    }
                    default:
                    {
                        if(!tea_char_isdigit(c))
                            goto err_xesc;
                        c -= '0'; /* Decimal escape '\ddd' */
                        if(tea_char_isdigit(lex_next(ls)))
                        {
                            c = c * 10 + (ls->c - '0');
                            if(tea_char_isdigit(lex_next(ls)))
                            {
                                c = c * 10 + (ls->c - '0');
                                if(c > 255)
                                {
                                err_xesc:
                                    tea_lex_error(ls, TK_string, ls->linenumber, TEA_ERR_XESC);
                                }
                                lex_next(ls);
                            }
                        }
                        lex_save(ls, c);
                        continue;
                    }
                }
                lex_save(ls, c);
                lex_next(ls);
                break;
            }
            default:
            {
                lex_savenext(ls);
                break;
            }
        }
    }

    lex_savenext(ls);

    Token tok = lex_token(ls, type);
    GCstr* str = tea_parse_keepstr(ls, ls->sb.b + 1, sbuf_len(&ls->sb) - 2);
    setstrV(T, &tok.tv, str);
	return tok;
}

/* -- Main lexical scanner ------------------------------------------------ */

/* Get next lexical token */
static Token lex_scan(LexState* ls)
{
    tea_buf_reset(&ls->sb);
    while(true)
    {
        if(tea_char_isident(ls->c))
        {
            /* Numeric literal */
            if(tea_char_isdigit(ls->c))
            {
                return lex_number(ls);
            }
            /* Identifier or reserved word */
            do
            {
                lex_savenext(ls);
            }
            while(tea_char_isident(ls->c) || tea_char_isdigit(ls->c));
            TValue tv;
            Token tok;
            GCstr* s = tea_parse_keepstr(ls, ls->sb.b, sbuf_len(&ls->sb));
            setstrV(ls->T, &tv, s);
            if(s->reserved > 0)
            {
                tok = lex_token(ls, TK_OFS + s->reserved);
                copyTV(ls->T, &tok.tv, &tv);
            }
            else
            {
                tok = lex_token(ls, TK_name);
                copyTV(ls->T, &tok.tv, &tv);
            }
            return tok;
        }
        switch(ls->c)
        {
            case '/':
            {
                lex_next(ls);
                if(ls->c == '/')
                {
                    lex_next(ls);
                    while(ls->c != '\n' && ls->c != LEX_EOF)
                        lex_next(ls);
                    continue;
                }
                else if(ls->c == '*')
                {
                    lex_next(ls);
                    int nesting = 1;
                    while(nesting > 0)
                    {
                        if(ls->c == LEX_EOF)
                        {
                            tea_lex_error(ls, 0, ls->linenumber, TEA_ERR_XLCOM);
                        }

                        if(ls->c == '/' && lex_next(ls) == '*')
                        {
                            lex_next(ls);
                            nesting++;
                            continue;
                        }

                        if(ls->c == '*' && lex_next(ls) == '/')
                        {
                            lex_next(ls);
                            nesting--;
                            continue;
                        }

                        if(ls->c == '\n')
                            ls->linenumber++;

                        lex_next(ls);
                    }
                    continue;
                }
                else if(ls->c == '=')
                {
                    lex_next(ls);
                    return lex_token(ls, TK_diveq);
                }
                else
                {
                    return lex_token(ls, '/');
                }
            }
            case LEX_EOF:
                return lex_token(ls, TK_eof);
            case '\r':
            case '\n':
            {
                lex_newline(ls);
                continue;
            }
            case ' ':
            case '\t':
            case '\v':
            case '\f':
            {
                lex_next(ls);
                continue;
            }
            case '{':
            {
                if(ls->num_braces > 0)
                {
                    ls->braces[ls->num_braces - 1]++;
                }

                lex_next(ls);
                return lex_token(ls, '{');
            }
            case '}':
            {
                if(ls->num_braces > 0 && --ls->braces[ls->num_braces - 1] == 0)
                {
                    ls->sc = ls->str_braces[ls->num_braces - 1];
                    ls->num_braces--;
                    return lex_string(ls);
                }

                lex_next(ls);
                return lex_token(ls, '}');
            }
            case '.':
            {
                lex_next(ls);
                if(ls->c == '.')
                {
                    lex_next(ls);
                    if(ls->c == '.')
                    {
                        lex_next(ls);
                        return lex_token(ls, TK_dotdotdot);
                    }
                    else
                    {
                        return lex_token(ls, TK_dotdot);
                    }
                }
                else
                {
                    return lex_token(ls, '.');
                }
            }
            case '-':
            {
                lex_next(ls);
                if(ls->c == '=')
                {
                    lex_next(ls);
                    return lex_token(ls, TK_mineq);
                }
                else
                {
                    return lex_token(ls, '-');
                }
            }
            case '+':
            {
                lex_next(ls);
                if(ls->c == '=')
                {
                    lex_next(ls);
                    return lex_token(ls, TK_pluseq);
                }
                else
                {
                    return lex_token(ls, '+');
                }
            }
            case '*':
            {
                lex_next(ls);
                if(ls->c == '=')
                {
                    lex_next(ls);
                    return lex_token(ls, TK_muleq);
                }
                else if(ls->c == '*')
                {
                    lex_next(ls);
                    if(ls->c == '=')
                    {
                        lex_next(ls);
                        return lex_token(ls, TK_poweq);
                    }
                    else
                    {
                        return lex_token(ls, TK_pow);
                    }
                }
                else
                {
                    return lex_token(ls, '*');
                }
            }
            case '%':
            {
                lex_next(ls);
                if(ls->c == '=')
                {
                    lex_next(ls);
                    return lex_token(ls, TK_modeq);
                }
                return lex_token(ls, '%');
            }
            case '&':
            {
                lex_next(ls);
                if(ls->c == '=')
                {
                    lex_next(ls);
                    return lex_token(ls, TK_bandeq);
                }
                return lex_token(ls, '&');
            }
            case '|':
            {
                lex_next(ls);
                if(ls->c == '=')
                {
                    lex_next(ls);
                    return lex_token(ls, TK_boreq);
                }
                return lex_token(ls, '|');
            }
            case '^':
            {
                lex_next(ls);
                if(ls->c == '=')
                {
                    lex_next(ls);
                    return lex_token(ls, TK_bxoreq);
                }
                return lex_token(ls, '^');
            }
            case '(': case ')':
            case '[': case ']':
            case ',': case ';':
            case ':': case '?':
            case '~':
            {
                LexChar c = ls->c;
                lex_next(ls);
                return lex_token(ls, c);
            }
            case '!':
            {
                lex_next(ls);
                if(ls->c == '=')
                {
                    lex_next(ls);
                    return lex_token(ls, TK_noteq);
                }
                return lex_token(ls, '!');
            }
            case '=':
            {
                lex_next(ls);
                if(ls->c == '=')
                {
                    lex_next(ls);
                    return lex_token(ls, TK_eq);
                }
                else if(ls->c == '>')
                {
                    lex_next(ls);
                    return lex_token(ls, TK_arrow);
                }
                else
                {
                    return lex_token(ls, '=');
                }
            }
            case '<':
            {
                lex_next(ls);
                if(ls->c == '=')
                {
                    lex_next(ls);
                    return lex_token(ls, TK_le);
                }
                else if(ls->c == '<')
                {
                    lex_next(ls);
                    if(ls->c == '=')
                    {
                        lex_next(ls);
                        return lex_token(ls, TK_lshifteq);
                    }
                    else
                    {
                        return lex_token(ls, TK_lshift);
                    }
                }
                else
                    return lex_token(ls, '<');
            }
            case '>':
            {
                lex_next(ls);
                if(ls->c == '=')
                {
                    lex_next(ls);
                    return lex_token(ls, TK_ge);
                }
                else if(ls->c == '>')
                {
                    lex_next(ls);
                    if(ls->c == '=')
                    {
                        lex_next(ls);
                        return lex_token(ls, TK_rshifteq);
                    }
                    else
                    {
                        return lex_token(ls, TK_rshift);
                    }
                }
                else
                    return lex_token(ls, '>');
            }
            case '`':
            case '"':
            case '\'':
            {
                /* Don't allow the same string delimiter inside interpolation*/
                if(ls->num_braces > 0 && ls->c == ls->str_braces[ls->num_braces - 1])
                {
                    tea_lex_error(ls, TK_string, ls->linenumber, TEA_ERR_XSTR);
                }

                ls->sc = ls->c;
                lex_savenext(ls);
                if(ls->c == ls->sc && lex_lookahead(ls) == ls->sc)
                {
                    return lex_multistring(ls);
                }
                return lex_string(ls);
            }
            default:
                tea_lex_error(ls, 0, ls->linenumber, TEA_ERR_XCHAR);
                break;
        }
    }
}

/* -- Lexer API ----------------------------------------------------------- */

/* Setup lexer */
bool tea_lex_setup(tea_State* T, LexState* ls)
{
    bool header = false;
    ls->T = T;
    ls->fs = NULL;
    ls->pe = ls->p = NULL;
    ls->vstack = NULL;
    ls->sizevstack = 0;
    ls->vtop = 0;
    ls->bcstack = NULL;
    ls->sizebcstack = 0;
    ls->next.t = 0; /* Initialize the next token */
    ls->linenumber = 1;
    ls->num_braces = 0;
    ls->endmark = false;
    lex_next(ls);  /* Read first char */
    /* Skip UTF-8 BOM */
    if(ls->c == 0xef && ls->p + 2 <= ls->pe && (uint8_t)ls->p[0] == 0xbb && (uint8_t)ls->p[1] == 0xbf)
    {
        ls->p += 2;
        lex_next(ls);
        header = true;
    }
    /* Skip POSIX #! header line */
    if(ls->c == '#' && lex_lookahead(ls) == '!')
    {
        do
        {
            lex_next(ls);
            if(ls->c == LEX_EOF)
                return false;
        }
        while(!lex_iseol(ls));
        lex_newline(ls);
        header = true;
    }
    /* Bytecode dump */
    if(ls->c == TEA_SIGNATURE[0])
    {
        if(header)
        {
            /*
            ** Loading bytecode with an extra header is disabled for security
            ** reasons. This may circumvent the usual check for bytecode vs.
            ** Teascript code by looking at the first char. Since this is a potential
            ** security violation no attempt is made to echo the name either.
            */
            setstrV(T, T->top++, tea_err_str(T, TEA_ERR_BCBAD));
            tea_err_throw(T, TEA_ERROR_SYNTAX);
        }
        return true;
    }
    return false;
}

/* Cleanup lexer state */
void tea_lex_cleanup(tea_State* T, LexState* ls)
{
    tea_mem_freevec(T, BCInsLine, ls->bcstack, ls->sizebcstack);
    tea_mem_freevec(T, VarInfo, ls->vstack, ls->sizevstack);
    tea_buf_free(T, &ls->sb);
}

/* Convert token to string */
const char* tea_lex_token2str(LexState* ls, LexToken t)
{
    if(t > TK_OFS)
        return lex_tokennames[t - TK_OFS - 1];
    else if(!tea_char_iscntrl(t))
        return tea_strfmt_pushf(ls->T, "%c", t);
    else
        return tea_strfmt_pushf(ls->T, "char(%d)", t);
}

/* Lexer error */
void tea_lex_error(LexState* ls, LexToken tok, BCLine line, ErrMsg em, ...)
{
    const char* tokstr = NULL;
    va_list argp;
    char* name = str_datawr(ls->module->name);
    char c = name[0];
    int off = 0;
    if(c == '?' || c == '=') off = 1;
    if(tok == 0)
    {
        tokstr = NULL;
    }
    else
    {
        tokstr = tea_lex_token2str(ls, tok);
    }
    va_start(argp, em);
    tea_err_lex(ls->T, name + off, tokstr, line, em, argp);
    va_end(argp);
}

/* Return next lexical token */
void tea_lex_next(LexState* ls)
{
    ls->prev = ls->curr;
    ls->curr = ls->next;
    if(ls->next.t == TK_eof) return;
    if(ls->curr.t == TK_eof) return;
    ls->next = lex_scan(ls);
}

/* Initialize strings for reserved words */
void tea_lex_init(tea_State* T)
{
    uint32_t i;
    for(i = 0; i < TK_RESERVED; i++)
    {
        GCstr* s = tea_str_newlen(T, lex_tokennames[i]);
        fix_string(s);
        s->reserved = (uint8_t)(i + 1);
    }
}