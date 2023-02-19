// tea_scanner.c
// Teascript scanner

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "tea_common.h"
#include "tea_value.h"
#include "tea_scanner.h"
#include "tea_utf.h"

void tea_init_scanner(TeaState* T, TeaScanner* scanner, const char* source)
{
    // Skip the UTF-8 BOM if there is one
    if(strncmp(source, "\xEF\xBB\xBF", 3) == 0) source += 3;

    scanner->T = T;
    scanner->start = source;
    scanner->current = source;
    scanner->line = 1;
    scanner->num_braces = 0;
    scanner->raw = false;
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

static void skip_line_comment(TeaScanner* scanner)
{
    while(peek(scanner) != '\n' && !is_at_end(scanner)) 
        advance(scanner);
}

static bool skip_whitespace(TeaScanner* scanner)
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
            {
                advance(scanner);
                break;
            }
            case '\n':
            {
                scanner->line++;
                advance(scanner);
                break;
            }
            case '#':
            {
                // Ignore shebang on first line
                if(scanner->line == 1 && peek_next(scanner) == '!')
                {
                    skip_line_comment(scanner);
                }
                else
                {
                    return false;
                }
                break;
            }
            case '/':
            {
                if(peek_next(scanner) == '/') 
                {
                    skip_line_comment(scanner);
                } 
                else if(peek_next(scanner) == '*') 
                {
                    advance(scanner);
                    advance(scanner);

                    int nesting = 1;
                    while(nesting > 0)
                    {
                        if(peek(scanner) == '\0')
                        {
                            return true;
                        }

                        if(peek(scanner) == '/' && peek_next(scanner) == '*')
                        {
                            advance(scanner);
                            advance(scanner);
                            nesting++;
                            continue;
                        }

                        if(peek(scanner) == '*' && peek_next(scanner) == '/')
                        {
                            advance(scanner);
                            advance(scanner);
                            nesting--;
                            continue;
                        }

                        if(peek(scanner) == '\n')
                            scanner->line++;
                        advance(scanner);
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
        case 'e':
        {
            if(scanner->current - scanner->start > 1)
            {
                switch(scanner->start[1])
                {
                    case 'n': return check_keyword(scanner, 2, 2, "um", TOKEN_ENUM);
                    case 'l': return check_keyword(scanner, 2, 2, "se", TOKEN_ELSE);
                }
            }
            break;
        }
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
                    case 't': return check_keyword(scanner, 2, 4, "atic", TOKEN_STATIC);
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

static TeaToken make_number_token(TeaScanner* scanner, bool strip, bool is_hex, bool is_bin, bool is_octal)
{
    errno = 0;
	TeaValue value;

    if(strip)
    {
        int len = (int)(scanner->current - scanner->start);
        char* buffer = ALLOCATE(scanner->T, char, len + 1);
        char* current = buffer;

        // Strip it of any underscores
        for(int i = 0; i < len; i++)
        {
            char c = scanner->start[i];

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
        
        FREE_ARRAY(scanner->T, char, buffer, len + 1);
        goto done;
    }

	if(is_hex) 
    {
		value = NUMBER_VAL((double)strtoll(scanner->start, NULL, 16));
	} 
    else if(is_bin) 
    {
		value = NUMBER_VAL((int)strtoll(scanner->start + 2, NULL, 2));
	} 
    else if(is_octal)
    {
        value = NUMBER_VAL((int)strtoll(scanner->start + 2, NULL, 8));
    }
    else 
    {
		value = NUMBER_VAL(strtod(scanner->start, NULL));
	}

    done:
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
    bool underscore, last = false;

    while(is_hex_digit(peek(scanner)) || peek(scanner) == '_')
    {
        if(peek(scanner) == '_')
        {
            if(!underscore) underscore = true;
            if(last) return error_token(scanner, "Cannot have consecutive underscores");
            advance(scanner);
            last = true;
        }
        else
        {
            advance(scanner);
            last = false;
        }
    }

    if(last) return error_token(scanner, "Invalid hex number");

    return make_number_token(scanner, underscore, true, false, false);
}

static TeaToken binary_number(TeaScanner* scanner)
{
    bool underscore, last = false;

    while(is_binary_digit(peek(scanner)) || peek(scanner) == '_')
    {
        if(peek(scanner) == '_')
        {
            if(!underscore) underscore = true;
            if(last) return error_token(scanner, "Cannot have consecutive underscores");
            advance(scanner);
            last = true;
        }
        else
        {
            advance(scanner);
            last = false;
        }
    }

    if(last) return error_token(scanner, "Cannot have leading underscores");

    return make_number_token(scanner, underscore, false, true, false);
}

static TeaToken octal_number(TeaScanner* scanner)
{
    bool underscore, last = false;

    while(is_octal_digit(peek(scanner)) || peek(scanner) == '_')
    {
        if(peek(scanner) == '_')
        {
            if(!underscore) underscore = true;
            if(last) return error_token(scanner, "Cannot have consecutive underscores");
            advance(scanner);
            last = true;
        }
        else
        {
            advance(scanner);
            last = false;
        }
    }

    if(last) return error_token(scanner, "Cannot have leading underscores");

    return make_number_token(scanner, underscore, false, false, true);
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

    bool underscore, last = false;

    while(is_digit(peek(scanner)) || peek(scanner) == '_')
    {
        if(peek(scanner) == '_')
        {
            if(!underscore) underscore = true;
            if(last) return error_token(scanner, "Cannot have consecutive underscores");
            advance(scanner);
            last = true;
        }
        else
        {
            advance(scanner);
            last = false;
        }
    }

    if(last) return error_token(scanner, "Cannot have leading underscores");

    return make_number_token(scanner, underscore, false, false, false);
}

static TeaToken number(TeaScanner* scanner)
{
    bool underscore, last = false;

    while(is_digit(peek(scanner)) || peek(scanner) == '_')
    {
        if(peek(scanner) == '_')
        {
            if(!underscore) underscore = true;
            if(last) return error_token(scanner, "Cannot have consecutive underscores");
            advance(scanner);
            last = true;
        }
        else
        {
            advance(scanner);
            last = false;
        }
    }

    // Look for a fractional part
    if(peek(scanner) == '.' && is_digit(peek_next(scanner)))
    {
        // Consume the "."
        advance(scanner);
        last = false;

        while(is_digit(peek(scanner)) || peek(scanner) == '_')
        {
            if(peek(scanner) == '_')
            {
                if(!underscore) underscore = true;
                if(last) return error_token(scanner, "Cannot have consecutive underscores");
                advance(scanner);
                last = true;
            }
            else
            {
                advance(scanner);
                last = false;
            }
        }
    }

    if(match(scanner, 'e') || match(scanner, 'E'))
    {
        return exponent_number(scanner);
    }

    if(last) return error_token(scanner, "Cannot have leading underscores");

    return make_number_token(scanner, underscore, false, false, false);
}

static int read_hex_digit(TeaScanner* scanner)
{
    char c = advance(scanner);
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;

    return -1;
}

static int read_hex_escape(TeaScanner* scanner, int digits)
{
    int value = 0;
    for(int i = 0; i < digits; i++)
    {
        if(peek(scanner) == scanner->string || peek(scanner) == '\0')
        {
            return -1;
        }

        int digit = read_hex_digit(scanner);
        if(digit == -1)
        {
            return -1;
        }

        value = (value * 16) | digit;
    }

    return value;
}

static bool read_unicode_escape(TeaScanner* scanner, TeaBytes* bytes, int length)
{
    int value = read_hex_escape(scanner, length);
    if(value == -1) return true;

    int num_bytes = tea_encode_bytes(value);
    if(num_bytes != 0)
    {
        tea_fill_bytes(scanner->T, bytes, 0, num_bytes);
        tea_ustring_encode(value, bytes->values + bytes->count - num_bytes);
    }
    return false;
}

static TeaToken multistring(TeaScanner* scanner)
{
    TeaState* T = scanner->T;
    TeaBytes bytes;
    tea_init_bytes(&bytes);

    // Consume second and third quote
    advance(scanner);
    advance(scanner);

    int skip_start = 0;
    int first_newline = -1;

    int skip_end = -1;
    int last_newline = -1;

    while(true)
    {
        char c = advance(scanner);
        char c1 = peek(scanner);
        char c2 = peek_next(scanner);

        if(c == '\r') continue;

        if(c == '\n')
        {
            last_newline = bytes.count;
            skip_end = last_newline;
            first_newline = first_newline == -1 ? bytes.count : first_newline;
        }

        char s = scanner->string;
        if(c == s && c1 == s && c2 == s) break;
    
        bool whitespace = c == ' ' || c == '\t';
        skip_end = c == '\n' || whitespace ? skip_end : -1;

        // If we haven't seen a newline or other character yet, 
        // and still seeing whitespace, count the characters 
        // as skippable till we know otherwise
        bool skippable = skip_start != -1 && whitespace && first_newline == -1;
        skip_start = skippable ? bytes.count + 1 : skip_start;
        
        // We've counted leading whitespace till we hit something else, 
        // but it's not a newline, so we reset skipStart since we need these characters
        if(first_newline == -1 && !whitespace && c != '\n') skip_start = -1;

        if(c == '\0' || c1 == '\0' || c2 == '\0')
        {
            tea_free_bytes(T, &bytes);
            return error_token(scanner, "Unterminated string");
        }
    
        tea_write_bytes(T, &bytes, c);
    }

    //consume the second and third quote
    advance(scanner);
    advance(scanner);

    int offset = 0;
    int count = bytes.count;

    if(first_newline != -1 && skip_start == first_newline) offset = first_newline + 1;
    if(last_newline != -1 && skip_end == last_newline) count = last_newline;

    count -= (offset > count) ? count : offset;

    TeaToken token = make_token(scanner, TOKEN_STRING);
	token.value = OBJECT_VAL(tea_copy_string(T, (const char*)bytes.values, bytes.count));
	tea_free_bytes(T, &bytes);

	return token;
}

static TeaToken string(TeaScanner* scanner, bool interpolation)
{
    TeaState* T = scanner->T;
    TeaTokenType type = TOKEN_STRING;

    TeaBytes bytes;
    tea_init_bytes(&bytes);

    while(true)
    {
        char c = advance(scanner);

        if(c == scanner->string)
        {
            break;
        }
        else if(interpolation && c == '{')
        {
            if(scanner->num_braces >= 4)
            {
                tea_free_bytes(T, &bytes);
				return error_token(scanner, "String interpolation is too deep");
			}

			type = TOKEN_INTERPOLATION;
			scanner->braces[scanner->num_braces++] = 1;
			break;
        }

        if(c == '\r') continue;
        if(c == '\t') continue;

        switch(c)
        {
            case '\0':
            {
                tea_free_bytes(T, &bytes);
                return error_token(scanner, "Unterminated string");
            }
            case '\n':
            {
                scanner->line++;
                tea_write_bytes(T, &bytes, c);
                break;
            }
            case '\\':
            {
                if(scanner->raw)
                {
                    tea_write_bytes(T, &bytes, c);
                    break;
                }

                switch(advance(scanner))
                {
                    case '\"': tea_write_bytes(T, &bytes, '\"'); break;
                    case '\'': tea_write_bytes(T, &bytes, '\''); break;
                    case '\\': tea_write_bytes(T, &bytes, '\\'); break;
                    case '0': tea_write_bytes(T, &bytes, '\0'); break;
                    case 'a': tea_write_bytes(T, &bytes, '\a'); break;
                    case 'b': tea_write_bytes(T, &bytes, '\b'); break;
                    case 'f': tea_write_bytes(T, &bytes, '\f'); break;
                    case 'n': tea_write_bytes(T, &bytes, '\n'); break;
                    case 'r': tea_write_bytes(T, &bytes, '\r'); break;
                    case 't': tea_write_bytes(T, &bytes, '\t'); break;
                    case 'v': tea_write_bytes(T, &bytes, '\v'); break;
                    case 'x':
                    {
                        int h = read_hex_escape(scanner, 2);
                        if(h == -1)
                        {
                            tea_free_bytes(T, &bytes);
                            return error_token(scanner, "Incomplete byte escape sequence.");
                        }
                        tea_write_bytes(T, &bytes, (uint8_t)h);
                        break;
                    }
                    case 'u':
                    {
                        bool e = read_unicode_escape(scanner, &bytes, 4);
                        if(e)
                        {
                            tea_free_bytes(T, &bytes);
                            return error_token(scanner, "Incomplete unicode escape sequence.");
                        }
                        break;
                    }
                    case 'U':
                    {
                        bool e = read_unicode_escape(scanner, &bytes, 8);
                        if(e)
                        {
                            tea_free_bytes(T, &bytes);
                            return error_token(scanner, "Incomplete unicode escape sequence.");
                        }
                        break;
                    }
                    default: 
                    {
                        tea_free_bytes(T, &bytes);
                        return error_token(scanner, "Invalid escape character");
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

    scanner->raw = false;
    TeaToken token = make_token(scanner, type);
	token.value = OBJECT_VAL(tea_copy_string(T, (const char*)bytes.values, bytes.count));
	tea_free_bytes(T, &bytes);

	return token;
}

void tea_backtrack(TeaScanner* scanner)
{
    scanner->current--;
}

TeaToken tea_scan_token(TeaScanner* scanner)
{
    if(skip_whitespace(scanner))
    {
        return error_token(scanner, "Unterminated block comment");
    }
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
        if(peek(scanner) == 'c' || peek(scanner) == 'C')
        {
            advance(scanner);
            return octal_number(scanner);
        }
    }
    else if(c == 'r')
    {
        if(peek(scanner) == '"' || peek(scanner) == '\'')
        {
            scanner->raw = true;
            scanner->string = advance(scanner);
            return string(scanner, false);
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
        case '{':
        {
            if(scanner->num_braces > 0)
            {
                scanner->braces[scanner->num_braces - 1]++;
			}

            return make_token(scanner, TOKEN_LEFT_BRACE);
        }
        case '}':
        {
            if(scanner->num_braces > 0 && --scanner->braces[scanner->num_braces - 1] == 0)
            {
				scanner->num_braces--;

				return string(scanner, true);
			}

            return make_token(scanner, TOKEN_RIGHT_BRACE);
        }
        case ',': return make_token(scanner, TOKEN_COMMA);
        case ';': return make_token(scanner, TOKEN_SEMICOLON);
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
        case '=':
        {
            if(match(scanner, '='))
            {
                return make_token(scanner, TOKEN_EQUAL_EQUAL);
            }
            else if(match(scanner, '>'))
            {
                return make_token(scanner, TOKEN_ARROW);
            }
            else
            {
                return make_token(scanner, TOKEN_EQUAL);
            }
        }
        case '<': return match_tokens(scanner, '=', '<', TOKEN_LESS_EQUAL, TOKEN_LESS_LESS, TOKEN_LESS);
        case '>': return match_tokens(scanner, '=', '>', TOKEN_GREATER_EQUAL, TOKEN_GREATER_GREATER, TOKEN_GREATER);
        case '"':
        {
            char s = scanner->string = '"';
            if(peek(scanner) == s && peek_next(scanner) == s)
            {
                return multistring(scanner);
            }
            return string(scanner, true);
        }
        case '\'':
        {
            char s = scanner->string = '\'';
            if(peek(scanner) == s && peek_next(scanner) == s)
            {
                return multistring(scanner);
            }
            return string(scanner, true);
        }
    }

    return error_token(scanner, "Unexpected character");
}