/*
** tea_lexer.c
** Teascript lexer
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define tea_lexer_c
#define TEA_CORE

#include "tea_def.h"
#include "tea_value.h"
#include "tea_lexer.h"
#include "tea_string.h"
#include "tea_utf.h"

void tea_lex_init(TeaState* T, TeaLexer* lex, const char* source)
{
    /* Skip the UTF-8 BOM if there is one */
    if(strncmp(source, "\xEF\xBB\xBF", 3) == 0) source += 3;

    lex->T = T;
    lex->start = source;
    lex->current = source;
    lex->line = 1;
    lex->num_braces = 0;
    lex->raw = false;
}

static bool is_at_end(TeaLexer* lex)
{
    return *lex->current == '\0';
}

static char advance_char(TeaLexer* lex)
{
    lex->current++;

    return lex->current[-1];
}

static char peek(TeaLexer* lex)
{
    return *lex->current;
}

static char peek_next(TeaLexer* lex)
{
    if(is_at_end(lex))
        return '\0';

    return lex->current[1];
}

static bool match_char(TeaLexer* lex, char expected)
{
    if(is_at_end(lex))
        return false;

    if(*lex->current != expected)
        return false;

    lex->current++;

    return true;
}

static TeaToken make_token(TeaLexer* lex, TeaTokenType type)
{
    TeaToken token;
    token.type = type;
    token.start = lex->start;
    token.length = (int)(lex->current - lex->start);
    token.line = lex->line;

    return token;
}

static TeaToken match_tokens(TeaLexer* lex, char cr, char cb, TeaTokenType a, TeaTokenType b, TeaTokenType c)
{
    return make_token(lex, match_char(lex, cr) ? a : match_char(lex, cb) ? b : c);
}

static TeaToken match_token(TeaLexer* lex, char c, TeaTokenType a, TeaTokenType b)
{
    return make_token(lex, match_char(lex, c) ? a : b);
}

static TeaToken error_token(TeaLexer* lex, const char* message)
{
    TeaToken token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = lex->line;
    
    return token;
}

static void skip_line_comment(TeaLexer* lex)
{
    while(peek(lex) != '\n' && !is_at_end(lex)) 
        advance_char(lex);
}

static bool skip_whitespace(TeaLexer* lex)
{
    while(true)
    {
        char c = peek(lex);
        switch(c)
        {
            case ' ':
            case '\r':
            case '\t':
            {
                advance_char(lex);
                break;
            }
            case '\n':
            {
                lex->line++;
                advance_char(lex);
                break;
            }
            case '#':
            {
                /* Ignore shebang on first line */
                if(lex->line == 1 && peek_next(lex) == '!')
                {
                    skip_line_comment(lex);
                }
                else
                {
                    return false;
                }
                break;
            }
            case '/':
            {
                if(peek_next(lex) == '/') 
                {
                    skip_line_comment(lex);
                } 
                else if(peek_next(lex) == '*') 
                {
                    advance_char(lex);
                    advance_char(lex);

                    int nesting = 1;
                    while(nesting > 0)
                    {
                        if(peek(lex) == '\0')
                        {
                            return true;
                        }

                        if(peek(lex) == '/' && peek_next(lex) == '*')
                        {
                            advance_char(lex);
                            advance_char(lex);
                            nesting++;
                            continue;
                        }

                        if(peek(lex) == '*' && peek_next(lex) == '/')
                        {
                            advance_char(lex);
                            advance_char(lex);
                            nesting--;
                            continue;
                        }

                        if(peek(lex) == '\n')
                            lex->line++;
                        advance_char(lex);
                    }
                }
                else
                {
                    return false;
                }
                break;
            }
            default:
                return false;
        }
    }
}

static TeaTokenType check_keyword(TeaLexer* lex, int start, int length, const char* rest, TeaTokenType type)
{
    if(lex->current - lex->start == start + length && memcmp(lex->start + start, rest, length) == 0)
    {
        return type;
    }

    return TOKEN_NAME;
}

static TeaTokenType identifier_type(TeaLexer* lex)
{
    switch(lex->start[0])
    {
        case 'a':
        {
            if(lex->current - lex->start > 1)
            {
                switch(lex->start[1])
                {
                    case 'n': return check_keyword(lex, 2, 1, "d", TOKEN_AND);
                    case 's': return check_keyword(lex, 2, 0, "", TOKEN_AS);
                }
            }
            break;
        }
        case 'b': return check_keyword(lex, 1, 4, "reak", TOKEN_BREAK);
        case 'c':
        {
            if(lex->current - lex->start > 1)
            {
                switch(lex->start[1])
                {
                    case 'a': return check_keyword(lex, 2, 2, "se", TOKEN_CASE);
                    case 'l': return check_keyword(lex, 2, 3, "ass", TOKEN_CLASS);
                    case 'o': 
                    {
                        if(lex->current - lex->start > 3)
                        {
                            switch(lex->start[3])
                            {
                                case 's': return check_keyword(lex, 2, 3, "nst", TOKEN_CONST);
                                case 't': return check_keyword(lex, 2, 6, "ntinue", TOKEN_CONTINUE);
                            }
                        }
                    }
                    
                }
            }
            break;
        }
        case 'd':
        {
            if(lex->current - lex->start > 1)
            {
                switch(lex->start[1])
                {
                    case 'e': return check_keyword(lex, 2, 5, "fault", TOKEN_DEFAULT);
                    case 'o': return check_keyword(lex, 2, 0, "", TOKEN_DO);
                }
            }
            break;
        }
        case 'e':
        {
            if(lex->current - lex->start > 1)
            {
                switch(lex->start[1])
                {
                    case 'n': return check_keyword(lex, 2, 2, "um", TOKEN_ENUM);
                    case 'l': return check_keyword(lex, 2, 2, "se", TOKEN_ELSE);
                }
            }
            break;
        }
        case 'f':
        {
            if(lex->current - lex->start > 1)
            {
                switch(lex->start[1])
                {
                    case 'a': return check_keyword(lex, 2, 3, "lse", TOKEN_FALSE);
                    case 'o': return check_keyword(lex, 2, 1, "r", TOKEN_FOR);
                    case 'u': return check_keyword(lex, 2, 6, "nction", TOKEN_FUNCTION);
                    case 'r': return check_keyword(lex, 2, 2, "om", TOKEN_FROM);
                }
            }
            break;
        }
        case 'i':
        {
            if(lex->current - lex->start > 1)
            {
                switch(lex->start[1])
                {
                    case 'f': return check_keyword(lex, 2, 0, "", TOKEN_IF);
                    case 'm': return check_keyword(lex, 2, 4, "port", TOKEN_IMPORT);
                    case 'n': return check_keyword(lex, 2, 0, "", TOKEN_IN);
                    case 's': return check_keyword(lex, 2, 0, "", TOKEN_IS);
                }
            }
            break;
        }
        case 'n':
        {
            if(lex->current - lex->start > 1)
            {
                switch(lex->start[1])
                {
                    case 'o': return check_keyword(lex, 2, 1, "t", TOKEN_BANG);
                    case 'u': return check_keyword(lex, 2, 2, "ll", TOKEN_NULL);
                }
            }
            break;
        }
        case 'o': return check_keyword(lex, 1, 1, "r", TOKEN_OR);
        case 'r': 
        {
            if(lex->current - lex->start > 1)
            {
                switch(lex->start[1])
                {
                    case 'e': return check_keyword(lex, 1, 5, "eturn", TOKEN_RETURN);
                }
            }
            break;
        }
        case 's':
        {
            if(lex->current - lex->start > 1)
            {
                switch(lex->start[1])
                {
                    case 'u': return check_keyword(lex, 2, 3, "per", TOKEN_SUPER);
                    case 'w': return check_keyword(lex, 2, 4, "itch", TOKEN_SWITCH);
                    case 't': return check_keyword(lex, 2, 4, "atic", TOKEN_STATIC);
                }
            }
            break;
        }
        case 't':
        {
            if(lex->current - lex->start > 1)
            {
                switch(lex->start[1])
                {
                    case 'h': return check_keyword(lex, 2, 2, "is", TOKEN_THIS);
                    case 'r': return check_keyword(lex, 2, 2, "ue", TOKEN_TRUE);
                }
            }
            break;
        }
        case 'v': return check_keyword(lex, 1, 2, "ar", TOKEN_VAR);
        case 'w':
        {
            if(lex->current - lex->start > 1)
            {
                switch(lex->start[1])
                {
                    case 'h': return check_keyword(lex, 2, 3, "ile", TOKEN_WHILE);
                }
            }
            break;
        }
    }

    return TOKEN_NAME;
}

static TeaToken identifier(TeaLexer* lex)
{
    while(is_alpha(peek(lex)) || is_digit(peek(lex)))
        advance_char(lex);

    return make_token(lex, identifier_type(lex));
}

static TeaToken make_number_token(TeaLexer* lex, bool strip, bool is_hex, bool is_bin, bool is_octal)
{
    errno = 0;
	TeaValue value;

    if(strip)
    {
        int len = (int)(lex->current - lex->start);
        char* buffer = TEA_ALLOCATE(lex->T, char, len + 1);
        char* current = buffer;

        /* Strip it of any underscores */
        for(int i = 0; i < len; i++)
        {
            char c = lex->start[i];

            if(c != '_')
            {
                *(current++) = c;
            }
        }

        *current = '\0';

        if(is_hex) 
        {
            value = NUMBER_VAL((double)strtoll(buffer, NULL, 16));
        } 
        else if(is_bin) 
        {
            value = NUMBER_VAL((int)strtoll(buffer + 2, NULL, 2));
        } 
        else if(is_octal)
        {
            value = NUMBER_VAL((int)strtoll(buffer + 2, NULL, 8));
        }
        else 
        {
            value = NUMBER_VAL(strtod(buffer, NULL));
        }
        
        TEA_FREE_ARRAY(lex->T, char, buffer, len + 1);
        goto done;
    }

	if(is_hex) 
    {
		value = NUMBER_VAL((double)strtoll(lex->start, NULL, 16));
	} 
    else if(is_bin) 
    {
		value = NUMBER_VAL((int)strtoll(lex->start + 2, NULL, 2));
	} 
    else if(is_octal)
    {
        value = NUMBER_VAL((int)strtoll(lex->start + 2, NULL, 8));
    }
    else 
    {
		value = NUMBER_VAL(strtod(lex->start, NULL));
	}

    done:
	if(errno == ERANGE) 
    {
		errno = 0;
		return error_token(lex, "Number too big");
	}

	TeaToken token = make_token(lex, TOKEN_NUMBER);
	token.value = value;

	return token;
}

static TeaToken hex_number(TeaLexer* lex)
{
    bool underscore, last = false;

    while(is_hex_digit(peek(lex)) || peek(lex) == '_')
    {
        if(peek(lex) == '_')
        {
            if(!underscore) underscore = true;
            if(last) return error_token(lex, "Cannot have consecutive underscores");
            advance_char(lex);
            last = true;
        }
        else
        {
            advance_char(lex);
            last = false;
        }
    }

    if(last) return error_token(lex, "Invalid hex number");

    return make_number_token(lex, underscore, true, false, false);
}

static TeaToken binary_number(TeaLexer* lex)
{
    bool underscore, last = false;

    while(is_binary_digit(peek(lex)) || peek(lex) == '_')
    {
        if(peek(lex) == '_')
        {
            if(!underscore) underscore = true;
            if(last) return error_token(lex, "Cannot have consecutive underscores");
            advance_char(lex);
            last = true;
        }
        else
        {
            advance_char(lex);
            last = false;
        }
    }

    if(last) return error_token(lex, "Cannot have leading underscores");

    return make_number_token(lex, underscore, false, true, false);
}

static TeaToken octal_number(TeaLexer* lex)
{
    bool underscore, last = false;

    while(is_octal_digit(peek(lex)) || peek(lex) == '_')
    {
        if(peek(lex) == '_')
        {
            if(!underscore) underscore = true;
            if(last) return error_token(lex, "Cannot have consecutive underscores");
            advance_char(lex);
            last = true;
        }
        else
        {
            advance_char(lex);
            last = false;
        }
    }

    if(last) return error_token(lex, "Cannot have leading underscores");

    return make_number_token(lex, underscore, false, false, true);
}

static TeaToken exponent_number(TeaLexer* lex)
{
    if(!match_char(lex, '+'))
    {
        match_char(lex, '-');
    }

    if(!is_digit(peek(lex)))
    {
        return error_token(lex, "Unterminated scientific notation");
    }

    bool underscore, last = false;

    while(is_digit(peek(lex)) || peek(lex) == '_')
    {
        if(peek(lex) == '_')
        {
            if(!underscore) underscore = true;
            if(last) return error_token(lex, "Cannot have consecutive underscores");
            advance_char(lex);
            last = true;
        }
        else
        {
            advance_char(lex);
            last = false;
        }
    }

    if(last) return error_token(lex, "Cannot have leading underscores");

    return make_number_token(lex, underscore, false, false, false);
}

static TeaToken number(TeaLexer* lex)
{
    bool underscore, last = false;

    while(is_digit(peek(lex)) || peek(lex) == '_')
    {
        if(peek(lex) == '_')
        {
            if(!underscore) underscore = true;
            if(last) return error_token(lex, "Cannot have consecutive underscores");
            advance_char(lex);
            last = true;
        }
        else
        {
            advance_char(lex);
            last = false;
        }
    }

    /* Look for a fractional part */
    if(peek(lex) == '.' && is_digit(peek_next(lex)))
    {
        /* Consume the "." */
        advance_char(lex);
        last = false;

        while(is_digit(peek(lex)) || peek(lex) == '_')
        {
            if(peek(lex) == '_')
            {
                if(!underscore) underscore = true;
                if(last) return error_token(lex, "Cannot have consecutive underscores");
                advance_char(lex);
                last = true;
            }
            else
            {
                advance_char(lex);
                last = false;
            }
        }
    }

    if(match_char(lex, 'e') || match_char(lex, 'E'))
    {
        return exponent_number(lex);
    }

    if(last) return error_token(lex, "Cannot have leading underscores");

    return make_number_token(lex, underscore, false, false, false);
}

static int read_hex_digit(TeaLexer* lex)
{
    char c = advance_char(lex);
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;

    return -1;
}

static int read_hex_escape(TeaLexer* lex, int digits)
{
    int value = 0;
    for(int i = 0; i < digits; i++)
    {
        if(peek(lex) == lex->string || peek(lex) == '\0')
        {
            return -1;
        }

        int digit = read_hex_digit(lex);
        if(digit == -1)
        {
            return -1;
        }

        value = (value * 16) | digit;
    }

    return value;
}

static bool read_unicode_escape(TeaLexer* lex, TeaBytes* bytes, int length)
{
    int value = read_hex_escape(lex, length);
    if(value == -1) return true;

    int num_bytes = tea_utf_encode_bytes(value);
    if(num_bytes != 0)
    {
        tea_fill_bytes(lex->T, bytes, 0, num_bytes);
        tea_utf_encode(value, bytes->values + bytes->count - num_bytes);
    }
    return false;
}

static TeaToken string(TeaLexer* lex, bool interpolation)
{
    TeaState* T = lex->T;
    TeaTokenType type = TOKEN_STRING;

    TeaBytes bytes;
    tea_init_bytes(&bytes);

    while(true)
    {
        char c = advance_char(lex);

        if(c == lex->string)
        {
            break;
        }
        else if(interpolation && c == '{')
        {
            if(lex->num_braces >= 4)
            {
                tea_free_bytes(T, &bytes);
				return error_token(lex, "String interpolation is too deep");
			}

			type = TOKEN_INTERPOLATION;
			lex->braces[lex->num_braces++] = 1;
			break;
        }

        if(c == '\r') continue;
        if(c == '\t') continue;

        switch(c)
        {
            case '\0':
            {
                tea_free_bytes(T, &bytes);
                return error_token(lex, "Unterminated string");
            }
            case '\n':
            {
                lex->line++;
                tea_write_bytes(T, &bytes, c);
                break;
            }
            case '\\':
            {
                if(lex->raw)
                {
                    tea_write_bytes(T, &bytes, c);
                    break;
                }

                switch(advance_char(lex))
                {
                    case '\"': tea_write_bytes(T, &bytes, '\"'); break;
                    case '\'': tea_write_bytes(T, &bytes, '\''); break;
                    case '\\': tea_write_bytes(T, &bytes, '\\'); break;
                    case '0': tea_write_bytes(T, &bytes, '\0'); break;
                    case '{': tea_write_bytes(T, &bytes, '{'); break;
                    case 'a': tea_write_bytes(T, &bytes, '\a'); break;
                    case 'b': tea_write_bytes(T, &bytes, '\b'); break;
                    case 'f': tea_write_bytes(T, &bytes, '\f'); break;
                    case 'n': tea_write_bytes(T, &bytes, '\n'); break;
                    case 'r': tea_write_bytes(T, &bytes, '\r'); break;
                    case 't': tea_write_bytes(T, &bytes, '\t'); break;
                    case 'v': tea_write_bytes(T, &bytes, '\v'); break;
                    case 'x':
                    {
                        int h = read_hex_escape(lex, 2);
                        if(h == -1)
                        {
                            tea_free_bytes(T, &bytes);
                            return error_token(lex, "Incomplete byte escape sequence.");
                        }
                        tea_write_bytes(T, &bytes, (uint8_t)h);
                        break;
                    }
                    case 'u':
                    {
                        bool e = read_unicode_escape(lex, &bytes, 4);
                        if(e)
                        {
                            tea_free_bytes(T, &bytes);
                            return error_token(lex, "Incomplete unicode escape sequence.");
                        }
                        break;
                    }
                    case 'U':
                    {
                        bool e = read_unicode_escape(lex, &bytes, 8);
                        if(e)
                        {
                            tea_free_bytes(T, &bytes);
                            return error_token(lex, "Incomplete unicode escape sequence.");
                        }
                        break;
                    }
                    default: 
                    {
                        tea_free_bytes(T, &bytes);
                        return error_token(lex, "Invalid escape character");
                    }
                }
                break;
            }
            default:
            {
                tea_write_bytes(T, &bytes, c);
                break;
            }
        }
    }

    lex->raw = false;
    TeaToken token = make_token(lex, type);
	token.value = OBJECT_VAL(tea_str_copy(T, (const char*)bytes.values, bytes.count));
	tea_free_bytes(T, &bytes);

	return token;
}

void tea_lex_backtrack(TeaLexer* lex)
{
    lex->current--;
}

TeaToken tea_lex_token(TeaLexer* lex)
{
    if(skip_whitespace(lex))
    {
        return error_token(lex, "Unterminated block comment");
    }
    lex->start = lex->current;

    if(is_at_end(lex))
        return make_token(lex, TOKEN_EOF);

    char c = advance_char(lex);

    if(c == '0')
    {
        if(peek(lex) == 'x' || peek(lex) == 'X')
        {
            advance_char(lex);
            return hex_number(lex);
        }
        if(peek(lex) == 'b' || peek(lex) == 'B')
        {
            advance_char(lex);
            return binary_number(lex);
        }
        if(peek(lex) == 'c' || peek(lex) == 'C')
        {
            advance_char(lex);
            return octal_number(lex);
        }
    }
    else if(c == 'r' || c == 'f')
    {
        if(peek(lex) == '"' || peek(lex) == '\'')
        {
            lex->raw = (c == 'r');
            lex->string = advance_char(lex);
            return string(lex, c == 'f');
        }
    }

    if(is_alpha(c))
        return identifier(lex);

    if(is_digit(c))
        return number(lex);

    switch(c)
    {
        case '(': return make_token(lex, TOKEN_LEFT_PAREN);
        case ')': return make_token(lex, TOKEN_RIGHT_PAREN);
        case '[': return make_token(lex, TOKEN_LEFT_BRACKET);
        case ']': return make_token(lex, TOKEN_RIGHT_BRACKET);
        case '{':
        {
            if(lex->num_braces > 0)
            {
                lex->braces[lex->num_braces - 1]++;
			}

            return make_token(lex, TOKEN_LEFT_BRACE);
        }
        case '}':
        {
            if(lex->num_braces > 0 && --lex->braces[lex->num_braces - 1] == 0)
            {
				lex->num_braces--;

				return string(lex, true);
			}

            return make_token(lex, TOKEN_RIGHT_BRACE);
        }
        case ',': return make_token(lex, TOKEN_COMMA);
        case ';': return make_token(lex, TOKEN_SEMICOLON);
        case ':': return make_token(lex, TOKEN_COLON);
        case '?': return make_token(lex, TOKEN_QUESTION);
        case '.':
        {
            if(is_digit(peek(lex)))
            {
                return number(lex);
            }
            if(!match_char(lex, '.'))
            {
                return make_token(lex, TOKEN_DOT);
            }
            return match_token(lex, '.', TOKEN_DOT_DOT_DOT, TOKEN_DOT_DOT);
        }
        case '-': return match_tokens(lex, '=', '-', TOKEN_MINUS_EQUAL, TOKEN_MINUS_MINUS, TOKEN_MINUS);
        case '+': return match_tokens(lex, '=', '+', TOKEN_PLUS_EQUAL, TOKEN_PLUS_PLUS, TOKEN_PLUS);
        case '*':
        {
            if(match_char(lex, '='))
            {
                return make_token(lex, TOKEN_STAR_EQUAL);
            }
            else if(match_char(lex, '*'))
            {
                if(match_char(lex, '='))
                {
                    return make_token(lex, TOKEN_STAR_STAR_EQUAL);
                }
                else
                {
                    return make_token(lex, TOKEN_STAR_STAR);
                }
            }
            else
            {
                return make_token(lex, TOKEN_STAR);
            }
        }
        case '/': return make_token(lex, match_char(lex, '=') ? TOKEN_SLASH_EQUAL : TOKEN_SLASH);
        case '%': return make_token(lex, match_char(lex, '=') ? TOKEN_PERCENT_EQUAL : TOKEN_PERCENT);
        case '&': return make_token(lex, match_char(lex, '=') ? TOKEN_AMPERSAND_EQUAL : TOKEN_AMPERSAND);
        case '|': return make_token(lex, match_char(lex, '=') ? TOKEN_PIPE_EQUAL : TOKEN_PIPE);
        case '^': return make_token(lex, match_char(lex, '=') ? TOKEN_CARET_EQUAL : TOKEN_CARET);
        case '~': return make_token(lex, TOKEN_TILDE);
        case '!': return make_token(lex, match_char(lex, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=':
        {
            if(match_char(lex, '='))
            {
                return make_token(lex, TOKEN_EQUAL_EQUAL);
            }
            else if(match_char(lex, '>'))
            {
                return make_token(lex, TOKEN_ARROW);
            }
            else
            {
                return make_token(lex, TOKEN_EQUAL);
            }
        }
        case '<': return match_tokens(lex, '=', '<', TOKEN_LESS_EQUAL, TOKEN_LESS_LESS, TOKEN_LESS);
        case '>': return match_tokens(lex, '=', '>', TOKEN_GREATER_EQUAL, TOKEN_GREATER_GREATER, TOKEN_GREATER);
        case '"':
        {
            lex->string = '"';
            return string(lex, false);
        }
        case '\'':
        {
            lex->string = '\'';
            return string(lex, false);
        }
    }

    return error_token(lex, "Unexpected character");
}