/*
** tea_lex.c
** Lexical analyzer
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define tea_lex_c
#define TEA_CORE

#include "tea_def.h"
#include "tea_lex.h"
#include "tea_char.h"
#include "tea_obj.h"
#include "tea_str.h"
#include "tea_utf.h"
#include "tea_strscan.h"

/* Teascript lexer token names */
TEA_DATADEF const char* const lex_tokennames[] = {
#define TKSTR(name, sym) #sym,
    TKDEF(TKSTR)
#undef TKSTR
};

/* -- Buffer handling -------------------------------------------------- */

#define LEX_EOF (-1)
#define lex_iseol(lex)  (lex->c == '\n' || lex->c == '\r')

/* Get more input from reader */
static TEA_NOINLINE LexChar lex_more(Lexer* lex)
{
    size_t size;
    const char* p = lex->reader(lex->T, lex->data, &size);
    if(p == NULL || size == 0) 
        return LEX_EOF;
    if(size >= TEA_MAX_BUF)
    {
        if(size != ~(size_t)0)
            tea_err_mem(lex->T);
        size = ~(uintptr_t)0 - (uintptr_t)p;
        if(size >= TEA_MAX_BUF)
            size = TEA_MAX_BUF - 1;
        lex->endmark = true;
    }
    lex->pe = p + size;
    lex->p = p + 1;
    return (LexChar)(uint8_t)p[0];
}

/* Get next character */
static TEA_AINLINE LexChar lex_next(Lexer* lex)
{
    return (lex->c = lex->p < lex->pe ? (LexChar)(uint8_t)*lex->p++ : lex_more(lex));
}

/* Lookahead one character and backtrack */
static LexChar lex_lookahead(Lexer* lex)
{
    if(lex->p > lex->pe)
    {
        if(lex_more(lex) == LEX_EOF)
            return LEX_EOF;
        else
        {
            lex->pe--;
            lex->p--;
        }
    }
    return (int)(uint8_t)*lex->p;
}

/* Save character */
static TEA_AINLINE void lex_save(Lexer* lex, LexChar c)
{
    tea_buf_putb(lex->T, &lex->sb, c);
}

/* Save previous character and get next character */
static TEA_AINLINE LexChar lex_savenext(Lexer* lex)
{
    lex_save(lex, lex->c);
    return lex_next(lex);
}

/* Skip "\n", "\r", "\r\n" or "\n\r" line breaks  */
static void lex_newline(Lexer* lex)
{
    LexChar old = lex->c;
    tea_assertLS(lex_iseol(lex), "bad usage");
    lex_next(lex);   /* Skip "\n" or "\r" */
    if(lex_iseol(lex) && lex->c != old)
        lex_next(lex);  /* Skip "\n\r" or "\r\n" */
    lex->line++;
}

static void lex_syntaxerror(Lexer* lex, ErrMsg em)
{
    tea_lex_error(lex, NULL, em);
}

static Token lex_token(Lexer* lex, int type)
{
    Token token;
    token.type = type;
    token.line = lex->line;
    return token;
}

/* -- Scanner for terminals ----------------------------------------------- */

/* Parse a number literal */
static Token lex_number(Lexer* lex)
{
    TValue tv;
    StrScanFmt fmt;
    LexChar c, xp = 'e';

    tea_assertLS(tea_char_isdigit(lex->c), "bad usage");
    if((c = lex->c) == '0' && (lex_savenext(lex) | 0x20) == 'x')
        xp = 'p';
    while(tea_char_isident(lex->c) || lex->c == '.' ||
           ((lex->c == '-' || lex->c == '+') && (c | 0x20) == xp))
    {
        /* Ignore underscores */
        if(lex->c == '_')
        {
            lex_next(lex);
            continue;
        }
        /* Do not allow leading '.' numbers */
        if(lex->c == '.')
        {
            LexChar d = lex_lookahead(lex);
            if(d == '.') break;
            else if(!tea_char_isxdigit(d))
            {
                lex_syntaxerror(lex, TEA_ERR_XNUMBER);
            }
        }
        c = lex->c;
        lex_savenext(lex);
    }
    lex_save(lex, '\0');

    fmt = tea_strscan_scan((const uint8_t *)lex->sb.b, sbuf_len(&lex->sb) - 1, &tv,
                          STRSCAN_OPT_TONUM);

    if(fmt == STRSCAN_NUM)
    {
        /* Already in correct format */
    }
    else
    {
        tea_assertLS(fmt == STRSCAN_ERROR, "unexpected number format %d", fmt);
        lex_syntaxerror(lex, TEA_ERR_XNUMBER);
    }

    Token token = lex_token(lex, TK_NUMBER);
    tv.tt = TEA_TNUM;
    copyTV(lex->T, &token.value, &tv);
	return token;
}

static LexChar lex_hex_escape(Lexer* lex)
{
    LexChar c = (lex_next(lex) & 15u) << 4;
    if(!tea_char_isdigit(lex->c))
    {
        if(!tea_char_isxdigit(lex->c))
            return -1;
        c += 9 << 4;
    }
    c += (lex_next(lex) & 15u);
    if(!tea_char_isdigit(lex->c))
    {
        if(!tea_char_isxdigit(lex->c))
            return -1;
        c += 9;
    }
    return c;
}

static int lex_unicode_escape(Lexer* lex, int len)
{
    LexChar c = 0;
    for(int i = 0; i < len; i++)
    {
        c = (c << 4) | (lex->c & 15u);
        if(!tea_char_isdigit(lex->c))
        {
            if(!tea_char_isxdigit(lex->c))
                return -1;
            c += 9;
        }
        if(c >= 0x110000)
            return -1;  /* Out of Unicode range */
        lex_next(lex);
    }
    if(c < 0x800)
    {
        if(c < 0x80)
            return c;
        lex_save(lex, 0xc0 | (c >> 6));
    }
    else
    {
        if(c >= 0x10000)
        {
            lex_save(lex, 0xf0 | (c >> 18));
            lex_save(lex, 0x80 | ((c >> 12) & 0x3f));
        }
        else
        {
            if(c >= 0xd800 && c < 0xe000)
                return -1; /* No surrogates */
            lex_save(lex, 0xe0 | (c >> 12));
        }
        lex_save(lex, 0x80 | ((c >> 6) & 0x3f));
    }
    c = 0x80 | (c & 0x3f);
    return c;
}

/* Parse a multi line string */
static Token lex_multistring(Lexer* lex)
{
    lex_savenext(lex);
    lex_savenext(lex);

    if(lex_iseol(lex))
        lex_newline(lex);

    while(lex->c != lex->string)
    {
        switch(lex->c)
        {
            case LEX_EOF:
            {
                lex_syntaxerror(lex, TEA_ERR_XSTR);
                break;
            }
            case '\r':
            case '\n':
            {
                lex_save(lex, '\n');
                lex_newline(lex);
                break;
            }
            default:
            {
                lex_savenext(lex);
                break;
            }
        }
    }

    LexChar c = lex->c;
    LexChar c1 = lex_savenext(lex);
    LexChar c2 = lex_savenext(lex);
    lex_savenext(lex);

    if((c != lex->string) || (c1 != lex->string) || (c2 != lex->string))
    {
        lex_syntaxerror(lex, TEA_ERR_XSTR);
    }

    Token token = lex_token(lex, TK_STRING);
    GCstr* str = tea_str_new(lex->T, lex->sb.b + 3, sbuf_len(&lex->sb) - 6);
    setstrV(lex->T, &token.value, str);
    return token;
}

/* Parse a string */
static Token lex_string(Lexer* lex)
{
    tea_State* T = lex->T;
    LexToken type = TK_STRING;

    while(lex->c != lex->string)
    {
        if(lex->c == '$')
        {
            if(lex->num_braces >= 4)
            {
				lex_syntaxerror(lex, TEA_ERR_XSFMT);
			}

            lex_next(lex);
            if(lex->c != '{')
            {
                lex_save(lex, '$');
                continue;
            }

			type = TK_INTERPOLATION;
			lex->braces[lex->num_braces++] = 1;
			break;
        }

        switch(lex->c)
        {
            case LEX_EOF:
            {
                lex_syntaxerror(lex, TEA_ERR_XSTR);
                continue;
            }
            case '\n':
            case '\r':
            {
                lex_syntaxerror(lex, TEA_ERR_XSTR);
                continue;
            }
            case '\\':
            {
                LexChar c = lex_next(lex);  /* Skip the '\\' */
                switch(lex->c)
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
                        c = lex_hex_escape(lex);
                        if(c == -1)
                        {
                            lex_syntaxerror(lex, TEA_ERR_XHESC);
                        }
                        break;
                    }
                    case 'u':   /* Unicode escapes '\uXXXX' and '\uXXXXXXXX' */
                    case 'U':
                    {
                        int u = (lex->c == 'u') * 4 + (lex->c == 'U') * 8;
                        lex_next(lex);
                        c = lex_unicode_escape(lex, u);
                        if(c == -1)
                        {
                            lex_syntaxerror(lex, TEA_ERR_XUESC);
                        }
                        lex_save(lex, c);
                        continue;
                    }
                    default:
                    {
                        if(!tea_char_isdigit(c))
                            goto err_xesc;
                        c -= '0'; /* Decimal escape '\ddd' */
                        if(tea_char_isdigit(lex_next(lex)))
                        {
                            c = c * 10 + (lex->c - '0');
                            if(tea_char_isdigit(lex_next(lex)))
                            {
                                c = c * 10 + (lex->c - '0');
                                if(c > 255)
                                {
                                err_xesc:
                                    lex_syntaxerror(lex, TEA_ERR_XESC);
                                }
                                lex_next(lex);
                            }
                        }
                        lex_save(lex, c);
                        continue;
                    }
                }
                lex_save(lex, c);
                lex_next(lex);
                break;
            }
            default:
            {
                lex_savenext(lex);
                break;
            }
        }
    }

    lex_savenext(lex);

    Token token = lex_token(lex, type);
    GCstr* str = tea_str_new(T, lex->sb.b + 1, sbuf_len(&lex->sb) - 2);
    setstrV(T, &token.value, str);

	return token;
}

/* -- Main lexical scanner ------------------------------------------------ */

/* Get next lexical token */
static Token lex_scan(Lexer* lex)
{
    tea_buf_reset(&lex->sb);
    while(true)
    {
        switch(lex->c)
        {
            case '/':
            {
                lex_next(lex);
                if(lex->c == '/')
                {
                    lex_next(lex);
                    while(lex->c != '\n' && lex->c != LEX_EOF)
                        lex_next(lex);
                    continue;
                }
                else if(lex->c == '*')
                {
                    lex_next(lex);
                    int nesting = 1;
                    while(nesting > 0)
                    {
                        if(lex->c == LEX_EOF)
                        {
                            lex_syntaxerror(lex, TEA_ERR_XLCOM);
                        }

                        if(lex->c == '/' && lex_next(lex) == '*')
                        {
                            lex_next(lex);
                            nesting++;
                            continue;
                        }

                        if(lex->c == '*' && lex_next(lex) == '/')
                        {
                            lex_next(lex);
                            nesting--;
                            continue;
                        }

                        if(lex->c == '\n')
                            lex->line++;

                        lex_next(lex);
                    }
                    continue;
                }
                else if(lex->c == '=')
                {
                    lex_next(lex);
                    return lex_token(lex, TK_SLASH_EQUAL);
                }
                else
                {
                    return lex_token(lex, '/');
                }
            }
            case LEX_EOF:
                return lex_token(lex, TK_EOF);
            case '\r':
            case '\n':
            {
                lex_newline(lex);
                continue;
            }
            case ' ':
            case '\t':
            case '\v':
            case '\f':
            {
                lex_next(lex);
                continue;
            }
            case '{':
            {
                if(lex->num_braces > 0)
                {
                    lex->braces[lex->num_braces - 1]++;
                }

                lex_next(lex);
                return lex_token(lex, '{');
            }
            case '}':
            {
                if(lex->num_braces > 0 && --lex->braces[lex->num_braces - 1] == 0)
                {
                    lex->num_braces--;

                    return lex_string(lex);
                }

                lex_next(lex);
                return lex_token(lex, '}');
            }
            case '.':
            {
                lex_next(lex);
                if(lex->c == '.')
                {
                    lex_next(lex);
                    if(lex->c == '.')
                    {
                        lex_next(lex);
                        return lex_token(lex, TK_DOT_DOT_DOT);
                    }
                    else
                    {
                        return lex_token(lex, TK_DOT_DOT);
                    }
                }
                else
                {
                    return lex_token(lex, '.');
                }
            }
            case '-':
            {
                lex_next(lex);
                if(lex->c == '=')
                {
                    lex_next(lex);
                    return lex_token(lex, TK_MINUS_EQUAL);
                }
                else if(lex->c == '-')
                {
                    lex_next(lex);
                    return lex_token(lex, TK_MINUS_MINUS);
                }
                else
                {
                    return lex_token(lex, '-');
                }
            }
            case '+':
            {
                lex_next(lex);
                if(lex->c == '=')
                {
                    lex_next(lex);
                    return lex_token(lex, TK_PLUS_EQUAL);
                }
                else if(lex->c == '+')
                {
                    lex_next(lex);
                    return lex_token(lex, TK_PLUS_PLUS);
                }
                else
                {
                    return lex_token(lex, '+');
                }
            }
            case '*':
            {
                lex_next(lex);
                if(lex->c == '=')
                {
                    lex_next(lex);
                    return lex_token(lex, TK_STAR_EQUAL);
                }
                else if(lex->c == '*')
                {
                    lex_next(lex);
                    if(lex->c == '=')
                    {
                        lex_next(lex);
                        return lex_token(lex, TK_STAR_STAR_EQUAL);
                    }
                    else
                    {
                        return lex_token(lex, TK_STAR_STAR);
                    }
                }
                else
                {
                    return lex_token(lex, '*');
                }
            }
            case '%':
            {
                lex_next(lex);
                if(lex->c == '=')
                {
                    lex_next(lex);
                    return lex_token(lex, TK_PERCENT_EQUAL);
                }
                return lex_token(lex, '%');
            }
            case '&':
            {
                lex_next(lex);
                if(lex->c == '=')
                {
                    lex_next(lex);
                    return lex_token(lex, TK_AMPERSAND_EQUAL);
                }
                return lex_token(lex, '&');
            }
            case '|':
            {
                lex_next(lex);
                if(lex->c == '=')
                {
                    lex_next(lex);
                    return lex_token(lex, TK_PIPE_EQUAL);
                }
                return lex_token(lex, '|');
            }
            case '^':
            {
                lex_next(lex);
                if(lex->c == '=')
                {
                    lex_next(lex);
                    return lex_token(lex, TK_CARET_EQUAL);
                }
                return lex_token(lex, '^');
            }
            case '(': case ')':
            case '[': case ']':
            case ',': case ';':
            case ':': case '?':
            case '~':
            {
                LexChar c = lex->c;
                lex_next(lex);
                return lex_token(lex, c);
            }
            case '!':
            {
                lex_next(lex);
                if(lex->c == '=')
                {
                    lex_next(lex);
                    return lex_token(lex, TK_BANG_EQUAL);
                }
                return lex_token(lex, '!');
            }
            case '=':
            {
                lex_next(lex);
                if(lex->c == '=')
                {
                    lex_next(lex);
                    return lex_token(lex, TK_EQUAL_EQUAL);
                }
                else if(lex->c == '>')
                {
                    lex_next(lex);
                    return lex_token(lex, TK_ARROW);
                }
                else
                {
                    return lex_token(lex, '=');
                }
            }
            case '<':
            {
                lex_next(lex);
                if(lex->c == '=')
                {
                    lex_next(lex);
                    return lex_token(lex, TK_LESS_EQUAL);
                }
                else if(lex->c == '<')
                {
                    lex_next(lex);
                    if(lex->c == '=')
                    {
                        lex_next(lex);
                        return lex_token(lex, TK_LESS_LESS_EQUAL);
                    }
                    else
                    {
                        return lex_token(lex, TK_LESS_LESS);
                    }
                }
                else
                    return lex_token(lex, '<');
            }
            case '>':
            {
                lex_next(lex);
                if(lex->c == '=')
                {
                    lex_next(lex);
                    return lex_token(lex, TK_GREATER_EQUAL);
                }
                else if(lex->c == '>')
                {
                    lex_next(lex);
                    if(lex->c == '=')
                    {
                        lex_next(lex);
                        return lex_token(lex, TK_GREATER_GREATER_EQUAL);
                    }
                    else
                    {
                        return lex_token(lex, TK_GREATER_GREATER);
                    }
                }
                else
                    return lex_token(lex, '>');
            }
            case '`':
            case '"':
            case '\'':
            {
                lex->string = lex->c;
                lex_savenext(lex);
                if(lex->c == lex->string && lex_lookahead(lex) == lex->string)
                {
                    return lex_multistring(lex);
                }
                return lex_string(lex);
            }
            default:
            {
                if(tea_char_isident(lex->c))
                {
                    if(tea_char_isdigit(lex->c))
                    {
                        return lex_number(lex);
                    }

                    /* Identifier or reserved word */
                    do
                    {
                        lex_savenext(lex);
                    }
                    while(tea_char_isident(lex->c) || tea_char_isdigit(lex->c));
                    TValue tv;
                    GCstr* s = tea_str_new(lex->T, lex->sb.b, sbuf_len(&lex->sb));
                    setstrV(lex->T, &tv, s);
                    Token token;
                    if(s->reserved > 0)
                    {
                        token = lex_token(lex, TK_OFS + s->reserved);
                        copyTV(lex->T, &token.value, &tv);
                    }
                    else
                    {
                        token = lex_token(lex, TK_NAME);
                        copyTV(lex->T, &token.value, &tv);
                    }
                    return token;
                }
                else
                    lex_syntaxerror(lex, TEA_ERR_XCHAR);
            }
        }
    }
}

/* -- Lexer API ----------------------------------------------------------- */

/* Setup lexer */
bool tea_lex_setup(tea_State* T, Lexer* lex)
{
    bool header = false;
    lex->T = T;
    lex->pe = lex->p = NULL;
    lex->next.type = 0; /* Initialize the next token */
    lex->line = 1;
    lex->num_braces = 0;
    lex->endmark = false;
    lex_next(lex);  /* Read first char */
    /* Skip UTF-8 BOM */
    if(lex->c == 0xef && lex->p + 2 <= lex->pe && (uint8_t)lex->p[0] == 0xbb && (uint8_t)lex->p[1] == 0xbf)
    {
        lex->p += 2;
        lex_next(lex);
        header = true;
    }
    /* Skip POSIX #! header line */
    if(lex->c == '#' && lex_lookahead(lex) == '!')
    {
        do
        {
            lex_next(lex);
            if(lex->c == LEX_EOF)
                return false;
        }
        while(!lex_iseol(lex));
        lex_newline(lex);
        header = true;
    }
    /* Bytecode dump */
    if(lex->c == TEA_SIGNATURE[0])
    {
        if(header)
        {
            tea_err_throw(T, TEA_ERROR_SYNTAX);
        }
        return true;
    }
    return false;
}

/* Convert token to string */
const char* tea_lex_token2str(Lexer* lex, LexToken t)
{
    if(t > TK_OFS)
        return lex_tokennames[t - TK_OFS - 1];
    else
    {
        static char s[2];
        s[0] = (char)t;
        s[1] = '\0';
        return s;
    }
}

/* Lexer error */
void tea_lex_error(Lexer* lex, Token* token, ErrMsg em, ...)
{
    char* module_name = str_datawr(lex->module->name);
    char c = module_name[0];
    int off = 0;
    if(c == '?' || c == '=') off = 1;

    const char* tokstr = NULL;
    va_list argp;
    if(token != NULL)
    {
        tokstr = tea_lex_token2str(lex, token->type);
    }
    va_start(argp, em);
    tea_err_lex(lex->T, module_name + off, tokstr, token ? token->line : lex->line, em, argp);
    va_end(argp);
}

/* Return next lexical token */
void tea_lex_next(Lexer* lex)
{
    lex->prev = lex->curr;
    lex->curr = lex->next;

    if(lex->next.type == TK_EOF) return;
    if(lex->curr.type == TK_EOF) return;

    lex->next = lex_scan(lex);
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