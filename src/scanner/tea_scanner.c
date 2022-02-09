#include <stdio.h>
#include <string.h>

#include "tea_common.h"
#include "scanner/tea_scanner.h"

typedef struct
{
    const char* start;
    const char* current;
    int line;
} TeaScanner;

TeaScanner scanner;

void tea_init_scanner(const char* source)
{
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
}

static bool is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static bool is_at_end()
{
    return *scanner.current == '\0';
}

static char advance()
{
    scanner.current++;

    return scanner.current[-1];
}

static char peek()
{
    return *scanner.current;
}

static char peek_next()
{
    if(is_at_end())
        return '\0';

    return scanner.current[1];
}

static bool match(char expected)
{
    if(is_at_end())
        return false;
    if(*scanner.current != expected)
        return false;
    scanner.current++;

    return true;
}

static int hex_digit()
{
    char c = advance();
    
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

    scanner.current--;

    return -1;
}

static int binary_digit()
{
    char c = advance();

    if(c >= '0' && c <= '1')
    {
        return c - '0';
    }

    scanner.current--;

    return -1;
}

static TeaToken make_token(TeaTokenType type)
{
    TeaToken token;
    token.type = type;
    token.start = scanner.start;
    token.length = (int)(scanner.current - scanner.start);
    token.line = scanner.line;

    return token;
}

static TeaToken error_token(const char *message)
{
    TeaToken token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = scanner.line;
    
    return token;
}

static void skip_whitespace()
{
    while(true)
    {
        char c = peek();
        switch(c)
        {
            case ' ':
            case ';':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                scanner.line++;
                advance();
                break;
            case '/':
                if(peek_next() == '*') 
                {
                    advance();
                    advance();
                    while(true) 
                    {
                        while(peek() != '*' && !is_at_end()) 
                        {
                            if((c = advance()) == '\n') 
                            {
                                scanner.line++;
                            }
                        }

                        if(is_at_end())
                            return;

                        if(peek_next() == '/') 
                        {
                            break;
                        }
                        advance();
                    }
                    advance();
                    advance();
                } 
                else if(peek_next() == '/') 
                {
                    while(peek() != '\n' && !is_at_end()) advance();
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

static TeaTokenType check_keyword(int start, int length, const char* rest, TeaTokenType type)
{
    if(scanner.current - scanner.start == start + length && memcmp(scanner.start + start, rest, length) == 0)
    {
        return type;
    }

    return TOKEN_NAME;
}

static TeaTokenType identifier_type()
{
    switch(scanner.start[0])
    {
        case 'a':
        {
            if(scanner.current - scanner.start > 1)
            {
                switch(scanner.start[1])
                {
                    case 'n': return check_keyword(2, 1, "d", TOKEN_AND);
                    case 's': return check_keyword(2, 0, "", TOKEN_AS);
                }
            }
            break;
        }
        case 'b': return check_keyword(1, 4, "reak", TOKEN_BREAK);
        case 'c':
        {
            if(scanner.current - scanner.start > 1)
            {
                switch(scanner.start[1])
                {
                    case 'a': return check_keyword(2, 2, "se", TOKEN_CASE);
                    case 'l': return check_keyword(2, 3, "ass", TOKEN_CLASS);
                    case 'o': return check_keyword(2, 6, "ntinue", TOKEN_CONTINUE);
                }
            }
            break;
        }
        case 'd': return check_keyword(1, 6, "efault", TOKEN_DEFAULT);
        case 'e': return check_keyword(1, 3, "lse", TOKEN_ELSE);
        case 'f':
        {
            if(scanner.current - scanner.start > 1)
            {
                switch(scanner.start[1])
                {
                    case 'a': return check_keyword(2, 3, "lse", TOKEN_FALSE);
                    case 'o': return check_keyword(2, 1, "r", TOKEN_FOR);
                    case 'u': return check_keyword(2, 6, "nction", TOKEN_FUNCTION);
                }
            }
            break;
        }
        case 'i':
        {
            if(scanner.current - scanner.start > 1)
            {
                switch(scanner.start[1])
                {
                    case 'f': return check_keyword(2, 0, "", TOKEN_IF);
                    case 'm': return check_keyword(2, 4, "port", TOKEN_IMPORT);
                }
            }
            break;
        }
        case 'n': return check_keyword(1, 3, "ull", TOKEN_NULL);
        case 'o': return check_keyword(1, 1, "r", TOKEN_OR);
        case 'r': return check_keyword(1, 5, "eturn", TOKEN_RETURN);
        case 's':
        {
            if(scanner.current - scanner.start > 1)
            {
                switch(scanner.start[1])
                {
                    case 'u': return check_keyword(2, 3, "per", TOKEN_SUPER);
                    case 'w': return check_keyword(2, 4, "itch", TOKEN_SWITCH);
                }
            }
            break;
        }
        case 't':
        {
            if(scanner.current - scanner.start > 1)
            {
                switch(scanner.start[1])
                {
                    case 'h': return check_keyword(2, 2, "is", TOKEN_THIS);
                    case 'r': return check_keyword(2, 2, "ue", TOKEN_TRUE);
                }
            }
            break;
        }
        case 'v': return check_keyword(1, 2, "ar", TOKEN_VAR);
        case 'w': return check_keyword(1, 4, "hile", TOKEN_WHILE);
    }

    return TOKEN_NAME;
}

static TeaToken identifier()
{
    while(is_alpha(peek()) || is_digit(peek()))
        advance();

    return make_token(identifier_type());
}

static TeaToken number()
{
    // Hexadecimal representation

    // Binary representation

    while(is_digit(peek()))
        advance();

    // Look for a fractional part.
    if(peek() == '.' && is_digit(peek_next()))
    {
        // Consume the ".".
        advance();

        while(is_digit(peek()))
            advance();
    }

    return make_token(TOKEN_NUMBER);
}

static TeaToken string()
{
    while(peek() != '"' && !is_at_end())
    {
        if(peek() == '\n')
            scanner.line++;
        advance();
    }

    if(is_at_end())
        return error_token("Unterminated string.");

    advance();

    return make_token(TOKEN_STRING);
}

TeaToken tea_scan_token()
{
    skip_whitespace();
    scanner.start = scanner.current;

    if(is_at_end())
        return make_token(TOKEN_EOF);

    char c = advance();

    if(is_alpha(c))
        return identifier();

    if(is_digit(c))
        return number();

    switch(c)
    {
        case '(': return make_token(TOKEN_LEFT_PAREN);
        case ')': return make_token(TOKEN_RIGHT_PAREN);
        case '[': return make_token(TOKEN_LEFT_BRACKET);
        case ']': return make_token(TOKEN_RIGHT_BRACKET);
        case '{': return make_token(TOKEN_LEFT_BRACE);
        case '}': return make_token(TOKEN_RIGHT_BRACE);
        case ',': return make_token(TOKEN_COMMA);
        case ':': return make_token(TOKEN_COLON);
        case '?': return make_token(TOKEN_QUESTION);
        case '.': return make_token(TOKEN_DOT);
        case '-': return make_token(TOKEN_MINUS);
        case '+': return make_token(TOKEN_PLUS);
        case '*': return make_token(TOKEN_STAR);
        case '/': return make_token(TOKEN_SLASH);
        case '!': return make_token(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=': return make_token(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<': return make_token(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '>': return make_token(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '"': return string();
    }

    return error_token("Unexpected character.");
}