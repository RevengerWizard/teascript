#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tea_common.h"
#include "parser/tea_parser.h"
#include "memory/tea_memory.h"
#include "scanner/tea_scanner.h"
#include "modules/tea_module.h"

#ifdef DEBUG_PRINT_CODE
#include "debug/tea_debug.h"
#endif

typedef struct
{
    TeaToken current;
    TeaToken previous;
    bool had_error;
    bool panic_mode;
} TeaParser;

typedef enum
{
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_OR,         // or
    PREC_AND,        // and
    PREC_EQUALITY,   // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_TERM,       // + -
    PREC_FACTOR,     // * /
    PREC_UNARY,      // ! -
    PREC_CALL,       // . ()
    PREC_SUBSCRIPT,  // []
    PREC_PRIMARY
} TeaPrecedence;

typedef void (*TeaParsePrefixFn)(bool can_assign);
typedef void (*TeaParseInfixFn)(TeaToken previous_token, bool can_assign);

typedef struct
{
    TeaParsePrefixFn prefix;
    TeaParseInfixFn infix;
    TeaPrecedence precedence;
} TeaParseRule;

typedef struct
{
    TeaToken name;
    int depth;
    bool is_captured;
} TeaLocal;

typedef struct
{
    uint8_t index;
    bool is_local;
} TeaUpvalue;

typedef enum
{
    TYPE_FUNCTION,
    TYPE_INITIALIZER,
    TYPE_METHOD,
    TYPE_SCRIPT
} TeaFunctionType;

typedef struct TeaCompiler
{
    struct TeaCompiler* enclosing;
    TeaObjectFunction* function;
    TeaFunctionType type;

    TeaLocal locals[UINT8_COUNT];
    int local_count;
    TeaUpvalue upvalues[UINT8_COUNT];
    int scope_depth;
} TeaCompiler;

typedef struct TeaClassCompiler
{
    struct TeaClassCompiler* enclosing;
    bool has_superclass;
} TeaClassCompiler;

TeaParser parser;
TeaCompiler* current = NULL;
TeaClassCompiler* current_class = NULL;

static TeaChunk* current_chunk()
{
    return &current->function->chunk;
}

static void error_at(TeaToken* token, const char* message)
{
    if(parser.panic_mode)
        return;
    parser.panic_mode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if(token->type == TOKEN_EOF)
    {
        fprintf(stderr, " at end");
    }
    else if(token->type == TOKEN_ERROR)
    {
        // Nothing.
    }
    else
    {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.had_error = true;
}

static void error(const char* message)
{
    error_at(&parser.previous, message);
}

static void error_at_current(const char* message)
{
    error_at(&parser.current, message);
}

static void advance()
{
    parser.previous = parser.current;

    while(true)
    {
        parser.current = tea_scan_token();
        if(parser.current.type != TOKEN_ERROR)
            break;

        error_at_current(parser.current.start);
    }
}

static void consume(TeaTokenType type, const char* message)
{
    if(parser.current.type == type)
    {
        advance();
        return;
    }

    error_at_current(message);
}

static bool check(TeaTokenType type)
{
    return parser.current.type == type;
}

static bool match(TeaTokenType type)
{
    if(!check(type))
        return false;
    advance();
    return true;
}

static void emit_byte(uint8_t byte)
{
    tea_write_chunk(current_chunk(), byte, parser.previous.line);
}

static void emit_bytes(uint8_t byte1, uint8_t byte2)
{
    emit_byte(byte1);
    emit_byte(byte2);
}

static void emit_loop(int loop_start)
{
    emit_byte(OP_LOOP);

    int offset = current_chunk()->count - loop_start + 2;
    if(offset > UINT16_MAX)
        error("Loop body too large.");

    emit_byte((offset >> 8) & 0xff);
    emit_byte(offset & 0xff);
}

static int emit_jump(uint8_t instruction)
{
    emit_byte(instruction);
    emit_byte(0xff);
    emit_byte(0xff);

    return current_chunk()->count - 2;
}

static void emit_return()
{
    if(current->type == TYPE_INITIALIZER)
    {
        emit_bytes(OP_GET_LOCAL, 0);
    }
    else
    {
        emit_byte(OP_NULL);
    }

    emit_byte(OP_RETURN);
}

static uint8_t make_constant(TeaValue value)
{
    int constant = tea_add_constant(current_chunk(), value);
    if(constant > UINT8_MAX)
    {
        error("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

static void emit_constant(TeaValue value)
{
    emit_bytes(OP_CONSTANT, make_constant(value));
}

static void patch_jump(int offset)
{
    // -2 to adjust for the bytecode for the jump offset itself.
    int jump = current_chunk()->count - offset - 2;

    if(jump > UINT16_MAX)
    {
        error("Too much code to jump over.");
    }

    current_chunk()->code[offset] = (jump >> 8) & 0xff;
    current_chunk()->code[offset + 1] = jump & 0xff;
}

static void init_compiler(TeaCompiler* compiler, TeaFunctionType type)
{
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;
    compiler->local_count = 0;
    compiler->scope_depth = 0;
    compiler->function = tea_new_function();
    current = compiler;
    if(type != TYPE_SCRIPT)
    {
        current->function->name = tea_copy_string(parser.previous.start, parser.previous.length);
    }

    TeaLocal* local = &current->locals[current->local_count++];
    local->depth = 0;
    local->is_captured = false;
    if(type != TYPE_FUNCTION)
    {
        local->name.start = "this";
        local->name.length = 4;
    }
    else
    {
        local->name.start = "";
        local->name.length = 0;
    }
}

static TeaObjectFunction* end_compiler()
{
    emit_return();
    TeaObjectFunction* function = current->function;

#ifdef DEBUG_PRINT_CODE
    if(!parser.had_error)
    {
        tea_disassemble_chunk(current_chunk(), function->name != NULL ? function->name->chars : "<script>");
    }
#endif

    current = current->enclosing;
    
    return function;
}

static void begin_scope()
{
    current->scope_depth++;
}

static void end_scope()
{
    current->scope_depth--;

    while(current->local_count > 0 && current->locals[current->local_count - 1].depth > current->scope_depth)
    {
        if(current->locals[current->local_count - 1].is_captured)
        {
            emit_byte(OP_CLOSE_UPVALUE);
        }
        else
        {
            emit_byte(OP_POP);
        }
        current->local_count--;
    }
}

static void expression();
static void statement();
static void declaration();
static TeaParseRule* get_rule(TeaTokenType type);
static void parse_precendence(TeaPrecedence precedence);
static void anonymous(bool can_assign);

static uint8_t identifier_constant(TeaToken* name)
{
    return make_constant(OBJECT_VAL(tea_copy_string(name->start, name->length)));
}

static bool identifiers_equal(TeaToken* a, TeaToken* b)
{
    if(a->length != b->length)
        return false;

    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolve_local(TeaCompiler* compiler, TeaToken* name)
{
    for(int i = compiler->local_count - 1; i >= 0; i--)
    {
        TeaLocal* local = &compiler->locals[i];
        if(identifiers_equal(name, &local->name))
        {
            if(local->depth == -1)
            {
                error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }

    return -1;
}

static int add_upvalue(TeaCompiler* compiler, uint8_t index, bool is_local)
{
    int upvalue_count = compiler->function->upvalue_count;

    for(int i = 0; i < upvalue_count; i++)
    {
        TeaUpvalue *upvalue = &compiler->upvalues[i];
        if(upvalue->index == index && upvalue->is_local == is_local)
        {
            return i;
        }
    }

    if(upvalue_count == UINT8_COUNT)
    {
        error("Too many closure variables in function.");
        return 0;
    }

    compiler->upvalues[upvalue_count].is_local = is_local;
    compiler->upvalues[upvalue_count].index = index;
    return compiler->function->upvalue_count++;
}

static int resolve_upvalue(TeaCompiler* compiler, TeaToken* name)
{
    if(compiler->enclosing == NULL)
        return -1;

    int local = resolve_local(compiler->enclosing, name);
    if(local != -1)
    {
        compiler->enclosing->locals[local].is_captured = true;
        return add_upvalue(compiler, (uint8_t)local, true);
    }

    int upvalue = resolve_upvalue(compiler->enclosing, name);
    if(upvalue != -1)
    {
        return add_upvalue(compiler, (uint8_t)upvalue, false);
    }

    return -1;
}

static void add_local(TeaToken name)
{
    if(current->local_count == UINT8_COUNT)
    {
        error("Too many local variables in function.");
        return;
    }

    TeaLocal* local = &current->locals[current->local_count++];
    local->name = name;
    local->depth = -1;
    local->is_captured = false;
}

static void declare_variable()
{
    if(current->scope_depth == 0)
        return;

    TeaToken* name = &parser.previous;

    add_local(*name);
}

static uint8_t parse_variable(const char* error_message)
{
    consume(TOKEN_NAME, error_message);

    declare_variable();
    if(current->scope_depth > 0)
        return 0;

    return identifier_constant(&parser.previous);
}

static void mark_initialized()
{
    if(current->scope_depth == 0)
        return;

    current->locals[current->local_count - 1].depth = current->scope_depth;
}

static void define_variable(uint8_t global)
{
    if(current->scope_depth > 0)
    {
        mark_initialized();
        return;
    }

    emit_bytes(OP_DEFINE_GLOBAL, global);
}

static uint8_t argument_list()
{
    uint8_t arg_count = 0;
    if(!check(TOKEN_RIGHT_PAREN))
    {
        do
        {
            expression();
            if(arg_count == 255)
            {
                error("Can't have more than 255 arguments.");
            }
            arg_count++;
        } 
        while(match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");

    return arg_count;
}

static void and_(TeaToken previous_token, bool can_assign)
{
    int end_jump = emit_jump(OP_JUMP_IF_FALSE);

    emit_byte(OP_POP);
    parse_precendence(PREC_AND);

    patch_jump(end_jump);
}

static void binary(TeaToken previous_token, bool can_assign)
{
    TeaTokenType operator_type = parser.previous.type;
    TeaParseRule* rule = get_rule(operator_type);
    parse_precendence((TeaPrecedence)(rule->precedence + 1));

    switch(operator_type)
    {
        case TOKEN_BANG_EQUAL:
        {
            emit_bytes(OP_EQUAL, OP_NOT);
            break;
        }
        case TOKEN_EQUAL_EQUAL:
        {
            emit_byte(OP_EQUAL);
            break;
        }
        case TOKEN_GREATER:
        {
            emit_byte(OP_GREATER);
            break;
        }
        case TOKEN_GREATER_EQUAL:
        {
            emit_bytes(OP_LESS, OP_NOT);
            break;
        }
        case TOKEN_LESS:
        {
            emit_byte(OP_LESS);
            break;
        }
        case TOKEN_LESS_EQUAL:
        {
            emit_bytes(OP_GREATER, OP_NOT);
            break;
        }
        case TOKEN_PLUS:
        {
            emit_byte(OP_ADD);
            break;
        }
        case TOKEN_MINUS:
        {
            emit_byte(OP_SUBTRACT);
            break;
        }
        case TOKEN_STAR:
        {
            emit_byte(OP_MULTIPLY);
            break;
        }
        case TOKEN_SLASH:
        {
            emit_byte(OP_DIVIDE);
            break;
        }
        default: return; // Unreachable.
    }
}

static void call(TeaToken previous_token, bool can_assign)
{
    uint8_t arg_count = argument_list();
    emit_bytes(OP_CALL, arg_count);
}

static void dot(TeaToken previous_token, bool can_assign)
{
    consume(TOKEN_NAME, "Expect property name after '.'");
    uint8_t name = identifier_constant(&parser.previous);

    if(can_assign && match(TOKEN_EQUAL))
    {
        expression();
        emit_bytes(OP_SET_PROPERTY, name);
    }
    else if(match(TOKEN_LEFT_PAREN))
    {
        uint8_t arg_count = argument_list();
        emit_bytes(OP_INVOKE, name);
        emit_byte(arg_count);
    }
    else
    {
        emit_bytes(OP_GET_PROPERTY, name);
    }
}

static void literal(bool can_assign)
{
    switch(parser.previous.type)
    {
        case TOKEN_FALSE:
        {
            emit_byte(OP_FALSE);
            break;
        }
        case TOKEN_NULL:
        {
            emit_byte(OP_NULL);
            break;
        }
        case TOKEN_TRUE:
        {
            emit_byte(OP_TRUE);
            break;
        }
        default:
            return; // Unreachable.
    }
}

static void grouping(bool can_assign)
{
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void list(bool can_assign)
{
    int item_count = 0;
    if(!check(TOKEN_RIGHT_BRACKET))
    {
        do
        {
            if(check(TOKEN_RIGHT_BRACKET))
            {
                // Traling comma case
                break;
            }

            expression();

            if(item_count == UINT8_COUNT)
            {
                error("Cannot have more than 256 items in a list literal.");
            }
            item_count++;
        }
        while(match(TOKEN_COMMA));
    }

    consume(TOKEN_RIGHT_BRACKET, "Expect ']' after list literal.");

    emit_bytes(OP_LIST, item_count);

    return;
}

static void map(bool can_assign)
{
    int item_count = 0;
    if(!check(TOKEN_RIGHT_BRACE))
    {
        do
        {
            if(check(TOKEN_RIGHT_BRACE))
            {
                // Traling comma case
                break;
            }

            consume(TOKEN_NAME, "Expected key name");
            emit_constant(OBJECT_VAL(tea_copy_string(parser.previous.start, parser.previous.length)));
            consume(TOKEN_EQUAL, "Expected '=' after key name.");
            expression();
            if(item_count == UINT8_COUNT)
            {
                error("Cannot have more than 256 items in a map literal.");
            }
            item_count++;
        }
        while(match(TOKEN_COMMA));
    }

    consume(TOKEN_RIGHT_BRACE, "Expect '} after map literal.'");

    emit_bytes(OP_MAP, item_count);

    return;
}

static void subscript(TeaToken previous_token, bool can_assign)
{
    if(match(TOKEN_COLON))
    {
        emit_byte(OP_NULL);
        expression();
        emit_byte(OP_SLICE);
        consume(TOKEN_RIGHT_BRACKET, "Expect ']' after closing.");
        return;
    }

    expression();

    if(match(TOKEN_COLON))
    {
        if(check(TOKEN_RIGHT_BRACKET))
        {
            emit_byte(OP_NULL);
        }
        else
        {
            expression();
        }
        emit_byte(OP_SLICE);
        consume(TOKEN_RIGHT_BRACKET, "Expect ']' after closing.");
        return;
    }

    consume(TOKEN_RIGHT_BRACKET, "Expect ']' after closing.");

    if(can_assign && match(TOKEN_EQUAL))
    {
        expression();
        emit_byte(OP_SUBSCRIPT_STORE);
    }
    else
    {
        emit_byte(OP_SUBSCRIPT);
    }

    return;
}

static void number(bool can_assign)
{
    double value = strtod(parser.previous.start, NULL);
    emit_constant(NUMBER_VAL(value));
}

static void or_(TeaToken previous_token, bool can_assign)
{
    int else_jump = emit_jump(OP_JUMP_IF_FALSE);
    int end_jump = emit_jump(OP_JUMP);

    patch_jump(else_jump);
    emit_byte(OP_POP);

    parse_precendence(PREC_OR);
    patch_jump(end_jump);
}

static void string(bool can_assign)
{
    emit_constant(OBJECT_VAL(tea_copy_string(parser.previous.start + 1, parser.previous.length - 2)));
}

static void named_variable(TeaToken name, bool can_assign)
{
    uint8_t get_op, set_op;
    int arg = resolve_local(current, &name);
    if(arg != -1)
    {
        get_op = OP_GET_LOCAL;
        set_op = OP_SET_LOCAL;
    }
    else if((arg = resolve_upvalue(current, &name)) != -1)
    {
        get_op = OP_GET_UPVALUE;
        set_op = OP_SET_UPVALUE;
    }
    else
    {
        arg = identifier_constant(&name);
        get_op = OP_GET_GLOBAL;
        set_op = OP_SET_GLOBAL;
    }

    if(can_assign && match(TOKEN_EQUAL))
    {
        expression();
        emit_bytes(set_op, (uint8_t)arg);
    }
    else
    {
        emit_bytes(get_op, (uint8_t)arg);
    }
}

static void variable(bool can_assign)
{
    named_variable(parser.previous, can_assign);
}

static TeaToken synthetic_token(const char* text)
{
    TeaToken token;
    token.start = text;
    token.length = (int)strlen(text);

    return token;
}

static void super_(bool can_assign)
{
    if(current_class == NULL)
    {
        error("Can't use 'super' outside of a class.");
    }
    else if(!current_class->has_superclass)
    {
        error("Can't use 'super' in a class with no superclass.");
    }

    consume(TOKEN_DOT, "Expect '.' after 'super'.");
    consume(TOKEN_NAME, "Expect superclass method name.");
    uint8_t name = identifier_constant(&parser.previous);

    named_variable(synthetic_token("this"), false);

    if(match(TOKEN_LEFT_PAREN))
    {
        uint8_t arg_count = argument_list();
        named_variable(synthetic_token("super"), false);
        emit_bytes(OP_SUPER, name);
        emit_byte(arg_count);
    }
    else
    {
        named_variable(synthetic_token("super"), false);
        emit_bytes(OP_GET_SUPER, name);
    }
}

static void this_(bool can_assign)
{
    if(current_class == NULL)
    {
        error("Can't use 'this' outside of a class.");
        return;
    }

    variable(false);
}

static void unary(bool can_assign)
{
    TeaTokenType operator_type = parser.previous.type;

    parse_precendence(PREC_UNARY);

    // Emit the operator instruction.
    switch(operator_type)
    {
        case TOKEN_BANG:
        {
            emit_byte(OP_NOT);
            break;
        }
        case TOKEN_MINUS:
        {
            emit_byte(OP_NEGATE);
            break;
        }
        default:
            return; // Unreachable.
    }
}

#define UNUSED                  { NULL, NULL, PREC_NONE }
#define RULE(pr, in, prec)      { pr, in, PREC_##prec }    
#define PREFIX(pr)              { pr, NULL, PREC_NONE }
#define OPERATOR(in, prec)      { NULL, in, PREC_##prec }

TeaParseRule rules[] = {
    RULE(grouping, call, CALL),           // TOKEN_LEFT_PAREN
    UNUSED,                               // TOKEN_RIGHT_PAREN
    RULE(list, subscript, SUBSCRIPT),     // TOKEN_LEFT_BRACKET
    UNUSED,                               // TOKEN_RIGHT_BRACKET
    PREFIX(map),                          // TOKEN_LEFT_BRACE
    UNUSED,                               // TOKEN_RIGHT_BRACE
    UNUSED,                               // TOKEN_COMMA
    OPERATOR(dot, CALL),                  // TOKEN_DOT
    UNUSED,                               // TOKEN_COLON
    UNUSED,                               // TOKEN_QUESTION
    RULE(unary, binary, TERM),            // TOKEN_MINUS
    OPERATOR(binary, TERM),               // TOKEN_PLUS
    OPERATOR(binary, FACTOR),             // TOKEN_SLASH
    OPERATOR(binary, FACTOR),             // TOKEN_STAR
    UNUSED,                               // TOKEN_PLUS_PLUS
    UNUSED,                               // TOKEN_MINUS_MINUS
    UNUSED,                               // TOKEN_PLUS_EQUAL
    UNUSED,                               // TOKEN_MINUS_EQUAL
    UNUSED,                               // TOKEN_STAR_EQUAL
    UNUSED,                               // TOKEN_SLASH_EQUAL
    PREFIX(unary),                        // TOKEN_BANG
    OPERATOR(binary, EQUALITY),           // TOKEN_BANG_EQUAL
    UNUSED,                               // TOKEN_EQUAL
    OPERATOR(binary, EQUALITY),           // TOKEN_EQUAL_EQUAL
    OPERATOR(binary, COMPARISON),         // TOKEN_GREATER
    OPERATOR(binary, COMPARISON),         // TOKEN_GREATER_EQUAL
    OPERATOR(binary, COMPARISON),         // TOKEN_LESS
    OPERATOR(binary, COMPARISON),         // TOKEN_LESS_EQUAL
    UNUSED,                               // TOKEN_PERCENT
    UNUSED,                               // TOKEN_PERCENT_EQUAL
    UNUSED,                               // TOKEN_DOT_DOT
    UNUSED,                               // TOKEN_DOT_DOT_DOT
    PREFIX(variable),                     // TOKEN_NAME
    PREFIX(string),                       // TOKEN_STRING
    PREFIX(number),                       // TOKEN_NUMBER
    OPERATOR(and_, AND),                  // TOKEN_AND
    UNUSED,                               // TOKEN_CLASS
    UNUSED,                               // TOKEN_ELSE
    PREFIX(literal),                      // TOKEN_FALSE
    UNUSED,                               // TOKEN_FOR
    PREFIX(anonymous),                    // TOKEN_FUNCTION
    UNUSED,                               // TOKEN_CASE
    UNUSED,                               // TOKEN_SWITCH
    UNUSED,                               // TOKEN_DEFAULT
    UNUSED,                               // TOKEN_IF
    PREFIX(literal),                      // TOKEN_NULL
    OPERATOR(or_, OR),                    // TOKEN_OR
    UNUSED,                               // TOKEN_IMPORT
    UNUSED,                               // TOKEN_FROM
    UNUSED,                               // TOKEN_AS
    UNUSED,                               // TOKEN_RETURN
    PREFIX(super_),                       // TOKEN_SUPER
    PREFIX(this_),                        // TOKEN_THIS
    UNUSED,                               // TOKEN_CONTINUE
    UNUSED,                               // TOKEN_BREAK
    PREFIX(literal),                      // TOKEN_TRUE
    UNUSED,                               // TOKEN_VAR
    UNUSED,                               // TOKEN_WHILE
    UNUSED,                               // TOKEN_ERROR
    UNUSED,                               // TOKEN_EOF
};

#undef UNUSED
#undef RULE
#undef PREFIX
#undef OPERATOR

static void parse_precendence(TeaPrecedence precedence)
{
    advance();
    TeaParsePrefixFn prefix_rule = get_rule(parser.previous.type)->prefix;
    if(prefix_rule == NULL)
    {
        error("Expect expression.");
        return;
    }

    bool can_assign = precedence <= PREC_ASSIGNMENT;
    prefix_rule(can_assign);

    while(precedence <= get_rule(parser.current.type)->precedence)
    {
        TeaToken token = parser.previous;
        advance();
        TeaParseInfixFn infix_rule = get_rule(parser.previous.type)->infix;
        infix_rule(token, can_assign);
    }

    if(can_assign && match(TOKEN_EQUAL))
    {
        error("Invalid assignment target.");
    }
}

static TeaParseRule* get_rule(TeaTokenType type)
{
    return &rules[type];
}

static void expression()
{
    parse_precendence(PREC_ASSIGNMENT);
}

static void block()
{
    while(!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF))
    {
        declaration();
    }

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void function(TeaFunctionType type)
{
    TeaCompiler compiler;
    init_compiler(&compiler, type);
    begin_scope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");

    if(!check(TOKEN_RIGHT_PAREN))
    {
        do
        {
            current->function->arity++;
            if(current->function->arity > 255)
            {
                error_at_current("Can't have more than 255 parameters.");
            }
            uint8_t constant = parse_variable("Expect parameter name.");
            define_variable(constant);
        } 
        while(match(TOKEN_COMMA));
    }

    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block();

    TeaObjectFunction* function = end_compiler();
    emit_bytes(OP_CLOSURE, make_constant(OBJECT_VAL(function)));

    for(int i = 0; i < function->upvalue_count; i++)
    {
        emit_byte(current->upvalues[i].is_local ? 1 : 0);
        emit_byte(current->upvalues[i].index);
    }
}

static void anonymous(bool can_assign)
{
    function(TYPE_FUNCTION);
}

static void method()
{
    consume(TOKEN_NAME, "Expect method name.");
    uint8_t constant = identifier_constant(&parser.previous);

    TeaFunctionType type = TYPE_METHOD;

    if(parser.previous.length == 4 && memcmp(parser.previous.start, "init", 4) == 0)
    {
        type = TYPE_INITIALIZER;
    }

    function(type);
    emit_bytes(OP_METHOD, constant);
}

static void class_declaration()
{
    consume(TOKEN_NAME, "Expect class name.");
    TeaToken class_name = parser.previous;
    uint8_t name_constant = identifier_constant(&parser.previous);
    declare_variable();

    emit_bytes(OP_CLASS, name_constant);
    define_variable(name_constant);

    TeaClassCompiler class_compiler;
    class_compiler.has_superclass = false;
    class_compiler.enclosing = current_class;
    current_class = &class_compiler;

    if(match(TOKEN_COLON))
    {
        consume(TOKEN_NAME, "Expect superclass name.");
        variable(false);

        if(identifiers_equal(&class_name, &parser.previous))
        {
            error("A class can't inherit from itself.");
        }

        begin_scope();
        add_local(synthetic_token("super"));
        define_variable(0);

        named_variable(class_name, false);
        emit_byte(OP_INHERIT);
        class_compiler.has_superclass = true;
    }

    named_variable(class_name, false);
    consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");

    while(!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF))
    {
        method();
    }

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
    emit_byte(OP_POP);

    if(class_compiler.has_superclass)
    {
        end_scope();
    }

    current_class = current_class->enclosing;
}

static void function_declaration()
{
    uint8_t global = parse_variable("Expect function name.");
    mark_initialized();
    function(TYPE_FUNCTION);
    define_variable(global);
}

static void var_declaration()
{
    do
    {
        uint8_t global = parse_variable("Expect variable name.");

        if(match(TOKEN_EQUAL))
        {
            expression();
        }
        else
        {
            emit_byte(OP_NULL);
        }

        define_variable(global);
    }
    while(match(TOKEN_COMMA));
}

static void expression_statement()
{
    expression();
    emit_byte(OP_POP);
}

static void for_statement()
{
    begin_scope();
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");

    if(match(TOKEN_VAR))
    {
        var_declaration();
        consume(TOKEN_COMMA, "Expect ',' after loop variable.");
    }
    else if(match(TOKEN_COMMA))
    {
        // No initializer.
    }
    else
    {
        expression_statement();
        consume(TOKEN_COMMA, "Expect ',' after loop expression.");
    }

    int loop_start = current_chunk()->count;
    int exit_jump = -1;
    if(!match(TOKEN_COMMA))
    {
        expression();
        consume(TOKEN_COMMA, "Expect ',' after loop condition.");

        exit_jump = emit_jump(OP_JUMP_IF_FALSE);
        emit_byte(OP_POP); // Condition.
    }

    if(!match(TOKEN_RIGHT_PAREN))
    {
        int body_jump = emit_jump(OP_JUMP);
        int increment_start = current_chunk()->count;
        expression();
        emit_byte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        emit_loop(loop_start);
        loop_start = increment_start;
        patch_jump(body_jump);
    }

    statement();
    emit_loop(loop_start);

    if(exit_jump != -1)
    {
        patch_jump(exit_jump);
        emit_byte(OP_POP); // Condition.
    }

    end_scope();
}

static void if_statement()
{
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int else_jump = emit_jump(OP_JUMP_IF_FALSE);

    emit_byte(OP_POP);
    statement();

    int end_jump = emit_jump(OP_JUMP);

    patch_jump(else_jump);
    emit_byte(OP_POP);

    if(match(TOKEN_ELSE))
        statement();

    patch_jump(end_jump);
}

static void return_statement()
{
    if(current->type == TYPE_SCRIPT)
    {
        error("Can't return from top-level code.");
    }

    if(check(TOKEN_RIGHT_BRACE))
    {
        emit_return();
    }
    else
    {
        if(current->type == TYPE_INITIALIZER)
        {
            error("Can't return a value from an initializer.");
        }

        expression();
        emit_byte(OP_RETURN);
    }
}

static void import_statement()
{
    if(match(TOKEN_STRING))
    {
        
    }
    else
    {
        consume(TOKEN_NAME, "Expect import identifier.");
        uint8_t import_name = identifier_constant(&parser.previous);
        declare_variable(parser.previous);

        int index = tea_find_native_module(
            (char*)parser.previous.start,
            parser.current.length - parser.previous.length
        );

        if(index == -1) 
        {
            error("Unknown module");
        }

        emit_bytes(OP_IMPORT_NATIVE, index);
        emit_byte(import_name);

        define_variable(import_name);
    }
}

static void from_import_statement()
{

}

static void while_statement()
{
    int loop_start = current_chunk()->count;

    if(check(TOKEN_LEFT_BRACE))
    {
        emit_byte(OP_TRUE);
    }
    else
    {
        consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
        expression();
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
    }

    int exit_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    statement();
    emit_loop(loop_start);

    patch_jump(exit_jump);
    emit_byte(OP_POP);
}

static void synchronize()
{
    parser.panic_mode = false;

    while(parser.current.type != TOKEN_EOF)
    {
        switch(parser.current.type)
        {
            case TOKEN_CLASS:
            case TOKEN_FUNCTION:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_BREAK:
            case TOKEN_RETURN:
            case TOKEN_IMPORT:
                return;

            default:; // Do nothing.
        }

        advance();
    }
}

static void declaration()
{
    if(match(TOKEN_CLASS))
    {
        class_declaration();
    }
    else if(match(TOKEN_FUNCTION))
    {
        function_declaration();
    }
    else if(match(TOKEN_VAR))
    {
        var_declaration();
    }
    else
    {
        statement();
    }

    if(parser.panic_mode)
        synchronize();
}

static void statement()
{
    if(match(TOKEN_FOR))
    {
        for_statement();
    }
    else if(match(TOKEN_IF))
    {
        if_statement();
    }
    else if(match(TOKEN_RETURN))
    {
        return_statement();
    }
    else if(match(TOKEN_WHILE))
    {
        while_statement();
    }
    else if(match(TOKEN_IMPORT))
    {
        import_statement();
    }
    else if(match(TOKEN_FROM))
    {
        //from_import_statement();
    }
    else if(match(TOKEN_LEFT_BRACE))
    {
        begin_scope();
        block();
        end_scope();
    }
    else
    {
        expression_statement();
    }
}

TeaObjectFunction* tea_compile(const char* source)
{
    tea_init_scanner(source);
    TeaCompiler compiler;
    init_compiler(&compiler, TYPE_SCRIPT);

    parser.had_error = false;
    parser.panic_mode = false;

    advance();

    while(!match(TOKEN_EOF))
    {
        declaration();
    }

    TeaObjectFunction* function = end_compiler();

    return parser.had_error ? NULL : function;
}

void tea_mark_compiler_roots()
{
    TeaCompiler* compiler = current;
    while(compiler != NULL)
    {
        tea_mark_object((TeaObject*)compiler->function);
        compiler = compiler->enclosing;
    }
}