#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "tea_common.h"
#include "scanner/tea_scanner.h"

void tea_init_scanner(TeaState* state, TeaScanner* scanner, const char* source)
{
    scanner->state = state;
    
    scanner->start = source;
    scanner->current = source;
    scanner->line = 1;
}

static bool is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_digit(char c)
{
    return c >= '0' && c <= '9';
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

static int hex_digit(TeaScanner* scanner)
{
    char c = advance(scanner);
    
    if(c >= '0' && c <= '9')
    {
        return c - '0';
    }

    if(c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }

    if(c >= 'A' && c<= 'F')
    {
        return c - 'A' + 10;
    }

    scanner->current--;

    return -1;
}

static int binary_digit(TeaScanner* scanner)
{
    char c = advance(scanner);

    if(c >= '0' && c <= '1')
    {
        return c - '0';
    }

    scanner->current--;

    return -1;
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
                    case 'o': return check_keyword(scanner, 2, 6, "ntinue", TOKEN_CONTINUE);
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
                }
            }
            break;
        }
        case 'n': return check_keyword(scanner, 1, 3, "ull", TOKEN_NULL);
        case 'o': return check_keyword(scanner, 1, 1, "r", TOKEN_OR);
        case 'r': return check_keyword(scanner, 1, 5, "eturn", TOKEN_RETURN);
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
		return error_token(scanner, "Number too big.");
	}

	TeaToken token = make_token(scanner, TOKEN_NUMBER);
	token.value = value;

	return token;
}

static TeaToken hex_number(TeaScanner* scanner)
{
    while(hex_digit(scanner) != -1) continue;

    return make_number_token(scanner, true, false);
}

static TeaToken binary_number(TeaScanner* scanner)
{
    while(binary_digit(scanner) != -1) continue;

    return make_number_token(scanner, false, true);
}

static TeaToken number(TeaScanner* scanner)
{
    if(scanner->start[0] == '0')
    {
        if(match(scanner, 'x') || match(scanner, 'X'))
        {
            return hex_number(scanner);
        }
        if(match(scanner, 'b') || match(scanner, 'B'))
        {
            return binary_number(scanner);
        }
    }

    while(is_digit(peek(scanner)))
        advance(scanner);

    // Look for a fractional part.
    if(peek(scanner) == '.' && is_digit(peek_next(scanner)))
    {
        // Consume the ".".
        advance(scanner);

        while(is_digit(peek(scanner)))
            advance(scanner);
    }

    if(match(scanner, 'e') || match(scanner, 'E'))
    {
        if(!match(scanner, '+'))
        {
            match(scanner, '-');
        }

        if(!is_digit(peek(scanner)))
        {
            return error_token(scanner, "Unterminated scientific notation.");
        }

        while(is_digit(peek(scanner))) advance(scanner);
    }

    return make_number_token(scanner, false, false);
}

static TeaToken string(TeaScanner* scanner, char string_token)
{
    while(peek(scanner) != string_token && !is_at_end(scanner))
    {
        if(peek(scanner) == '\n')
            scanner->line++;
        advance(scanner);
    }

    if(is_at_end(scanner))
        return error_token(scanner, "Unterminated string.");

    advance(scanner);

    TeaToken token = make_token(scanner, TOKEN_STRING);
    token.value = OBJECT_VAL(tea_copy_string(scanner->state, scanner->start + 1, (int)(scanner->current - scanner->start - 2)));

    return token;
}

TeaToken tea_scan_token(TeaScanner* scanner)
{
    skip_whitespace(scanner);
    scanner->start = scanner->current;

    if(is_at_end(scanner))
        return make_token(scanner, TOKEN_EOF);

    char c = advance(scanner);

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
            if(!match(scanner, '.'))
            {
                return make_token(scanner, TOKEN_DOT);
            }
            return match_token(scanner, '.', TOKEN_DOT_DOT_DOT, TOKEN_DOT_DOT);
        }
        case '-': return match_tokens(scanner, '=', '-', TOKEN_MINUS_EQUAL, TOKEN_MINUS_MINUS, TOKEN_MINUS);
        case '+': return match_tokens(scanner, '=', '+', TOKEN_PLUS_EQUAL, TOKEN_PLUS_PLUS, TOKEN_PLUS);
        case '*': return make_token(scanner, match(scanner, '=') ? TOKEN_STAR_EQUAL : TOKEN_STAR);
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

    return error_token(scanner, "Unexpected character.");
}