/*
** tea_lex.c
** Lexical analyzer
*/

#define tea_lex_c
#define TEA_CORE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "tea_def.h"
#include "tea_char.h"
#include "tea_obj.h"
#include "tea_lex.h"
#include "tea_str.h"
#include "tea_utf.h"
#include "tea_err.h"

/* Teascript lexer token names */
static const char* const lex_tokennames[] = {
#define TKSTR(name, sym) #sym,
    TKDEF(TKSTR)
#undef TKSTR
};

#define LEX_EOF (-1)
#define lex_iseol(lex)  (lex->c == '\n' || lex->c == '\r')

/* Get more input from reader */
static TEA_NOINLINE int lex_more(Lexer* lex)
{
    size_t size;
    const char* p = lex->reader(lex->T, lex->data, &size);
    if(p == NULL || size == 0) 
        return LEX_EOF;
    if(size >= TEA_MAX_BUF)
    {
        size = ~(uintptr_t)0 - (uintptr_t)p;
        if(size >= TEA_MAX_BUF)
            size = TEA_MAX_BUF - 1;
        lex->endmark = true;
    }
    lex->pe = p + size;
    lex->p = p + 1;
    return (int)(uint8_t)p[0];
}

/* Get next character */
static TEA_AINLINE int lex_next(Lexer* lex)
{
    return (lex->c = lex->p < lex->pe ? (int)(uint8_t)*lex->p++ : lex_more(lex));
}

/* Lookahead one character and backtrack */
static int lex_lookahead(Lexer* lex)
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
static TEA_AINLINE void lex_save(Lexer* lex, int c)
{
    tea_buf_putb(lex->T, &lex->sbuf, c);
}

/* Save previous character and get next character */
static TEA_AINLINE int lex_savenext(Lexer* lex)
{
    lex_save(lex, lex->c);
    return lex_next(lex);
}

static bool lex_checknext(Lexer* lex, const char* set)
{
    if(!strchr(set, lex->c))
        return false;
    lex_savenext(lex);
    return true;
}

/* Skip "\n", "\r", "\r\n" or "\n\r" line breaks  */
static void lex_newline(Lexer* lex)
{
    int old = lex->c;
    lex_next(lex);   /* Skip "\n" or "\r" */
    if(lex_iseol(lex) && lex->c != old)
        lex_next(lex);  /* Skip "\n\r" or "\r\n" */
    lex->line++;
}

static void lex_syntaxerror(Lexer* lex, const char* message)
{
    tea_lex_error(lex, NULL, message);
}

static Token lex_token(Lexer* lex, int type)
{
    Token token;
    token.type = type;
    token.line = lex->line;

    return token;
}

typedef struct
{
    const char* name;
    int type;
} Keyword;

const Keyword lex_tokens[] = {
    { "and", TK_AND },
    { "not", TK_NOT },
    { "class", TK_CLASS },
    { "static", TK_STATIC },
    { "else", TK_ELSE },
    { "false", TK_FALSE },
    { "for", TK_FOR },
    { "function", TK_FUNCTION },
    { "case", TK_CASE },
    { "switch", TK_SWITCH },
    { "default", TK_DEFAULT },
    { "if", TK_IF },
    { "null", TK_NULL },
    { "or", TK_OR },
    { "is", TK_IS },
    { "import", TK_IMPORT },
    { "from", TK_FROM },
    { "as", TK_AS },
    { "enum", TK_ENUM },
    { "return", TK_RETURN },
    { "super", TK_SUPER },
    { "this", TK_THIS },
    { "continue", TK_CONTINUE },
    { "break", TK_BREAK },
    { "in", TK_IN },
    { "true", TK_TRUE },
    { "var", TK_VAR },
    { "const", TK_CONST },
    { "while", TK_WHILE },
    { "do", TK_DO },
    { NULL, 0 }
};

static Token lex_name(Lexer* lex)
{
    size_t len = sbuf_len(&lex->sbuf);
    int type = TK_NAME;

    for(int i = 0; lex_tokens[i].name != NULL; i++)
    {
        if(len == strlen(lex_tokens[i].name) &&
            memcmp(lex->sbuf.b, lex_tokens[i].name, len) == 0)
        {
            type = lex_tokens[i].type;
            break;
        }
    }

    Token token = lex_token(lex, type);
    token.value = OBJECT_VAL(tea_str_copy(lex->T, lex->sbuf.b, (int)len));

    return token;
}

#define NUM_HEX 16
#define NUM_BIN 2
#define NUM_OCTAL 8
#define NUM_DEC 10

static Token lex_number_token(Lexer* lex, int num)
{
    errno = 0;
	TValue value;

    lex_save(lex, '\0');
    const char* buff = lex->sbuf.b;

    switch(num)
    {
        case NUM_HEX:
		    value = NUMBER_VAL((double)strtoll(buff, NULL, NUM_HEX));
            break;
        case NUM_BIN:
		    value = NUMBER_VAL((int)strtoll(buff + 2, NULL, NUM_BIN));
            break;
        case NUM_OCTAL:
            value = NUMBER_VAL((int)strtoll(buff + 2, NULL, NUM_OCTAL));
            break;
        default:
		    value = NUMBER_VAL(strtod(buff, NULL));
            break;
    }

	if(errno == ERANGE)
    {
		errno = 0;
		lex_syntaxerror(lex, "Number too big");
	}

	Token token = lex_token(lex, TK_NUMBER);
	token.value = value;

	return token;
}

static Token lex_hex_number(Lexer* lex)
{
    bool last = false;

    while(tea_char_isxdigit(lex->c) || (lex->c == '_'))
    {
        if(lex->c == '_')
        {
            if(last) lex_syntaxerror(lex, "Cannot have consecutive underscores");
            lex_next(lex);
            last = true;
        }
        else
        {
            lex_savenext(lex);
            last = false;
        }
    }

    if(last) lex_syntaxerror(lex, "Invalid hex number");

    return lex_number_token(lex, NUM_HEX);
}

static Token lex_binary_number(Lexer* lex)
{
    bool last = false;

    while(tea_char_isbdigit(lex->c) || (lex->c == '_'))
    {
        if(lex->c == '_')
        {
            if(last) lex_syntaxerror(lex, "Cannot have consecutive underscores");
            lex_next(lex);
            last = true;
        }
        else
        {
            lex_savenext(lex);
            last = false;
        }
    }

    if(last) lex_syntaxerror(lex, "Cannot have leading underscores");

    return lex_number_token(lex, NUM_BIN);
}

static Token lex_octal_number(Lexer* lex)
{
    bool last = false;

    while(tea_char_iscdigit(lex->c) || (lex->c == '_'))
    {
        if(lex->c == '_')
        {
            if(last) lex_syntaxerror(lex, "Cannot have consecutive underscores");
            lex_next(lex);
            last = true;
        }
        else
        {
            lex_savenext(lex);
            last = false;
        }
    }

    if(last) lex_syntaxerror(lex, "Cannot have leading underscores");

    return lex_number_token(lex, NUM_OCTAL);
}

static Token lex_exponent_number(Lexer* lex)
{
    lex_checknext(lex, "+-"); /* Optional exponent sign */

    if(!tea_char_isdigit(lex->c))
    {
        lex_syntaxerror(lex, "Unterminated scientific notation");
    }

    bool last = false;

    while(tea_char_isdigit(lex->c) || (lex->c == '_'))
    {
        if(lex->c == '_')
        {
            if(last) lex_syntaxerror(lex, "Cannot have consecutive underscores");
            lex_next(lex);
            last = true;
        }
        else
        {
            lex_savenext(lex);
            last = false;
        }
    }

    if(last) lex_syntaxerror(lex, "Cannot have leading underscores");

    return lex_number_token(lex, NUM_DEC);
}

static Token lex_number(Lexer* lex)
{
    if(lex->c == '0')
    {
        lex_savenext(lex);
        if(lex_checknext(lex, "xX"))
        {
            return lex_hex_number(lex);
        }
        else if(lex_checknext(lex, "bB"))
        {
            return lex_binary_number(lex);
        }
        else if(lex_checknext(lex, "cC"))
        {
            return lex_octal_number(lex);
        }
    }

    bool last = false;
    while(tea_char_isdigit(lex->c) || (lex->c == '_'))
    {
        if(lex->c == '_')
        {
            if(last) lex_syntaxerror(lex, "Cannot have consecutive underscores");
            lex_next(lex);
            last = true;
        }
        else
        {
            lex_savenext(lex);
            last = false;
        }
    }

    /* Look for a fractional part */
    if(lex->c == '.')
    {
        /* Do not allow leading '.' numbers */
        if(!tea_char_isdigit(lex_lookahead(lex)))
        {
            return lex_number_token(lex, NUM_DEC);
        }

        /* Consume the "." */
        lex_savenext(lex);

        last = false;
        while(tea_char_isdigit(lex->c) || (lex->c == '_'))
        {
            if(lex->c == '_')
            {
                if(last) lex_syntaxerror(lex, "Cannot have consecutive underscores");
                lex_next(lex);
                last = true;
            }
            else
            {
                lex_savenext(lex);
                last = false;
            }
        }
    }

    if(lex_checknext(lex, "eE"))
    {
        return lex_exponent_number(lex);
    }

    if(last) lex_syntaxerror(lex, "Cannot have leading underscores");

    return lex_number_token(lex, NUM_DEC);
}

static int lex_hex_escape(Lexer* lex)
{
    int c = (lex_next(lex) & 15u) << 4;
    if(!tea_char_isdigit(lex->c))
    {
        if (!tea_char_isxdigit(lex->c))
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
    int c = 0;
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
                lex_syntaxerror(lex, "Unterminated string");
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

    int c = lex->c;
    int c1 = lex_savenext(lex);
    int c2 = lex_savenext(lex);
    lex_savenext(lex);

    if((c != lex->string) || (c1 != lex->string) || (c2 != lex->string))
    {
        lex_syntaxerror(lex, "Unterminated string");
    }

    Token token = lex_token(lex, TK_STRING);
	token.value = OBJECT_VAL(tea_str_copy(lex->T, lex->sbuf.b + 3, sbuf_len(&lex->sbuf) - 6));
    return token;
}

static Token lex_string(Lexer* lex)
{
    tea_State* T = lex->T;
    int type = TK_STRING;

    while(lex->c != lex->string)
    {
        if(lex->c == '$')
        {
            if(lex->num_braces >= 4)
            {
				lex_syntaxerror(lex, "String interpolation is too deep");
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
                lex_syntaxerror(lex, "Unterminated string");
                continue;
            }
            case '\n':
            case '\r':
            {
                lex_syntaxerror(lex, "Unterminated string");
                continue;
            }
            case '\\':
            {
                int c = lex_next(lex);  /* Skip the '\\' */
                switch(lex->c)
                {
                    case LEX_EOF: continue; /* Will raise an error next loop */
                    case '\"': c = '\"'; break;
                    case '\'': c = '\''; break;
                    case '\\': c = '\\'; break;
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
                            lex_syntaxerror(lex, "Incomplete hex escape sequence");
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
                            lex_syntaxerror(lex, "Incomplete unicode escape sequence");
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
                                    lex_syntaxerror(lex, "Invalid escape character");
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
	token.value = OBJECT_VAL(tea_str_copy(T, lex->sbuf.b + 1, sbuf_len(&lex->sbuf) - 2));

	return token;
}

static const char* lex_token2str(Lexer* lex, int t)
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

void tea_lex_error(Lexer* lex, Token* token, const char* message)
{
    char* module_name = lex->module->name->chars;
    char c = module_name[0];
    int off = 0;
    if(c == '?' || c == '=') off = 1;

    const char* tokstr = NULL;
    if(token != NULL)
    {
        tokstr = lex_token2str(lex, token->type);
    }

    tea_err_lex(lex->T, module_name + off, tokstr, token ? token->line : lex->line, message);
}

static Token lex_scan(Lexer* lex)
{
    tea_buf_reset(&lex->sbuf);
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
                            lex_syntaxerror(lex, "Unterminated block comment");
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
                int c = lex->c;
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
                    do
                    {
                        lex_savenext(lex);
                    }
                    while(tea_char_isident(lex->c) || tea_char_isdigit(lex->c));

                    return lex_name(lex);
                }
                else if(tea_char_isdigit(lex->c))
                {
                    return lex_number(lex);
                }
                else
                    lex_syntaxerror(lex, "Unexpected character");
            }
        }
    }
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

bool tea_lex_init(tea_State* T, Lexer* lex)
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