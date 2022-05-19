#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "tea_common.h"
#include "tea_value.h"
#include "tea_scanner.h"

void tea_init_scanner(TeaState* state, TeaScanner* scanner, const char* source)
{
    scanner->state = state;
    
    scanner->start = source;
    scanner->current = source;
    scanner->line = 1;

    scanner->is_raw = false;
}

static bool is_at_end(TeaScanner* scanner)
{
    return *scanner->current == '\0';
}

static char advance(TeaScanner* scanner)
{
    scanner->current++;

    return scanner->current[-1];
}

static char peek(TeaScanner* scanner)
{
    return *scanner->current;
}

static char peek_next(TeaScanner* scanner)
{
    if(is_at_end(scanner))
        return '\0';

    return scanner->current[1];
}

static bool match(TeaScanner* scanner, char expected)
{
    if(is_at_end(scanner))
        return false;

    if(*scanner->current != expected)
        return false;

    scanner->current++;

    return true;
}

static TeaToken make_token(TeaScanner* scanner, TeaTokenType type)
{
    TeaToken token;
    token.type = type;
    token.start = scanner->start;
    token.length = (int)(scanner->current - scanner->start);
    token.line = scanner->line;

    return token;
}

static TeaToken match_tokens(TeaScanner* scanner, char cr, char cb, TeaTokenType a, TeaTokenType b, TeaTokenType c)
{
    return make_token(scanner, match(scanner, cr) ? a : match(scanner, cb) ? b : c);
}

static TeaToken match_token(TeaScanner* scanner, char c, TeaTokenType a, TeaTokenType b)
{
    return make_token(scanner, match(scanner, c) ? a : b);
}

static TeaToken error_token(TeaScanner* scanner, const char *message)
{
    TeaToken token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = scanner->line;
    
    return token;
}

static void skip_whitespace(TeaScanner* scanner)
{
    while(true)
    {
        char c = peek(scanner);
        switch(c)
        {
            case ' ':
            case ';':
            case '\r':
            case '\t':
                advance(scanner);
                break;
            case '\n':
                scanner->line++;
                advance(scanner);
                break;
            case '/':
                if(peek_next(scanner) == '*') 
                {
                    advance(scanner);
                    advance(scanner);
                    while(true) 
                    {
                        while(peek(scanner) != '*' && !is_at_end(scanner)) 
                        {
                            if((c = advance(scanner)) == '\n') 
                            {
                                scanner->line++;
                            }
                        }

                        if(is_at_end(scanner))
                            return;

                        if(peek_next(scanner) == '/') 
                        {
                            break;
                        }
                        advance(scanner);
                    }
                    advance(scanner);
                    advance(scanner);
                } 
                else if(peek_next(scanner) == '/') 
                {
                    while(peek(scanner) != '\n' && !is_at_end(scanner)) advance(scanner);
                } 
                else 
                {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static TeaTokenType check_keyword(TeaScanner* scanner, int start, int length, const char* rest, TeaTokenType type)
{
    if(scanner->current - scanner->start == start + length && memcmp(scanner->start + start, rest, length) == 0)
    {
        return type;
    }

    return TOKEN_NAME;
}

static TeaTokenType identifier_type(TeaScanner* scanner)
{
    switch(scanner->start[0])
    {
        case 'a':
        {
            if(scanner->current - scanner->start > 1)
            {
                switch(scanner->start[1])
                {
                    case 'n': return check_keyword(scanner, 2, 1, "d", TOKEN_AND);
                    case 's': return check_keyword(scanner, 2, 0, "", TOKEN_AS);
                }
            }
            break;
        }
        case 'b': return check_keyword(scanner, 1, 4, "reak", TOKEN_BREAK);
        case 'c':
        {
            if(scanner->current - scanner->start > 1)
            {
                switch(scanner->start[1])
                {
                    case 'a': return check_keyword(scanner, 2, 2, "se", TOKEN_CASE);
                    case 'l': return check_keyword(scanner, 2, 3, "ass", TOKEN_CLASS);
                    case 'o': 
                    {
                        if(scanner->current - scanner->start > 3)
                        {
                            switch(scanner->start[3])
                            {
                                case 's': return check_keyword(scanner, 2, 3, "nst", TOKEN_CONST);
                                case 't': return check_keyword(scanner, 2, 6, "ntinue", TOKEN_CONTINUE);
                            }
                        }
                    }
                    
                }
            }
            break;
        }
        case 'd':
        {
            if(scanner->current - scanner->start > 1)
            {
                switch(scanner->start[1])
                {
                    case 'e': return check_keyword(scanner, 2, 5, "fault", TOKEN_DEFAULT);
                    case 'o': return check_keyword(scanner, 2, 0, "", TOKEN_DO);
                }
            }
            break;
        }
        case 'e': return check_keyword(scanner, 1, 3, "lse", TOKEN_ELSE);
        case 'f':
        {
            if(scanner->current - scanner->start > 1)
            {
                switch(scanner->start[1])
                {
                    case 'a': return check_keyword(scanner, 2, 3, "lse", TOKEN_FALSE);
                    case 'o': return check_keyword(scanner, 2, 1, "r", TOKEN_FOR);
                    case 'u': return check_keyword(scanner, 2, 6, "nction", TOKEN_FUNCTION);
                    case 'r': return check_keyword(scanner, 2, 2, "om", TOKEN_FROM);
                }
            }
            break;
        }
        case 'i':
        {
            if(scanner->current - scanner->start > 1)
            {
                switch(scanner->start[1])
                {
                    case 'f': return check_keyword(scanner, 2, 0, "", TOKEN_IF);
                    case 'm': return check_keyword(scanner, 2, 4, "port", TOKEN_IMPORT);
                    case 'n': return check_keyword(scanner, 2, 0, "", TOKEN_IN);
                    case 's': return check_keyword(scanner, 2, 0, "", TOKEN_IS);
                }
            }
            break;
        }
        case 'n':
        {
            if(scanner->current - scanner->start > 1)
            {
                switch(scanner->start[1])
                {
                    case 'o': return check_keyword(scanner, 2, 1, "t", TOKEN_BANG);
                    case 'u': return check_keyword(scanner, 2, 2, "ll", TOKEN_NULL);
                }
            }
            break;
        }
        case 'o': return check_keyword(scanner, 1, 1, "r", TOKEN_OR);
        case 'r': 
        {
            if(scanner->current - scanner->start > 1)
            {
                switch(scanner->start[1])
                {
                    case 'e': return check_keyword(scanner, 1, 5, "eturn", TOKEN_RETURN);
                }
            }
            else
            {
                if(scanner->start[1] == '"' || scanner->start[1] == '\'')
                {
                    scanner->is_raw = true;
                    return TOKEN_R;
                }
            }
            break;
        }
        case 's':
        {
            if(scanner->current - scanner->start > 1)
            {
                switch(scanner->start[1])
                {
                    case 'u': return check_keyword(scanner, 2, 3, "per", TOKEN_SUPER);
                    case 'w': return check_keyword(scanner, 2, 4, "itch", TOKEN_SWITCH);
                }
            }
            break;
        }
        case 't':
        {
            if(scanner->current - scanner->start > 1)
            {
                switch(scanner->start[1])
                {
                    case 'h': return check_keyword(scanner, 2, 2, "is", TOKEN_THIS);
                    case 'r': return check_keyword(scanner, 2, 2, "ue", TOKEN_TRUE);
                }
            }
            break;
        }
        case 'v': return check_keyword(scanner, 1, 2, "ar", TOKEN_VAR);
        case 'w':
        {
            if(scanner->current - scanner->start > 1)
            {
                switch(scanner->start[1])
                {
                    case 'h': return check_keyword(scanner, 2, 3, "ile", TOKEN_WHILE);
                    case 'i': return check_keyword(scanner, 2, 2, "th", TOKEN_WITH);
                }
            }
            break;
        }
    }

    return TOKEN_NAME;
}

static TeaToken identifier(TeaScanner* scanner)
{
    while(is_alpha(peek(scanner)) || is_digit(peek(scanner)))
        advance(scanner);

    return make_token(scanner, identifier_type(scanner));
}

static TeaToken make_number_token(TeaScanner* scanner, bool is_hex, bool is_bin)
{
    errno = 0;
	TeaValue value;

	if(is_hex) 
    {
		value = NUMBER_VAL((double)strtoll(scanner->start, NULL, 16));
	} 
    else if(is_bin) 
    {
		value = NUMBER_VAL((int)strtoll(scanner->start + 2, NULL, 2));
	} 
    else 
    {
		value = NUMBER_VAL(strtod(scanner->start, NULL));
	}

	if(errno == ERANGE) 
    {
		errno = 0;
		return error_token(scanner, "Number too big");
	}

	TeaToken token = make_token(scanner, TOKEN_NUMBER);
	token.value = value;

	return token;
}

static TeaToken hex_number(TeaScanner* scanner)
{
    if(!is_hex_digit(peek(scanner)))
    {
        return error_token(scanner, "Invalid hex literal");
    }
    while(is_hex_digit(peek(scanner))) advance(scanner);

    return make_number_token(scanner, true, false);
}

static TeaToken binary_number(TeaScanner* scanner)
{
    if(!is_binary_digit(peek(scanner)))
    {
        return error_token(scanner, "Invalid binary literal");
    }
    while(is_binary_digit(peek(scanner))) advance(scanner);

    return make_number_token(scanner, false, true);
}

static TeaToken exponent_number(TeaScanner* scanner)
{
    if(!match(scanner, '+'))
    {
        match(scanner, '-');
    }

    if(!is_digit(peek(scanner)))
    {
        return error_token(scanner, "Unterminated scientific notation");
    }

    while(is_digit(peek(scanner))) advance(scanner);

    return make_number_token(scanner, false, false);
}

static TeaToken number(TeaScanner* scanner)
{
    while(is_digit(peek(scanner)))
        advance(scanner);

    // Look for a fractional part
    if(peek(scanner) == '.' && is_digit(peek_next(scanner)))
    {
        // Consume the "."
        advance(scanner);

        while(is_digit(peek(scanner)))
            advance(scanner);
    }

    if(match(scanner, 'e') || match(scanner, 'E'))
    {
        return exponent_number(scanner);
    }

    return make_number_token(scanner, false, false);
}

static TeaToken string(TeaScanner* scanner, char string_token)
{
    TeaState* state = scanner->state;

    TeaBytes bytes;
    tea_init_bytes(&bytes);

    while(true)
    {
        char c = advance(scanner);

        if(c == string_token) break;
        if(c == '\r') continue;
        if(c == '\t') continue;

        switch(c)
        {
            case '\0':
            {
                return error_token(scanner, "Unterminated string");
            }
            case '\n':
            {
                scanner->line++;
                tea_write_bytes(state, &bytes, c);
                break;
            }
            case '\\':
            {
                if(scanner->is_raw)
                {
                    tea_write_bytes(state, &bytes, c);
                    break;
                }

                switch(advance(scanner))
                {
                    case '\"': tea_write_bytes(state, &bytes, '\"'); break;
                    case '\'': tea_write_bytes(state, &bytes, '\''); break;
                    case '\\': tea_write_bytes(state, &bytes, '\\'); break;
                    case '0': tea_write_bytes(state, &bytes, '\0'); break;
                    case 'a': tea_write_bytes(state, &bytes, '\a'); break;
                    case 'b': tea_write_bytes(state, &bytes, '\b'); break;
                    case 'f': tea_write_bytes(state, &bytes, '\f'); break;
                    case 'n': tea_write_bytes(state, &bytes, '\n'); break;
                    case 'r': tea_write_bytes(state, &bytes, '\r'); break;
                    case 't': tea_write_bytes(state, &bytes, '\t'); break;
                    case 'v': tea_write_bytes(state, &bytes, '\v'); break;
                    default: 
                    {
                        return error_token(scanner, "Invalid escape character");
                    }
                }
                break;
            }
            default:
            {
                tea_write_bytes(state, &bytes, c);
                break;
            }
        }
    }

    scanner->is_raw = false;
    TeaToken token = make_token(scanner, TOKEN_STRING);
	token.value = OBJECT_VAL(tea_copy_string(state, (const char*)bytes.values, bytes.count));
	tea_free_bytes(state, &bytes);

	return token;
}

void tea_back_track(TeaScanner* scanner)
{
    scanner->current--;
}

TeaToken tea_scan_token(TeaScanner* scanner)
{
    skip_whitespace(scanner);
    scanner->start = scanner->current;

    if(is_at_end(scanner))
        return make_token(scanner, TOKEN_EOF);

    char c = advance(scanner);

    if(c == '0')
    {
        if(peek(scanner) == 'x' || peek(scanner) == 'X')
        {
            advance(scanner);
            return hex_number(scanner);
        }
        if(peek(scanner) == 'b' || peek(scanner) == 'B')
        {
            advance(scanner);
            return binary_number(scanner);
        }
    }

    if(is_alpha(c))
        return identifier(scanner);

    if(is_digit(c))
        return number(scanner);

    switch(c)
    {
        case '(': return make_token(scanner, TOKEN_LEFT_PAREN);
        case ')': return make_token(scanner, TOKEN_RIGHT_PAREN);
        case '[': return make_token(scanner, TOKEN_LEFT_BRACKET);
        case ']': return make_token(scanner, TOKEN_RIGHT_BRACKET);
        case '{': return make_token(scanner, TOKEN_LEFT_BRACE);
        case '}': return make_token(scanner, TOKEN_RIGHT_BRACE);
        case ',': return make_token(scanner, TOKEN_COMMA);
        case ':': return make_token(scanner, TOKEN_COLON);
        case '?': return make_token(scanner, TOKEN_QUESTION);
        case '.':
        {
            if(is_digit(peek(scanner)))
            {
                return number(scanner);
            }
            if(!match(scanner, '.'))
            {
                return make_token(scanner, TOKEN_DOT);
            }
            return match_token(scanner, '.', TOKEN_DOT_DOT_DOT, TOKEN_DOT_DOT);
        }
        case '-': return match_tokens(scanner, '=', '-', TOKEN_MINUS_EQUAL, TOKEN_MINUS_MINUS, TOKEN_MINUS);
        case '+': return match_tokens(scanner, '=', '+', TOKEN_PLUS_EQUAL, TOKEN_PLUS_PLUS, TOKEN_PLUS);
        case '*':
        {
            if(match(scanner, '='))
            {
                return make_token(scanner, TOKEN_STAR_EQUAL);
            }
            else if(match(scanner, '*'))
            {
                if(match(scanner, '='))
                {
                    return make_token(scanner, TOKEN_STAR_STAR_EQUAL);
                }
                else
                {
                    return make_token(scanner, TOKEN_STAR_STAR);
                }
            }
            else
            {
                return make_token(scanner, TOKEN_STAR);
            }
        }
        case '/': return make_token(scanner, match(scanner, '=') ? TOKEN_SLASH_EQUAL : TOKEN_SLASH);
        case '%': return make_token(scanner, match(scanner, '=') ? TOKEN_PERCENT_EQUAL : TOKEN_PERCENT);
        case '&': return make_token(scanner, match(scanner, '=') ? TOKEN_AMPERSAND_EQUAL : TOKEN_AMPERSAND);
        case '|': return make_token(scanner, match(scanner, '=') ? TOKEN_PIPE_EQUAL : TOKEN_PIPE);
        case '^': return make_token(scanner, match(scanner, '=') ? TOKEN_CARET_EQUAL : TOKEN_CARET);
        case '~': return make_token(scanner, TOKEN_TILDE);
        case '!': return make_token(scanner, match(scanner, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=': return make_token(scanner, match(scanner, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<': return match_tokens(scanner, '=', '<', TOKEN_LESS_EQUAL, TOKEN_LESS_LESS, TOKEN_LESS);
        case '>': return match_tokens(scanner, '=', '>', TOKEN_GREATER_EQUAL, TOKEN_GREATER_GREATER, TOKEN_GREATER);
        case '"': return string(scanner, '"');
        case '\'': return string(scanner, '\'');
    }

    return error_token(scanner, "Unexpected character");
}