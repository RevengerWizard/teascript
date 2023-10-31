/*
** tea_parser.c
** Teascript parser and compiler
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define tea_parser_c
#define TEA_CORE

#include "tea_def.h"
#include "tea_state.h"
#include "tea_parser.h"
#include "tea_func.h"
#include "tea_string.h"
#include "tea_gc.h"
#include "tea_lexer.h"
#include "tea_import.h"

#ifdef TEA_DEBUG_PRINT_CODE
#include "tea_debug.h"
#endif

static TeaChunk* current_chunk(TeaParser* parser)
{
    return &parser->function->chunk;
}

static void error(TeaParser* parser, const char* message)
{
    tea_lex_error(parser->lex, &parser->lex->previous, message);
}

static void error_at_current(TeaParser* parser, const char* message)
{
    tea_lex_error(parser->lex, &parser->lex->current, message);
}

static void next(TeaParser* parser)
{
    parser->lex->previous = parser->lex->current;

    parser->lex->current = tea_lex_token(parser->lex);
}

/* Go back by one token. You need to patch the current token to the previous one after calling this function */
static void recede(TeaParser* parser)
{
    for(int i = 0; i < parser->lex->current.length; i++)
    {
        tea_lex_backtrack(parser->lex);
    }
}

static void consume(TeaParser* parser, TeaTokenType type, const char* message)
{
    if(parser->lex->current.type == type)
    {
        next(parser);
        return;
    }

    error_at_current(parser, message);
}

static bool check(TeaParser* parser, TeaTokenType type)
{
    return parser->lex->current.type == type;
}

static bool match(TeaParser* parser, TeaTokenType type)
{
    if(!check(parser, type))
        return false;
    next(parser);
    return true;
}

static void emit_byte(TeaParser* parser, uint8_t byte)
{
    tea_chunk_write(parser->lex->T, current_chunk(parser), byte, parser->lex->previous.line);
}

static void emit_bytes(TeaParser* parser, uint8_t byte1, uint8_t byte2)
{
    emit_byte(parser, byte1);
    emit_byte(parser, byte2);
}

static const int stack_effects[] = {
    #define OPCODE(_, effect) effect
    #include "tea_opcodes.h"
    #undef OPCODE
};

static void emit_op(TeaParser* parser, TeaOpCode op)
{
    emit_byte(parser, op);

    parser->slot_count += stack_effects[op];
    if(parser->slot_count > parser->function->max_slots)
    {
        parser->function->max_slots = parser->slot_count;
    }
}

static void emit_ops(TeaParser* parser, TeaOpCode op1, TeaOpCode op2)
{
    emit_bytes(parser, op1, op2);

    parser->slot_count += stack_effects[op1] + stack_effects[op2];
    if(parser->slot_count > parser->function->max_slots)
    {
        parser->function->max_slots = parser->slot_count;
    }
}

static void emit_argued(TeaParser* parser, TeaOpCode op, uint8_t byte)
{
    emit_bytes(parser, op, byte);

    parser->slot_count += stack_effects[op];
    if(parser->slot_count > parser->function->max_slots)
    {
        parser->function->max_slots = parser->slot_count;
    }
}

static void emit_loop(TeaParser* parser, int loop_start)
{
    emit_op(parser, OP_LOOP);

    int offset = current_chunk(parser)->count - loop_start + 2;
    if(offset > UINT16_MAX)
        error(parser, "Loop body too large");

    emit_byte(parser, (offset >> 8) & 0xff);
    emit_byte(parser, offset & 0xff);
}

static int emit_jump(TeaParser* parser, uint8_t instruction)
{
    emit_op(parser, instruction);
    emit_bytes(parser, 0xff, 0xff);

    return current_chunk(parser)->count - 2;
}

static void emit_return(TeaParser* parser)
{
    if(parser->type == TYPE_CONSTRUCTOR)
    {
        emit_argued(parser, OP_GET_LOCAL, 0);
    }
    else
    {
        emit_op(parser, OP_NULL);
    }

    emit_byte(parser, OP_RETURN);
}

static uint8_t make_constant(TeaParser* parser, TeaValue value)
{
    int constant = tea_chunk_add_constant(parser->lex->T, current_chunk(parser), value);
    if(constant > UINT8_MAX)
    {
        error(parser, "Too many constants in one chunk");
    }

    return (uint8_t)constant;
}

static void invoke_method(TeaParser* parser, int args, const char* name)
{
    emit_argued(parser, OP_INVOKE, make_constant(parser, OBJECT_VAL(tea_str_copy(parser->lex->T, name, strlen(name)))));
    emit_byte(parser, args);
}

static void emit_constant(TeaParser* parser, TeaValue value)
{
    emit_argued(parser, OP_CONSTANT, make_constant(parser, value));
}

static void patch_jump(TeaParser* parser, int offset)
{
    /* -2 to adjust for the bytecode for the jump offset itself */
    int jump = current_chunk(parser)->count - offset - 2;

    if(jump > UINT16_MAX)
    {
        error(parser, "Too much code to jump over");
    }

    current_chunk(parser)->code[offset] = (jump >> 8) & 0xff;
    current_chunk(parser)->code[offset + 1] = jump & 0xff;
}

static void init_compiler(TeaLexer* lexer, TeaParser* parser, TeaParser* parent, TeaFunctionType type)
{
    parser->lex = lexer;
    parser->enclosing = parent;
    parser->function = NULL;
    parser->klass = NULL;
    parser->loop = NULL;

    if(parent != NULL)
    {
        parser->klass = parent->klass;
    }

    parser->type = type;
    parser->local_count = 1;
    parser->slot_count = parser->local_count;
    parser->scope_depth = 0;

    lexer->T->parser = parser;

    parser->function = tea_func_new_function(lexer->T, type, lexer->module, parser->slot_count);

    if(type != TYPE_SCRIPT)
    {
        parser->function->name = tea_str_copy(lexer->T, lexer->previous.start, lexer->previous.length);
    }

    TeaLocal* local = &parser->locals[0];
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

static TeaOFunction* end_compiler(TeaParser* parser)
{
    emit_return(parser);
    TeaOFunction* function = parser->function;

#ifdef TEA_DEBUG_PRINT_CODE
    TeaState* T = parser->lex->T;
    tea_debug_chunk(T, current_chunk(parser), function->name != NULL ? function->name->chars : "<script>");
#endif

    if(parser->enclosing != NULL)
    {
        emit_argued(parser->enclosing, OP_CLOSURE, make_constant(parser->enclosing, OBJECT_VAL(function)));

        for(int i = 0; i < function->upvalue_count; i++)
        {
            emit_byte(parser->enclosing, parser->upvalues[i].is_local ? 1 : 0);
            emit_byte(parser->enclosing, parser->upvalues[i].index);
        }
    }

    parser->lex->T->parser = parser->enclosing;

    return function;
}

static void begin_scope(TeaParser* parser)
{
    parser->scope_depth++;
}

static int discard_locals(TeaParser* parser, int depth)
{
    int local;
    for(local = parser->local_count - 1; local >= 0 && parser->locals[local].depth >= depth; local--)
    {
        if(parser->locals[local].is_captured)
        {
            emit_byte(parser, OP_CLOSE_UPVALUE);
        }
        else
        {
            emit_byte(parser, OP_POP);
        }
    }

    return parser->local_count - local - 1;
}

static void end_scope(TeaParser* parser)
{
    int effect = discard_locals(parser, parser->scope_depth);
    parser->local_count -= effect;
    parser->slot_count -= effect;
    parser->scope_depth--;
}

static void expression(TeaParser* parser);
static void statement(TeaParser* parser);
static void declaration(TeaParser* parser);
static TeaParseRule* get_rule(TeaTokenType type);
static void parse_precedence(TeaParser* parser, TeaPrecedence precedence);
static void arrow(TeaParser* parser, TeaParser* fn_parser, TeaToken name);
static void anonymous(TeaParser* parser, bool can_assign);
static void grouping(TeaParser* parser, bool can_assign);

static uint8_t identifier_constant(TeaParser* parser, TeaToken* name)
{
    return make_constant(parser, OBJECT_VAL(tea_str_copy(parser->lex->T, name->start, name->length)));
}

static bool identifiers_equal(TeaToken* a, TeaToken* b)
{
    if(a->length != b->length)
        return false;

    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolve_local(TeaParser* parser, TeaToken* name)
{
    for(int i = parser->local_count - 1; i >= 0; i--)
    {
        TeaLocal* local = &parser->locals[i];
        if(identifiers_equal(name, &local->name))
        {
            if(local->depth == -1)
            {
                break;
            }

            return i;
        }
    }

    return -1;
}

static int add_upvalue(TeaParser* parser, uint8_t index, bool is_local, bool constant)
{
    int upvalue_count = parser->function->upvalue_count;

    for(int i = 0; i < upvalue_count; i++)
    {
        TeaUpvalue* upvalue = &parser->upvalues[i];
        if(upvalue->index == index && upvalue->is_local == is_local)
        {
            return i;
        }
    }

    if(upvalue_count == UINT8_COUNT)
    {
        error(parser, "Too many closure variables in function");
    }

    parser->upvalues[upvalue_count].is_local = is_local;
    parser->upvalues[upvalue_count].index = index;
    parser->upvalues[upvalue_count].constant = constant;

    return parser->function->upvalue_count++;
}

static int resolve_upvalue(TeaParser* parser, TeaToken* name)
{
    if(parser->enclosing == NULL)
        return -1;

    int local = resolve_local(parser->enclosing, name);
    bool constant = parser->enclosing->locals[local].constant;
    if(local != -1)
    {
        parser->enclosing->locals[local].is_captured = true;
        return add_upvalue(parser, (uint8_t)local, true, constant);
    }

    int upvalue = resolve_upvalue(parser->enclosing, name);
    constant = parser->enclosing->upvalues[upvalue].constant;
    if(upvalue != -1)
    {
        return add_upvalue(parser, (uint8_t)upvalue, false, constant);
    }

    return -1;
}

static void add_local(TeaParser* parser, TeaToken name)
{
    if(parser->local_count == UINT8_COUNT)
    {
        error(parser, "Too many local variables in function");
    }

    TeaLocal* local = &parser->locals[parser->local_count++];
    local->name = name;
    local->depth = -1;
    local->is_captured = false;
    local->constant = false;
}

static int add_init_local(TeaParser* parser, TeaToken name, bool constant)
{
    add_local(parser, name);

    TeaLocal* local = &parser->locals[parser->local_count - 1];
    local->depth = parser->scope_depth;
    local->constant = constant;

    return parser->local_count - 1;
}

static void declare_variable(TeaParser* parser, TeaToken* name)
{
    if(parser->scope_depth == 0)
        return;

    add_local(parser, *name);
}

static uint8_t parse_variable(TeaParser* parser, const char* error_message)
{
    consume(parser, TOKEN_NAME, error_message);

    declare_variable(parser, &parser->lex->previous);
    if(parser->scope_depth > 0)
        return 0;

    return identifier_constant(parser, &parser->lex->previous);
}

static uint8_t parse_variable_at(TeaParser* parser, TeaToken name)
{
    declare_variable(parser, &name);
    if(parser->scope_depth > 0)
        return 0;

    return identifier_constant(parser, &name);
}

static void mark_initialized(TeaParser* parser, bool constant)
{
    if(parser->scope_depth == 0) return;

    parser->locals[parser->local_count - 1].depth = parser->scope_depth;
    parser->locals[parser->local_count - 1].constant = constant;
}

static void define_variable(TeaParser* parser, uint8_t global, bool constant)
{
    if(parser->scope_depth > 0)
    {
        mark_initialized(parser, constant);
        return;
    }

    TeaOString* string = AS_STRING(current_chunk(parser)->constants.values[global]);
    if(constant)
    {
        tea_tab_set(parser->lex->T, &parser->lex->T->constants, string, NULL_VAL);
    }

    TeaValue value;
    if(tea_tab_get(&parser->lex->T->globals, string, &value))
    {
        emit_argued(parser, OP_DEFINE_GLOBAL, global);
    }
    else
    {
        emit_argued(parser, OP_DEFINE_MODULE, global);
    }
}

static uint8_t argument_list(TeaParser* parser)
{
    uint8_t arg_count = 0;
    if(!check(parser, TOKEN_RIGHT_PAREN))
    {
        do
        {
            expression(parser);
            if(arg_count == 255)
            {
                error(parser, "Can't have more than 255 arguments");
            }
            arg_count++;
        }
        while(match(parser, TOKEN_COMMA));
    }
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after arguments");

    return arg_count;
}

static void and_(TeaParser* parser, bool can_assign)
{
    int jump = emit_jump(parser, OP_AND);
    parse_precedence(parser, PREC_AND);
    patch_jump(parser, jump);
}

static void binary(TeaParser* parser, bool can_assign)
{
    TeaTokenType operator_type = parser->lex->previous.type;

    if(operator_type == TOKEN_BANG)
    {
        consume(parser, TOKEN_IN, "Expected 'not in' binary operator");

        TeaParseRule* rule = get_rule(operator_type);
        parse_precedence(parser, (TeaPrecedence)(rule->precedence + 1));

        emit_ops(parser, OP_IN, OP_NOT);
        return;
    }

    if(operator_type == TOKEN_IS && match(parser, TOKEN_BANG))
    {
        TeaParseRule* rule = get_rule(operator_type);
        parse_precedence(parser, (TeaPrecedence)(rule->precedence + 1));

        emit_ops(parser, OP_IS, OP_NOT);
        return;
    }

    TeaParseRule* rule = get_rule(operator_type);
    parse_precedence(parser, (TeaPrecedence)(rule->precedence + 1));

    switch(operator_type)
    {
        case TOKEN_BANG_EQUAL:
        {
            emit_ops(parser, OP_EQUAL, OP_NOT);
            break;
        }
        case TOKEN_EQUAL_EQUAL:
        {
            emit_op(parser, OP_EQUAL);
            break;
        }
        case TOKEN_IS:
        {
            emit_op(parser, OP_IS);
            break;
        }
        case TOKEN_GREATER:
        {
            emit_op(parser, OP_GREATER);
            break;
        }
        case TOKEN_GREATER_EQUAL:
        {
            emit_op(parser, OP_GREATER_EQUAL);
            break;
        }
        case TOKEN_LESS:
        {
            emit_op(parser, OP_LESS);
            break;
        }
        case TOKEN_LESS_EQUAL:
        {
            emit_op(parser, OP_LESS_EQUAL);
            break;
        }
        case TOKEN_PLUS:
        {
            emit_op(parser, OP_ADD);
            break;
        }
        case TOKEN_MINUS:
        {
            emit_op(parser, OP_SUBTRACT);
            break;
        }
        case TOKEN_STAR:
        {
            emit_op(parser, OP_MULTIPLY);
            break;
        }
        case TOKEN_SLASH:
        {
            emit_op(parser, OP_DIVIDE);
            break;
        }
        case TOKEN_PERCENT:
        {
            emit_op(parser, OP_MOD);
            break;
        }
        case TOKEN_STAR_STAR:
        {
            emit_op(parser, OP_POW);
            break;
        }
        case TOKEN_AMPERSAND:
        {
            emit_op(parser, OP_BAND);
            break;
        }
        case TOKEN_PIPE:
        {
            emit_op(parser, OP_BOR);
            break;
        }
        case TOKEN_CARET:
        {
            emit_op(parser, OP_BXOR);
            break;
        }
        case TOKEN_GREATER_GREATER:
        {
            emit_op(parser, OP_RSHIFT);
            break;
        }
        case TOKEN_LESS_LESS:
        {
            emit_op(parser, OP_LSHIFT);
            break;
        }
        case TOKEN_IN:
        {
            emit_op(parser, OP_IN);
            break;
        }
        default: return; /* Unreachable */
    }
}

static void ternary(TeaParser* parser, bool can_assign)
{
    /* Jump to else branch if the condition is false */
    int else_jump = emit_jump(parser, OP_JUMP_IF_FALSE);

    /* Pop the condition */
    emit_op(parser, OP_POP);
    expression(parser);

    int end_jump = emit_jump(parser, OP_JUMP);

    patch_jump(parser, else_jump);
    emit_op(parser, OP_POP);

    consume(parser, TOKEN_COLON, "Expected colon after ternary expression");
    expression(parser);

    patch_jump(parser, end_jump);
}

static void call(TeaParser* parser, bool can_assign)
{
    uint8_t arg_count = argument_list(parser);
    emit_argued(parser, OP_CALL, arg_count);
}

static void dot(TeaParser* parser, bool can_assign)
{
#define SHORT_HAND_ASSIGNMENT(op) \
    emit_argued(parser, OP_GET_PROPERTY_NO_POP, name); \
    expression(parser); \
    emit_op(parser, op); \
    emit_argued(parser, OP_SET_PROPERTY, name);

#define SHORT_HAND_INCREMENT(op) \
    emit_argued(parser, OP_GET_PROPERTY_NO_POP, name); \
    emit_constant(parser, NUMBER_VAL(1)); \
    emit_op(parser, op); \
    emit_argued(parser, OP_SET_PROPERTY, name);

    consume(parser, TOKEN_NAME, "Expect property name after '.'");
    uint8_t name = identifier_constant(parser, &parser->lex->previous);

    if(match(parser, TOKEN_LEFT_PAREN))
    {
        uint8_t arg_count = argument_list(parser);
        emit_argued(parser, OP_INVOKE, name);
        emit_byte(parser, arg_count);
        return;
    }

    if(can_assign && match(parser, TOKEN_EQUAL))
    {
        expression(parser);
        emit_argued(parser, OP_SET_PROPERTY, name);
    }
    else if(can_assign && match(parser, TOKEN_PLUS_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_ADD);
    }
    else if(can_assign && match(parser, TOKEN_MINUS_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_SUBTRACT);
    }
    else if(can_assign && match(parser, TOKEN_STAR_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_MULTIPLY);
    }
    else if(can_assign && match(parser, TOKEN_SLASH_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_DIVIDE);
    }
    else if(can_assign && match(parser, TOKEN_PERCENT_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_MOD);
    }
    else if(can_assign && match(parser, TOKEN_STAR_STAR_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_POW);
    }
    else if(can_assign && match(parser, TOKEN_AMPERSAND_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_BAND);
    }
    else if(can_assign && match(parser, TOKEN_PIPE_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_BOR);
    }
    else if(can_assign && match(parser, TOKEN_CARET_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_BXOR);
    }
    else
    {
        if(match(parser, TOKEN_PLUS_PLUS))
        {
            SHORT_HAND_INCREMENT(OP_ADD);
        }
        else if(match(parser, TOKEN_MINUS_MINUS))
        {
            SHORT_HAND_INCREMENT(OP_SUBTRACT);
        }
        else
        {
            emit_argued(parser, OP_GET_PROPERTY, name);
        }
    }
#undef SHORT_HAND_ASSIGNMENT
#undef SHORT_HAND_INCREMENT
}

static void boolean_(TeaParser* parser, bool can_assign)
{
    emit_op(parser, parser->lex->previous.type == TOKEN_FALSE ? OP_FALSE : OP_TRUE);
}

static void null(TeaParser* parser, bool can_assign)
{
    emit_op(parser, OP_NULL);
}

static void list(TeaParser* parser, bool can_assign)
{
    emit_op(parser, OP_LIST);

    if(!check(parser, TOKEN_RIGHT_BRACKET))
    {
        do
        {
            if(check(parser, TOKEN_RIGHT_BRACKET))
            {
                /* Traling comma case */
                break;
            }

            expression(parser);
            emit_op(parser, OP_PUSH_LIST_ITEM);
        }
        while(match(parser, TOKEN_COMMA));
    }

    consume(parser, TOKEN_RIGHT_BRACKET, "Expect ']' after list literal");
}

static void map(TeaParser* parser, bool can_assign)
{
    emit_op(parser, OP_MAP);

    if(!check(parser, TOKEN_RIGHT_BRACE))
    {
        do
        {
            if(check(parser, TOKEN_RIGHT_BRACE))
            {
                /* Traling comma case */
                break;
            }

            if(match(parser, TOKEN_LEFT_BRACKET))
            {
                expression(parser);
                consume(parser, TOKEN_RIGHT_BRACKET, "Expect ']' after key expression");
                consume(parser, TOKEN_EQUAL, "Expected '=' after key expression");
                expression(parser);
            }
            else if(match(parser, TOKEN_NAME))
            {
                emit_constant(parser, OBJECT_VAL(tea_str_copy(parser->lex->T, parser->lex->previous.start, parser->lex->previous.length)));
                consume(parser, TOKEN_EQUAL, "Expected '=' after key name");
                expression(parser);
            }

            emit_op(parser, OP_PUSH_MAP_FIELD);
        }
        while(match(parser, TOKEN_COMMA));
    }

    consume(parser, TOKEN_RIGHT_BRACE, "Expect '} after map literal'");
}

static bool s_expression(TeaParser* parser)
{
    expression(parser);

    // it's a slice
    if(match(parser, TOKEN_COLON))
    {
        // [n:]
        if(check(parser, TOKEN_RIGHT_BRACKET))
        {
            null(parser, false);
            emit_constant(parser, NUMBER_VAL(1));
        }
        else
        {
            // [n::n]
            if(match(parser, TOKEN_COLON))
            {
                null(parser, false);
                expression(parser);
            }
            else
            {
                expression(parser);
                if(match(parser, TOKEN_COLON))
                {
                    // [n:n:n]
                    expression(parser);
                }
                else
                {
                    emit_constant(parser, NUMBER_VAL(1));
                }
            }
        }
        return true;
    }
    return false;
}

static void csubscript(TeaParser* parser, bool can_assign)
{
#define SHORT_HAND_ASSIGNMENT(op) \
    expression(parser); \
    emit_ops(parser, OP_SUBSCRIPT_PUSH, op); \
    emit_op(parser, OP_SUBSCRIPT_STORE);

#define SHORT_HAND_INCREMENT(op) \
    emit_constant(parser, NUMBER_VAL(1)); \
    emit_ops(parser, OP_SUBSCRIPT_PUSH, op); \
    emit_op(parser, OP_SUBSCRIPT_STORE);

    if(s_expression(parser))
    {
        emit_op(parser, OP_SLICE);
        consume(parser, TOKEN_RIGHT_BRACKET, "Expect ']' after closing");
        return;
    }

    consume(parser, TOKEN_RIGHT_BRACKET, "Expect ']' after closing");

    if(can_assign && match(parser, TOKEN_EQUAL))
    {
        expression(parser);
        emit_op(parser, OP_SUBSCRIPT_STORE);
    }
    else if(can_assign && match(parser, TOKEN_PLUS_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_ADD);
    }
    else if(can_assign && match(parser, TOKEN_MINUS_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_SUBTRACT);
    }
    else if(can_assign && match(parser, TOKEN_STAR_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_MULTIPLY);
    }
    else if(can_assign && match(parser, TOKEN_SLASH_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_DIVIDE);
    }
    else if(can_assign && match(parser, TOKEN_PERCENT_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_MOD);
    }
    else if(can_assign && match(parser, TOKEN_STAR_STAR_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_POW);
    }
    else if(can_assign && match(parser, TOKEN_AMPERSAND_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_BAND);
    }
    else if(can_assign && match(parser, TOKEN_PIPE_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_BOR);
    }
    else if(can_assign && match(parser, TOKEN_CARET_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_BXOR);
    }
    else
    {
        if(match(parser, TOKEN_PLUS_PLUS))
        {
            SHORT_HAND_INCREMENT(OP_ADD);
        }
        else if(match(parser, TOKEN_MINUS_MINUS))
        {
            SHORT_HAND_INCREMENT(OP_SUBTRACT);
        }
        else
        {
            emit_op(parser, OP_SUBSCRIPT);
        }
    }

#undef SHORT_HAND_ASSIGNMENT
#undef SHORT_HAND_INCREMENT
}

static void or_(TeaParser* parser, bool can_assign)
{
    int jump = emit_jump(parser, OP_OR);
    parse_precedence(parser, PREC_OR);
    patch_jump(parser, jump);
}

static void literal(TeaParser* parser, bool can_assign)
{
    emit_constant(parser, parser->lex->previous.value);
}

static void interpolation(TeaParser* parser, bool can_assign)
{
    emit_op(parser, OP_LIST);

    do
    {
        literal(parser, false);
        invoke_method(parser, 1, "add");

        expression(parser);

        invoke_method(parser, 1, "add");
    }
    while(match(parser, TOKEN_INTERPOLATION));

    consume(parser, TOKEN_STRING, "Expect end of string interpolation");
    literal(parser, false);
    invoke_method(parser, 1, "add");

    invoke_method(parser, 0, "join");
}

static void check_const(TeaParser* parser, uint8_t set_op, int arg)
{
    switch(set_op)
    {
        case OP_SET_LOCAL:
        {
            if(parser->locals[arg].constant)
            {
                error(parser, "Cannot assign to a constant");
            }

            break;
        }
        case OP_SET_UPVALUE:
        {
            TeaUpvalue upvalue = parser->upvalues[arg];

            if(upvalue.constant)
            {
                error(parser, "Cannot assign to a constant");
            }

            break;
        }
        case OP_SET_GLOBAL:
        case OP_SET_MODULE:
        {
            TeaOString* string = AS_STRING(current_chunk(parser)->constants.values[arg]);
            TeaValue _;
            if(tea_tab_get(&parser->lex->T->constants, string, &_))
            {
                error(parser, "Cannot assign to a constant");
            }

            break;
        }
        default:;
    }
}

static void named_variable(TeaParser* parser, TeaToken name, bool can_assign)
{
#define SHORT_HAND_ASSIGNMENT(op) \
    check_const(parser, set_op, arg); \
    emit_argued(parser, get_op, (uint8_t)arg); \
    expression(parser); \
    emit_op(parser, op); \
    emit_argued(parser, set_op, (uint8_t)arg);

#define SHORT_HAND_INCREMENT(op) \
    check_const(parser, set_op, arg); \
    emit_argued(parser, get_op, (uint8_t)arg); \
    emit_constant(parser, NUMBER_VAL(1)); \
    emit_op(parser, op); \
    emit_argued(parser, set_op, (uint8_t)arg);

    uint8_t get_op, set_op;
    int arg = resolve_local(parser, &name);
    if(arg != -1)
    {
        get_op = OP_GET_LOCAL;
        set_op = OP_SET_LOCAL;
    }
    else if((arg = resolve_upvalue(parser, &name)) != -1)
    {
        get_op = OP_GET_UPVALUE;
        set_op = OP_SET_UPVALUE;
    }
    else
    {
        arg = identifier_constant(parser, &name);
        TeaOString* string = tea_str_copy(parser->lex->T, name.start, name.length);
        TeaValue value;
        if(tea_tab_get(&parser->lex->T->globals, string, &value))
        {
            get_op = OP_GET_GLOBAL;
            set_op = OP_SET_GLOBAL;
        }
        else
        {
            get_op = OP_GET_MODULE;
            set_op = OP_SET_MODULE;
        }
    }

    if(can_assign && match(parser, TOKEN_EQUAL))
    {
        check_const(parser, set_op, arg);
        expression(parser);
        emit_argued(parser, set_op, (uint8_t)arg);
    }
    else if(can_assign && match(parser, TOKEN_PLUS_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_ADD);
    }
    else if(can_assign && match(parser, TOKEN_MINUS_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_SUBTRACT);
    }
    else if(can_assign && match(parser, TOKEN_STAR_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_MULTIPLY);
    }
    else if(can_assign && match(parser, TOKEN_SLASH_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_DIVIDE);
    }
    else if(can_assign && match(parser, TOKEN_PERCENT_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_MOD);
    }
    else if(can_assign && match(parser, TOKEN_STAR_STAR_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_POW);
    }
    else if(can_assign && match(parser, TOKEN_AMPERSAND_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_BAND);
    }
    else if(can_assign && match(parser, TOKEN_PIPE_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_BOR);
    }
    else if(can_assign && match(parser, TOKEN_CARET_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_BXOR);
    }
    else
    {
        if(match(parser, TOKEN_PLUS_PLUS))
        {
            SHORT_HAND_INCREMENT(OP_ADD);
        }
        else if(match(parser, TOKEN_MINUS_MINUS))
        {
            SHORT_HAND_INCREMENT(OP_SUBTRACT);
        }
        else
        {
            emit_argued(parser, get_op, (uint8_t)arg);
        }
    }
#undef SHORT_HAND_ASSIGNMENT
#undef SHORT_HAND_INCREMENT
}

static void variable(TeaParser* parser, bool can_assign)
{
    named_variable(parser, parser->lex->previous, can_assign);
}

static TeaToken synthetic_token(const char* text)
{
    TeaToken token;
    token.start = text;
    token.length = (int)strlen(text);

    return token;
}

static void super_(TeaParser* parser, bool can_assign)
{
    if(parser->klass == NULL)
    {
        error(parser, "Can't use 'super' outside of a class");
    }
    else if(parser->klass->is_static)
    {
        error(parser, "Can't use 'this' inside a static method");
    }
    else if(!parser->klass->has_superclass)
    {
        error(parser, "Can't use 'super' in a class with no superclass");
    }

    /* super */
    if(!check(parser, TOKEN_LEFT_PAREN) && !check(parser, TOKEN_DOT))
    {
        named_variable(parser, synthetic_token("super"), false);
        return;
    }

    /* super() -> super.constructor() */
    if(match(parser, TOKEN_LEFT_PAREN))
    {
        TeaToken token = synthetic_token("constructor");

        uint8_t name = identifier_constant(parser, &token);
        named_variable(parser, synthetic_token("this"), false);
        uint8_t arg_count = argument_list(parser);
        named_variable(parser, synthetic_token("super"), false);
        emit_argued(parser, OP_SUPER, name);
        emit_byte(parser, arg_count);
        return;
    }

    /* super.name */
    consume(parser, TOKEN_DOT, "Expect '.' after 'super'");
    consume(parser, TOKEN_NAME, "Expect superclass method name");
    uint8_t name = identifier_constant(parser, &parser->lex->previous);

    named_variable(parser, synthetic_token("this"), false);

    if(match(parser, TOKEN_LEFT_PAREN))
    {
        /* super.name() */
        uint8_t arg_count = argument_list(parser);
        named_variable(parser, synthetic_token("super"), false);
        emit_argued(parser, OP_SUPER, name);
        emit_byte(parser, arg_count);
    }
    else
    {
        /* super.name */
        named_variable(parser, synthetic_token("super"), false);
        emit_argued(parser, OP_GET_SUPER, name);
    }
}

static void this_(TeaParser* parser, bool can_assign)
{
    if(parser->klass == NULL)
    {
        error(parser, "Can't use 'this' outside of a class");
    }
    else if(parser->klass->is_static)
    {
        error(parser, "Can't use 'this' inside a static method");
    }

    variable(parser, false);
}

static void static_(TeaParser* parser, bool can_assign)
{
    if(parser->klass == NULL)
    {
        error(parser, "Can't use 'static' outside of a class");
    }
}

static void unary(TeaParser* parser, bool can_assign)
{
    TeaTokenType operator_type = parser->lex->previous.type;

    parse_precedence(parser, PREC_UNARY);

    /* Emit the operator instruction */
    switch(operator_type)
    {
        case TOKEN_BANG:
        {
            emit_op(parser, OP_NOT);
            break;
        }
        case TOKEN_MINUS:
        {
            emit_op(parser, OP_NEGATE);
            break;
        }
        case TOKEN_TILDE:
        {
            emit_op(parser, OP_BNOT);
            break;
        }
        default:
            return; /* Unreachable */
    }
}

static void range(TeaParser* parser, bool can_assign)
{
    TeaTokenType operator_type = parser->lex->previous.type;
    TeaParseRule* rule = get_rule(operator_type);
    parse_precedence(parser, (TeaPrecedence)(rule->precedence + 1));

    if(match(parser, TOKEN_DOT_DOT))
    {
        TeaTokenType operator_type = parser->lex->previous.type;
        TeaParseRule* rule = get_rule(operator_type);
        parse_precedence(parser, (TeaPrecedence)(rule->precedence + 1));
    }
    else
    {
        emit_constant(parser, NUMBER_VAL(1));
    }

    emit_op(parser, OP_RANGE);
}

#define NONE                    { NULL, NULL, PREC_NONE }
#define RULE(pr, in, prec)      { pr, in, PREC_##prec }
#define INFIX(in)               { NULL, in, PREC_NONE }
#define PREFIX(pr)              { pr, NULL, PREC_NONE }
#define OPERATOR(in, prec)      { NULL, in, PREC_##prec }

static TeaParseRule rules[] = {
    RULE(grouping, call, CALL),             /* TOKEN_LEFT_PAREN */
    NONE,                                   /* TOKEN_RIGHT_PAREN */
    RULE(list, csubscript, SUBSCRIPT),      /* TOKEN_LEFT_BRACKET */
    NONE,                                   /* TOKEN_RIGHT_BRACKET */
    PREFIX(map),                            /* TOKEN_LEFT_BRACE */
    NONE,                                   /* TOKEN_RIGHT_BRACE */
    NONE,                                   /* TOKEN_COMMA */
    NONE,                                   /* TOKEN_SEMICOLON */
    OPERATOR(dot, CALL),                    /* TOKEN_DOT */
    NONE,                                   /* TOKEN_COLON */
    OPERATOR(ternary, ASSIGNMENT),          /* TOKEN_QUESTION */
    RULE(unary, binary, TERM),              /* TOKEN_MINUS */
    OPERATOR(binary, TERM),                 /* TOKEN_PLUS */
    OPERATOR(binary, FACTOR),               /* TOKEN_SLASH */
    OPERATOR(binary, FACTOR),               /* TOKEN_STAR */
    NONE,                                   /* TOKEN_PLUS_PLUS */
    NONE,                                   /* TOKEN_MINUS_MINUS */
    NONE,                                   /* TOKEN_PLUS_EQUAL */
    NONE,                                   /* TOKEN_MINUS_EQUAL */
    NONE,                                   /* TOKEN_STAR_EQUAL */
    NONE,                                   /* TOKEN_SLASH_EQUAL */
    RULE(unary, binary, IS),                /* TOKEN_BANG */
    OPERATOR(binary, EQUALITY),             /* TOKEN_BANG_EQUAL */
    NONE,                                   /* TOKEN_EQUAL */
    OPERATOR(binary, EQUALITY),             /* TOKEN_EQUAL_EQUAL */
    OPERATOR(binary, COMPARISON),           /* TOKEN_GREATER */
    OPERATOR(binary, COMPARISON),           /* TOKEN_GREATER_EQUAL */
    OPERATOR(binary, COMPARISON),           /* TOKEN_LESS */
    OPERATOR(binary, COMPARISON),           /* TOKEN_LESS_EQUAL */
    OPERATOR(binary, FACTOR),               /* TOKEN_PERCENT */
    NONE,                                   /* TOKEN_PERCENT_EQUAL */
    OPERATOR(binary, INDICES),              /* TOKEN_STAR_STAR */
    NONE,                                   /* TOKEN_STAR_STAR_EQUAL */
    OPERATOR(range, RANGE),                 /* TOKEN_DOT_DOT */
    NONE,                                   /* TOKEN_DOT_DOT_DOT */
    OPERATOR(binary, BAND),                 /* TOKEN_AMPERSAND */
    NONE,                                   /* TOKEN_AMPERSAND_EQUAL */
    OPERATOR(binary, BOR),                  /* TOKEN_PIPE */
    NONE,                                   /* TOKEN_PIPE_EQUAL */
    OPERATOR(binary, BXOR),                 /* TOKEN_CARET */
    NONE,                                   /* TOKEN_CARET_EQUAL */
    PREFIX(unary),                          /* TOKEN_TILDE */
    NONE,                                   /* TOKEN_ARROW */
    OPERATOR(binary, SHIFT),                /* TOKEN_GREATER_GREATER */
    OPERATOR(binary, SHIFT),                /* TOKEN_LESS_LESS */
    PREFIX(variable),                       /* TOKEN_NAME */
    PREFIX(literal),                        /* TOKEN_STRING */
    PREFIX(interpolation),                  /* TOKEN_INTERPOLATION */
    PREFIX(literal),                        /* TOKEN_NUMBER */
    OPERATOR(and_, AND),                    /* TOKEN_AND */
    NONE,                                   /* TOKEN_CLASS */
    PREFIX(static_),                        /* TOKEN_STATIC */
    NONE,                                   /* TOKEN_ELSE */
    PREFIX(boolean_),                       /* TOKEN_FALSE */
    NONE,                                   /* TOKEN_FOR */
    PREFIX(anonymous),                      /* TOKEN_FUNCTION */
    NONE,                                   /* TOKEN_CASE */
    NONE,                                   /* TOKEN_SWITCH */
    NONE,                                   /* TOKEN_DEFAULT */
    NONE,                                   /* TOKEN_IF */
    PREFIX(null),                           /* TOKEN_NULL */
    OPERATOR(or_, OR),                      /* TOKEN_OR */
    OPERATOR(binary, IS),                   /* TOKEN_IS */
    NONE,                                   /* TOKEN_IMPORT */
    NONE,                                   /* TOKEN_FROM */
    NONE,                                   /* TOKEN_AS */
    NONE,                                   /* TOKEN_ENUM */
    NONE,                                   /* TOKEN_RETURN */
    PREFIX(super_),                         /* TOKEN_SUPER */
    PREFIX(this_),                          /* TOKEN_THIS */
    NONE,                                   /* TOKEN_CONTINUE */
    NONE,                                   /* TOKEN_BREAK */
    OPERATOR(binary, COMPARISON),           /* TOKEN_IN */
    PREFIX(boolean_),                       /* TOKEN_TRUE */
    NONE,                                   /* TOKEN_VAR */
    NONE,                                   /* TOKEN_CONST */
    NONE,                                   /* TOKEN_WHILE */
    NONE,                                   /* TOKEN_DO */
    NONE,                                   /* TOKEN_ERROR */
    NONE,                                   /* TOKEN_EOF */
};

#undef NONE
#undef RULE
#undef INFIX
#undef PREFIX
#undef OPERATOR

static void parse_precedence(TeaParser* parser, TeaPrecedence precedence)
{
    next(parser);
    TeaParseFn prefix_rule = get_rule(parser->lex->previous.type)->prefix;
    if(prefix_rule == NULL)
    {
        error(parser, "Expect expression");
    }

    bool can_assign = precedence <= PREC_ASSIGNMENT;
    prefix_rule(parser, can_assign);

    while(precedence <= get_rule(parser->lex->current.type)->precedence)
    {
        next(parser);
        TeaParseFn infix_rule = get_rule(parser->lex->previous.type)->infix;
        infix_rule(parser, can_assign);
    }

    if(can_assign && match(parser, TOKEN_EQUAL))
    {
        error(parser, "Invalid assignment target");
    }
}

static TeaParseRule* get_rule(TeaTokenType type)
{
    return &rules[type];
}

static void expression(TeaParser* parser)
{
    parse_precedence(parser, PREC_ASSIGNMENT);
}

static void block(TeaParser* parser)
{
    while(!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF))
    {
        declaration(parser);
    }

    consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after block");
}

static void check_parameters(TeaParser* parser, TeaToken* name)
{
    for(int i = parser->local_count - 1; i >= 0; i--)
    {
        TeaLocal* local = &parser->locals[i];
        if(identifiers_equal(name, &local->name))
        {
            error(parser, "Duplicate parameter name in function declaration");
        }
    }
}

static void begin_function(TeaParser* parser, TeaParser* fn_parser, TeaFunctionType type)
{
    init_compiler(parser->lex, fn_parser, parser, type);
    begin_scope(fn_parser);

    consume(fn_parser, TOKEN_LEFT_PAREN, "Expect '(' after function name");

    if(!check(fn_parser, TOKEN_RIGHT_PAREN))
    {
        bool optional = false;
        bool spread = false;

        do
        {
            if(spread)
            {
                error(fn_parser, "spread parameter must be last in the parameter list");
            }

            spread = match(fn_parser, TOKEN_DOT_DOT_DOT);
            consume(fn_parser, TOKEN_NAME, "Expect parameter name");

            TeaToken name = fn_parser->lex->previous;
            check_parameters(fn_parser, &name);

            if(spread)
            {
                fn_parser->function->variadic = spread;
            }

            if(match(fn_parser, TOKEN_EQUAL))
            {
                if(spread)
                {
                    error(fn_parser, "spread parameter cannot have an optional value");
                }
                fn_parser->function->arity_optional++;
                optional = true;
                expression(fn_parser);
            }
            else
            {
                fn_parser->function->arity++;

                if(optional)
                {
                    error(fn_parser, "Cannot have non-optional parameter after optional");
                }
            }

            if(fn_parser->function->arity + fn_parser->function->arity_optional > 255)
            {
                error(fn_parser, "Cannot have more than 255 parameters");
            }

            uint8_t constant = parse_variable_at(fn_parser, name);
            define_variable(fn_parser, constant, false);
        }
        while(match(fn_parser, TOKEN_COMMA));

        if(fn_parser->function->arity_optional > 0)
        {
            emit_op(fn_parser, OP_DEFINE_OPTIONAL);
            emit_bytes(fn_parser, fn_parser->function->arity, fn_parser->function->arity_optional);
        }
    }

    consume(fn_parser, TOKEN_RIGHT_PAREN, "Expect ')' after parameters");
}

static void function(TeaParser* parser, TeaFunctionType type)
{
    TeaParser fn_parser;

    begin_function(parser, &fn_parser, type);
    consume(&fn_parser, TOKEN_LEFT_BRACE, "Expect '{' before function body");
    block(&fn_parser);
    end_compiler(&fn_parser);
}

static void anonymous(TeaParser* parser, bool can_assign)
{
    function(parser, TYPE_FUNCTION);
}

static void grouping(TeaParser* parser, bool can_assign)
{
    if(match(parser, TOKEN_RIGHT_PAREN))
    {
        TeaParser fn_parser;
        init_compiler(parser->lex, &fn_parser, parser, TYPE_FUNCTION);
        begin_scope(&fn_parser);
        consume(parser, TOKEN_ARROW, "Expected arrow function");
        if(match(&fn_parser, TOKEN_LEFT_BRACE))
        {
            block(&fn_parser);
        }
        else
        {
            expression(&fn_parser);
            emit_op(&fn_parser, OP_RETURN);
        }
        end_compiler(&fn_parser);
        return;
    }

    const char* start = parser->lex->previous.start;
	int line = parser->lex->previous.line;

    if(match(parser, TOKEN_NAME))
    {
        TeaToken name = parser->lex->previous;
        if(match(parser, TOKEN_COMMA))
        {
            TeaParser fn_parser;
            arrow(parser, &fn_parser, name);
            end_compiler(&fn_parser);
            return;
        }
        else if(match(parser, TOKEN_RIGHT_PAREN) && match(parser, TOKEN_ARROW))
        {
            TeaParser fn_parser;
            init_compiler(parser->lex, &fn_parser, parser, TYPE_FUNCTION);
            begin_scope(&fn_parser);
            fn_parser.function->arity = 1;
            uint8_t constant = parse_variable_at(&fn_parser, name);
            define_variable(&fn_parser, constant, false);
            if(match(&fn_parser, TOKEN_LEFT_BRACE))
            {
                block(&fn_parser);
            }
            else
            {
                expression(&fn_parser);
                emit_op(&fn_parser, OP_RETURN);
            }
            end_compiler(&fn_parser);
            return;
        }
        else
        {
            TeaLexer* lex = parser->lex;

			lex->curr = start;
			lex->line = line;

			parser->lex->current = tea_lex_token(lex);
			next(parser);
        }
    }

    expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after grouping expression");
}

static void arrow(TeaParser* parser, TeaParser* fn_parser, TeaToken name)
{
    init_compiler(parser->lex, fn_parser, parser, TYPE_FUNCTION);
    begin_scope(fn_parser);

    fn_parser->function->arity = 1;
    uint8_t constant = parse_variable_at(fn_parser, name);
    define_variable(fn_parser, constant, false);
    if(!check(fn_parser, TOKEN_RIGHT_PAREN))
    {
        do
        {
            fn_parser->function->arity++;
            if(fn_parser->function->arity > 255)
            {
                error_at_current(fn_parser, "Can't have more than 255 parameters");
            }
            uint8_t constant = parse_variable(fn_parser, "Expect parameter name");
            define_variable(fn_parser, constant, false);
        }
        while(match(fn_parser, TOKEN_COMMA));
    }

    consume(fn_parser, TOKEN_RIGHT_PAREN, "Expect ')' after parameters");
    consume(fn_parser, TOKEN_ARROW, "Expect '=>' after function arguments");
    if(match(fn_parser, TOKEN_LEFT_BRACE))
    {
        /* Brace so expect a block */
        block(fn_parser);
    }
    else
    {
        /* No brace, so expect single expression */
        expression(fn_parser);
        emit_op(fn_parser, OP_RETURN);
    }
}

static TeaTokenType operators[] = {
    TOKEN_PLUS,             /* + */
    TOKEN_MINUS,            /* - */
    TOKEN_STAR,             /* * */
    TOKEN_SLASH,            /* / */
    TOKEN_PERCENT,          /* % */
    TOKEN_STAR_STAR,        /* ** */
    TOKEN_AMPERSAND,        /* & */
    TOKEN_PIPE,             /* | */
    TOKEN_TILDE,            /* ~ */
    TOKEN_CARET,            /* ^ */
    TOKEN_LESS_LESS,        /* << */
    TOKEN_GREATER_GREATER,  /* >> */
	TOKEN_LESS,             /* < */
	TOKEN_LESS_EQUAL,       /* <= */
	TOKEN_GREATER,          /* > */
	TOKEN_GREATER_EQUAL,    /* >= */
	TOKEN_EQUAL_EQUAL,      /* == */
    TOKEN_LEFT_BRACKET,     /* [] */
    TOKEN_EOF
};

static void operator(TeaParser* parser)
{
    int i = 0;
    while(operators[i] != TOKEN_EOF)
    {
        if(match(parser, operators[i]))
        {
            break;
        }

        i++;
    }

    TeaOString* name = NULL;

    if(parser->lex->previous.type == TOKEN_LEFT_BRACKET)
    {
        parser->klass->is_static = false;
        consume(parser, TOKEN_RIGHT_BRACKET, "Expected ']' after '[' operator method");
        name = tea_str_literal(parser->lex->T, "[]");
    }
    else
    {
        parser->klass->is_static = true;
        name = tea_str_copy(parser->lex->T, parser->lex->previous.start, parser->lex->previous.length);
    }

    uint8_t constant = make_constant(parser, OBJECT_VAL(name));

    function(parser, TYPE_METHOD);
    emit_argued(parser, OP_METHOD, constant);
    parser->klass->is_static = false;
}

static void method(TeaParser* parser, TeaFunctionType type)
{
    uint8_t constant = identifier_constant(parser, &parser->lex->previous);

    if(parser->lex->previous.length == 11 && memcmp(parser->lex->previous.start, "constructor", 11) == 0)
    {
        type = TYPE_CONSTRUCTOR;
    }

    function(parser, type);
    emit_argued(parser, OP_METHOD, constant);
}

static void init_class_compiler(TeaParser* parser, TeaClassParser* class_compiler)
{
    class_compiler->is_static = false;
    class_compiler->has_superclass = false;
    class_compiler->enclosing = parser->klass;
    parser->klass = class_compiler;
}

static void class_body(TeaParser* parser)
{
    while(!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF))
    {
        if(match(parser, TOKEN_VAR))
        {
            consume(parser, TOKEN_NAME, "Expect class variable name");
            uint8_t name = identifier_constant(parser, &parser->lex->previous);

            if(match(parser, TOKEN_EQUAL))
            {
                expression(parser);
            }
            else
            {
                emit_op(parser, OP_NULL);
            }
            emit_argued(parser, OP_SET_CLASS_VAR, name);
        }
        else if(match(parser, TOKEN_STATIC))
        {
            parser->klass->is_static = true;
            consume(parser, TOKEN_NAME, "Expect method name after 'static' keyword");
            method(parser, TYPE_STATIC);
            parser->klass->is_static = false;
        }
        else if(match(parser, TOKEN_NAME))
        {
            method(parser, TYPE_METHOD);
        }
        else
        {
            operator(parser);
        }
    }
}

static void class_declaration(TeaParser* parser)
{
    consume(parser, TOKEN_NAME, "Expect class name");
    TeaToken class_name = parser->lex->previous;
    uint8_t name_constant = identifier_constant(parser, &parser->lex->previous);
    declare_variable(parser, &parser->lex->previous);

    emit_argued(parser, OP_CLASS, name_constant);
    define_variable(parser, name_constant, false);

    TeaClassParser class_compiler;
    init_class_compiler(parser, &class_compiler);

    if(match(parser, TOKEN_COLON))
    {
        expression(parser);

        begin_scope(parser);
        add_local(parser, synthetic_token("super"));
        define_variable(parser, 0, false);

        named_variable(parser, class_name, false);
        emit_op(parser, OP_INHERIT);
        class_compiler.has_superclass = true;
    }

    named_variable(parser, class_name, false);

    consume(parser, TOKEN_LEFT_BRACE, "Expect '{' before class body");
    class_body(parser);
    consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after class body");

    emit_op(parser, OP_POP);

    if(class_compiler.has_superclass)
    {
        end_scope(parser);
    }

    parser->klass = parser->klass->enclosing;
}

static void function_assignment(TeaParser* parser)
{
    if(match(parser, TOKEN_DOT))
    {
        consume(parser, TOKEN_NAME, "Expect property name");
        uint8_t dot = identifier_constant(parser, &parser->lex->previous);
        if(!check(parser, TOKEN_LEFT_PAREN))
        {
            emit_argued(parser, OP_GET_PROPERTY, dot);
            function_assignment(parser);
        }
        else
        {
            function(parser, TYPE_FUNCTION);
            emit_argued(parser, OP_SET_PROPERTY, dot);
            emit_op(parser, OP_POP);
            return;
        }
    }
    else if(match(parser, TOKEN_COLON))
    {
        consume(parser, TOKEN_NAME, "Expect method name");
        uint8_t constant = identifier_constant(parser, &parser->lex->previous);

        TeaClassParser class_compiler;
        init_class_compiler(parser, &class_compiler);

        function(parser, TYPE_METHOD);

        parser->klass = parser->klass->enclosing;

        emit_argued(parser, OP_EXTENSION_METHOD, constant);
        return;
    }
}

static void function_declaration(TeaParser* parser)
{
    consume(parser, TOKEN_NAME, "Expect function name");
    TeaToken name = parser->lex->previous;

    if(check(parser, TOKEN_DOT) || check(parser, TOKEN_COLON))
    {
        named_variable(parser, name, false);
        function_assignment(parser);
        return;
    }

    uint8_t global = parse_variable_at(parser, name);
    mark_initialized(parser, false);
    function(parser, TYPE_FUNCTION);
    define_variable(parser, global, false);
}

static void var_declaration(TeaParser* parser, bool constant)
{
    TeaToken variables[255];
    int var_count = 0;
    int expr_count = 0;
    bool rest = false;
    int rest_count = 0;
    int rest_pos = 0;

    do
    {
        if(rest_count > 1)
        {
            error(parser, "Multiple '...'");
        }

        if(match(parser, TOKEN_DOT_DOT_DOT))
        {
            rest = true;
            rest_count++;
        }

        consume(parser, TOKEN_NAME, "Expect variable name");
        variables[var_count] = parser->lex->previous;
        var_count++;

        if(rest)
        {
            rest_pos = var_count;
            rest = false;
        }

        if(var_count == 1 && match(parser, TOKEN_EQUAL))
        {
            if(rest_count)
            {
                error(parser, "Cannot rest single variable");
            }

            uint8_t global = parse_variable_at(parser, variables[0]);
            expression(parser);
            define_variable(parser, global, constant);

            if(match(parser, TOKEN_COMMA))
            {
                do
                {
                    uint8_t global = parse_variable(parser, "Expect variable name");
                    consume(parser, TOKEN_EQUAL, "Expected an assignment");
                    expression(parser);
                    define_variable(parser, global, constant);
                }
                while(match(parser, TOKEN_COMMA));
            }
            return;
        }
    }
    while(match(parser, TOKEN_COMMA));

    if(rest_count)
    {
        consume(parser, TOKEN_EQUAL, "Expected variable assignment");
        expression(parser);
        emit_op(parser, OP_UNPACK_REST_LIST);
        emit_bytes(parser, var_count, rest_pos - 1);
        goto finish;
    }

    if(match(parser, TOKEN_EQUAL))
    {
        do
        {
            expression(parser);
            expr_count++;
            if(expr_count == 1 && (!check(parser, TOKEN_COMMA)))
            {
                emit_argued(parser, OP_UNPACK_LIST, var_count);
                goto finish;
            }

        }
        while(match(parser, TOKEN_COMMA));

        if(expr_count != var_count)
        {
            error(parser, "Not enough values to assign to");
        }
    }
    else
    {
        for(int i = 0; i < var_count; i++)
        {
            emit_op(parser, OP_NULL);
        }
    }

    finish:
    if(parser->scope_depth == 0)
    {
        for(int i = var_count - 1; i >= 0; i--)
        {
            uint8_t identifier = identifier_constant(parser, &variables[i]);
            define_variable(parser, identifier, constant);
        }
    }
    else
    {
        for(int i = 0; i < var_count; i++)
        {
            declare_variable(parser, &variables[i]);
            define_variable(parser, 0, constant);
        }
    }
}

static void expression_statement(TeaParser* parser)
{
    expression(parser);
    if(parser->lex->T->repl && parser->type == TYPE_SCRIPT)
    {
        emit_op(parser, OP_POP_REPL);
    }
    else
    {
        emit_op(parser, OP_POP);
    }
}

static int get_arg_count(uint8_t* code, const TeaValueArray constants, int ip)
{
    switch(code[ip])
    {
        case OP_NULL:
        case OP_TRUE:
        case OP_FALSE:
        case OP_RANGE:
        case OP_SUBSCRIPT:
        case OP_SUBSCRIPT_STORE:
        case OP_SUBSCRIPT_PUSH:
        case OP_SLICE:
        case OP_INHERIT:
        case OP_POP:
        case OP_POP_REPL:
        case OP_IS:
        case OP_IN:
        case OP_EQUAL:
        case OP_GREATER:
        case OP_GREATER_EQUAL:
        case OP_LESS:
        case OP_LESS_EQUAL:
        case OP_ADD:
        case OP_SUBTRACT:
        case OP_MULTIPLY:
        case OP_DIVIDE:
        case OP_MOD:
        case OP_POW:
        case OP_NOT:
        case OP_NEGATE:
        case OP_CLOSE_UPVALUE:
        case OP_RETURN:
        case OP_IMPORT_ALIAS:
        case OP_IMPORT_END:
        case OP_END:
        case OP_BAND:
        case OP_BOR:
        case OP_BXOR:
        case OP_BNOT:
        case OP_LSHIFT:
        case OP_RSHIFT:
        case OP_LIST:
        case OP_MAP:
        case OP_PUSH_LIST_ITEM:
        case OP_PUSH_MAP_FIELD:
            return 0;
        case OP_CONSTANT:
        case OP_GET_LOCAL:
        case OP_SET_LOCAL:
        case OP_GET_GLOBAL:
        case OP_SET_GLOBAL:
        case OP_GET_MODULE:
        case OP_SET_MODULE:
        case OP_DEFINE_GLOBAL:
        case OP_DEFINE_MODULE:
        case OP_GET_UPVALUE:
        case OP_SET_UPVALUE:
        case OP_GET_PROPERTY:
        case OP_GET_PROPERTY_NO_POP:
        case OP_SET_PROPERTY:
        case OP_GET_SUPER:
        case OP_CLASS:
        case OP_SET_CLASS_VAR:
        case OP_CALL:
        case OP_METHOD:
        case OP_EXTENSION_METHOD:
        case OP_IMPORT_STRING:
        case OP_IMPORT_NAME:
        case OP_UNPACK_LIST:
        case OP_MULTI_CASE:
            return 1;
        case OP_IMPORT_VARIABLE:
        case OP_UNPACK_REST_LIST:
        case OP_DEFINE_OPTIONAL:
        case OP_COMPARE_JUMP:
        case OP_JUMP:
        case OP_JUMP_IF_FALSE:
        case OP_JUMP_IF_NULL:
        case OP_AND:
        case OP_OR:
        case OP_LOOP:
        case OP_INVOKE:
        case OP_SUPER:
        case OP_GET_ITER:
        case OP_FOR_ITER:
            return 2;
        case OP_CLOSURE:
        {
            int constant = code[ip + 1];
            TeaOFunction* loaded_fn = AS_FUNCTION(constants.values[constant]);

            /* There is one byte for the constant, then two for each upvalue */
            return 1 + (loaded_fn->upvalue_count * 2);
        }
    }

    return 0;
}

static void begin_loop(TeaParser* parser, TeaLoop* loop)
{
    loop->start = current_chunk(parser)->count;
    loop->scope_depth = parser->scope_depth;
    loop->enclosing = parser->loop;
    parser->loop = loop;
}

static void end_loop(TeaParser* parser)
{
    if(parser->loop->end != -1)
    {
        patch_jump(parser, parser->loop->end);
        emit_op(parser, OP_POP);
    }

    int i = parser->loop->body;
    while(i < parser->function->chunk.count)
    {
        if(parser->function->chunk.code[i] == OP_END)
        {
            parser->function->chunk.code[i] = OP_JUMP;
            patch_jump(parser, i + 1);
            i += 3;
        }
        else
        {
            i += 1 + get_arg_count(parser->function->chunk.code, parser->function->chunk.constants, i);
        }
    }

    parser->loop = parser->loop->enclosing;
}

static void for_in_statement(TeaParser* parser, TeaToken var, bool constant)
{
    if(parser->local_count + 2 > 256)
    {
        error(parser, "Cannot declare more than 256 variables in one scope (Not enough space for for-loops internal variables)");
    }

    TeaToken variables[255];
    int var_count = 1;
    variables[0] = var;

    if(match(parser, TOKEN_COMMA))
    {
        do
        {
            consume(parser, TOKEN_NAME, "Expect variable name");
            variables[var_count] = parser->lex->previous;
            var_count++;
        }
        while(match(parser, TOKEN_COMMA));
    }

    consume(parser, TOKEN_IN, "Expect for iterator");

    expression(parser);
    int seq_slot = add_init_local(parser, synthetic_token("seq "), false);

    null(parser, false);
    int iter_slot = add_init_local(parser, synthetic_token("iter "), false);

    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after loop expression");

    TeaLoop loop;
    begin_loop(parser, &loop);

    /* Get the iterator index. If it's null, it means the loop is over */
    emit_op(parser, OP_GET_ITER);
    emit_bytes(parser, seq_slot, iter_slot);
    parser->loop->end = emit_jump(parser, OP_JUMP_IF_NULL);
    emit_op(parser, OP_POP);

    /* Get the iterator value */
    emit_op(parser, OP_FOR_ITER);
    emit_bytes(parser, seq_slot, iter_slot);

    begin_scope(parser);

    if(var_count > 1)
        emit_argued(parser, OP_UNPACK_LIST, var_count);

    for(int i = 0; i < var_count; i++)
    {
        declare_variable(parser, &variables[i]);
        define_variable(parser, 0, constant);
    }

    parser->loop->body = parser->function->chunk.count;
    statement(parser);

    /* Loop variable */
    end_scope(parser);

    emit_loop(parser, parser->loop->start);
    end_loop(parser);

    /* Hidden variables */
    end_scope(parser);
}

static void for_statement(TeaParser* parser)
{
    begin_scope(parser);
    consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after 'for'");

    bool constant = false;
    if(match(parser, TOKEN_VAR) || (constant = match(parser, TOKEN_CONST)))
    {
        consume(parser, TOKEN_NAME, "Expect variable name");
        TeaToken var = parser->lex->previous;

        if(check(parser, TOKEN_IN) || check(parser, TOKEN_COMMA))
        {
            /* It's a for in statement */
            for_in_statement(parser, var, constant);
            return;
        }

        uint8_t global = parse_variable_at(parser, var);

        if(match(parser, TOKEN_EQUAL))
        {
            expression(parser);
        }
        else
        {
            emit_op(parser, OP_NULL);
        }

        define_variable(parser, global, constant);
        consume(parser, TOKEN_SEMICOLON, "Expect ';' after loop variable");
    }
    else
    {
        expression_statement(parser);
        consume(parser, TOKEN_SEMICOLON, "Expect ';' after loop expression");
    }

    TeaLoop loop;
    begin_loop(parser, &loop);

    parser->loop->end = -1;

    expression(parser);
    consume(parser, TOKEN_SEMICOLON, "Expect ';' after loop condition");

    parser->loop->end = emit_jump(parser, OP_JUMP_IF_FALSE);
    emit_op(parser, OP_POP); /* Condition */

    int body_jump = emit_jump(parser, OP_JUMP);

    int increment_start = current_chunk(parser)->count;
    expression(parser);
    emit_op(parser, OP_POP);
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after for clauses");

    emit_loop(parser, parser->loop->start);
    parser->loop->start = increment_start;

    patch_jump(parser, body_jump);

    parser->loop->body = parser->function->chunk.count;
    statement(parser);

    emit_loop(parser, parser->loop->start);

    end_loop(parser);
    end_scope(parser);
}

static void break_statement(TeaParser* parser)
{
    if(parser->loop == NULL)
    {
        error(parser, "Cannot use 'break' outside of a loop");
    }

    /* Discard any locals created inside the loop */
    discard_locals(parser, parser->loop->scope_depth + 1);

    emit_jump(parser, OP_END);
}

static void continue_statement(TeaParser* parser)
{
    if(parser->loop == NULL)
    {
        error(parser, "Cannot use 'continue' outside of a loop");
    }

    /* Discard any locals created inside the loop */
    discard_locals(parser, parser->loop->scope_depth + 1);

    /* Jump to the top of the innermost loop */
    emit_loop(parser, parser->loop->start);
}

static void if_statement(TeaParser* parser)
{
    consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after 'if'");
    expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after condition");

    int else_jump = emit_jump(parser, OP_JUMP_IF_FALSE);

    emit_op(parser, OP_POP);
    statement(parser);

    int end_jump = emit_jump(parser, OP_JUMP);

    patch_jump(parser, else_jump);
    emit_op(parser, OP_POP);

    if(match(parser, TOKEN_ELSE))
        statement(parser);

    patch_jump(parser, end_jump);
}

static void switch_statement(TeaParser* parser)
{
    int case_ends[256];
    int case_count = 0;

    consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after switch");
    expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after expression");
    consume(parser, TOKEN_LEFT_BRACE, "Expect '{' before switch body");

    if(match(parser, TOKEN_CASE))
    {
        do
        {
            expression(parser);
            int multiple_cases = 0;
            if(match(parser, TOKEN_COMMA))
            {
                do
                {
                    multiple_cases++;
                    expression(parser);
                }
                while(match(parser, TOKEN_COMMA));
                emit_argued(parser, OP_MULTI_CASE, multiple_cases);
            }
            int compare_jump = emit_jump(parser, OP_COMPARE_JUMP);
            consume(parser, TOKEN_COLON, "Expect ':' after expression");
            statement(parser);
            case_ends[case_count++] = emit_jump(parser, OP_JUMP);
            patch_jump(parser, compare_jump);
            if(case_count > 255)
            {
                error_at_current(parser, "Switch statement can not have more than 256 case blocks");
            }

        }
        while(match(parser, TOKEN_CASE));
    }

    emit_op(parser, OP_POP); /* Expression */
    if(match(parser, TOKEN_DEFAULT))
    {
        consume(parser, TOKEN_COLON, "Expect ':' after default");
        statement(parser);
    }

    if(match(parser, TOKEN_CASE))
    {
        error(parser, "Unexpected case after default");
    }

    consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after switch body");

    for(int i = 0; i < case_count; i++)
    {
    	patch_jump(parser, case_ends[i]);
    }
}

static void return_statement(TeaParser* parser)
{
    if(parser->type == TYPE_SCRIPT)
    {
        error(parser, "Can't return from top-level code");
    }

    if(check(parser, TOKEN_RIGHT_BRACE) || match(parser, TOKEN_SEMICOLON))
    {
        emit_return(parser);
    }
    else
    {
        if(parser->type == TYPE_CONSTRUCTOR)
        {
            error(parser, "Can't return a value from a constructor");
        }

        expression(parser);
        emit_op(parser, OP_RETURN);
    }
}

static void import_statement(TeaParser* parser)
{
    if(match(parser, TOKEN_STRING))
    {
        int import_constant = make_constant(parser, OBJECT_VAL(tea_str_copy(parser->lex->T, parser->lex->previous.start + 1, parser->lex->previous.length - 2)));

        emit_argued(parser, OP_IMPORT_STRING, import_constant);
        emit_op(parser, OP_POP);

        if(match(parser, TOKEN_AS))
        {
            uint8_t import_name = parse_variable(parser, "Expect import alias");
            emit_op(parser, OP_IMPORT_ALIAS);
            define_variable(parser, import_name, false);
        }

        emit_op(parser, OP_IMPORT_END);

        if(match(parser, TOKEN_COMMA))
        {
            import_statement(parser);
        }
    }
    else
    {
        consume(parser, TOKEN_NAME, "Expect import identifier");
        uint8_t import_name = identifier_constant(parser, &parser->lex->previous);
        declare_variable(parser, &parser->lex->previous);

        if(match(parser, TOKEN_AS))
        {
            uint8_t import_alias = parse_variable(parser, "Expect import alias");

            emit_argued(parser, OP_IMPORT_NAME, import_name);
            define_variable(parser, import_alias, false);
        }
        else
        {
            emit_argued(parser, OP_IMPORT_NAME, import_name);
            define_variable(parser, import_name, false);
        }

        emit_op(parser, OP_IMPORT_END);

        if(match(parser, TOKEN_COMMA))
        {
            import_statement(parser);
        }
    }
}

static void from_import_statement(TeaParser* parser)
{
    if(match(parser, TOKEN_STRING))
    {
        int import_constant = make_constant(parser, OBJECT_VAL(tea_str_copy(parser->lex->T, parser->lex->previous.start + 1, parser->lex->previous.length - 2)));

        consume(parser, TOKEN_IMPORT, "Expect 'import' after import path");
        emit_argued(parser, OP_IMPORT_STRING, import_constant);
        emit_op(parser, OP_POP);
    }
    else
    {
        consume(parser, TOKEN_NAME, "Expect import identifier");
        uint8_t import_name = identifier_constant(parser, &parser->lex->previous);

        consume(parser, TOKEN_IMPORT, "Expect 'import' after identifier");

        emit_argued(parser, OP_IMPORT_NAME, import_name);
        emit_op(parser, OP_POP);
    }

    int var_count = 0;

    do
    {
        consume(parser, TOKEN_NAME, "Expect variable name");
        TeaToken var_token = parser->lex->previous;
        uint8_t var_constant = identifier_constant(parser, &var_token);

        uint8_t slot;
        if(match(parser, TOKEN_AS))
        {
            slot = parse_variable(parser, "Expect variable name");
        }
        else
        {
            slot = parse_variable_at(parser, var_token);
        }

        emit_argued(parser, OP_IMPORT_VARIABLE, var_constant);
        define_variable(parser, slot, false);

        var_count++;
        if(var_count > 255)
        {
            error(parser, "Cannot have more than 255 variables");
        }
    }
    while(match(parser, TOKEN_COMMA));

    emit_op(parser, OP_IMPORT_END);
}

static void while_statement(TeaParser* parser)
{
    TeaLoop loop;
    begin_loop(parser, &loop);

    if(!check(parser, TOKEN_LEFT_PAREN))
    {
        emit_byte(parser, OP_TRUE);
    }
    else
    {
        consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after 'while'");
        expression(parser);
        consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after condition");
    }

    /* Jump ot of the loop if the condition is false */
    parser->loop->end = emit_jump(parser, OP_JUMP_IF_FALSE);
    emit_op(parser, OP_POP);

    /* Compile the body */
    parser->loop->body = parser->function->chunk.count;
    statement(parser);

    /* Loop back to the start */
    emit_loop(parser, parser->loop->start);
    end_loop(parser);
}

static void do_statement(TeaParser* parser)
{
    TeaLoop loop;
    begin_loop(parser, &loop);

    parser->loop->body = parser->function->chunk.count;
    statement(parser);

    consume(parser, TOKEN_WHILE, "Expect while after do statement");

    if(!check(parser, TOKEN_LEFT_PAREN))
    {
        emit_op(parser, OP_TRUE);
    }
    else
    {
        consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after 'while'");
        expression(parser);
        consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after condition");
    }

    parser->loop->end = emit_jump(parser, OP_JUMP_IF_FALSE);
    emit_op(parser, OP_POP);

    emit_loop(parser, parser->loop->start);
    end_loop(parser);
}

static void multiple_assignment(TeaParser* parser)
{
    int expr_count = 0;
    int var_count = 0;
    TeaToken variables[255];

    TeaToken previous = parser->lex->previous;

    if(!match(parser, TOKEN_COMMA))
    {
        recede(parser);
        parser->lex->current = parser->lex->previous;
        expression_statement(parser);
        return;
    }

    variables[var_count] = previous;
    var_count++;

    do
    {
        consume(parser, TOKEN_NAME, "Expect variable name");
        variables[var_count] = parser->lex->previous;
        var_count++;
    }
    while(match(parser, TOKEN_COMMA));

    consume(parser, TOKEN_EQUAL, "Expect '=' multiple assignment");

    do
    {
        expression(parser);
        expr_count++;
        if(expr_count == 1 && (!check(parser, TOKEN_COMMA)))
        {
            emit_argued(parser, OP_UNPACK_LIST, var_count);
            goto finish;
        }
    }
    while(match(parser, TOKEN_COMMA));

    if(expr_count != var_count)
    {
        error(parser, "Not enough values to assign to");
    }

    finish:
    for(int i = var_count - 1; i >= 0; i--)
    {
        TeaToken token = variables[i];

        uint8_t set_op;
        int arg = resolve_local(parser, &token);
        if(arg != -1)
        {
            set_op = OP_SET_LOCAL;
        }
        else if((arg = resolve_upvalue(parser, &token)) != -1)
        {
            set_op = OP_SET_UPVALUE;
        }
        else
        {
            arg = identifier_constant(parser, &token);
            TeaOString* string = tea_str_copy(parser->lex->T, token.start, token.length);
            TeaValue value;
            if(tea_tab_get(&parser->lex->T->globals, string, &value))
            {
                set_op = OP_SET_GLOBAL;
            }
            else
            {
                set_op = OP_SET_MODULE;
            }
        }
        check_const(parser, set_op, arg);
        emit_argued(parser, set_op, (uint8_t)arg);
        emit_op(parser, OP_POP);
    }
}

static void declaration(TeaParser* parser)
{
    if(match(parser, TOKEN_CLASS))
    {
        class_declaration(parser);
    }
    else if(match(parser, TOKEN_FUNCTION))
    {
        function_declaration(parser);
    }
    else if(match(parser, TOKEN_CONST))
    {
        var_declaration(parser, true);
    }
    else if(match(parser, TOKEN_VAR))
    {
        var_declaration(parser, false);
    }
    else
    {
        statement(parser);
    }
}

static void statement(TeaParser* parser)
{
    if(check(parser, TOKEN_SEMICOLON))
    {
        next(parser);
    }
    else if(match(parser, TOKEN_FOR))
    {
        for_statement(parser);
    }
    else if(match(parser, TOKEN_IF))
    {
        if_statement(parser);
    }
    else if(match(parser, TOKEN_SWITCH))
    {
        switch_statement(parser);
    }
    else if(match(parser, TOKEN_RETURN))
    {
        return_statement(parser);
    }
    else if(match(parser, TOKEN_WHILE))
    {
        while_statement(parser);
    }
    else if(match(parser, TOKEN_DO))
    {
        do_statement(parser);
    }
    else if(match(parser, TOKEN_IMPORT))
    {
        import_statement(parser);
    }
    else if(match(parser, TOKEN_FROM))
    {
        from_import_statement(parser);
    }
    else if(match(parser, TOKEN_BREAK))
    {
        break_statement(parser);
    }
    else if(match(parser, TOKEN_CONTINUE))
    {
        continue_statement(parser);
    }
    else if(match(parser, TOKEN_NAME))
    {
        multiple_assignment(parser);
    }
    else if(match(parser, TOKEN_LEFT_BRACE))
    {
        begin_scope(parser);
        block(parser);
        end_scope(parser);
    }
    else
    {
        expression_statement(parser);
    }
}

#ifdef TEA_DEBUG_TOKENS
static void tea_lex_all(TeaLexer* lex, const char* source)
{
    int line = -1;
    while(true)
    {
        TeaToken token = tea_lex_token(lex);
        if(token.line != line)
        {
            printf("%4d ", token.line);
            line = token.line;
        }
        else
        {
            printf("   | ");
        }
        printf("%2d '%.*s'\n", token.type, token.length, token.start);

        if(token.type == TOKEN_EOF) break;
    }
    tea_lex_init(lex->T, lex, source);
}
#endif

TeaOFunction* tea_parse(TeaState* T, TeaOModule* module, const char* source)
{
    TeaLexer lexer;
    lexer.T = T;
    lexer.module = module;
    tea_lex_init(T, &lexer, source);

#ifdef TEA_DEBUG_TOKENS
    tea_lex_all(&lex, source);
#endif

    TeaParser parser;
    init_compiler(&lexer, &parser, NULL, TYPE_SCRIPT);

    next(&parser);

    while(!match(&parser, TOKEN_EOF))
    {
        declaration(&parser);
    }

    TeaOFunction* function = end_compiler(&parser);

    if(!T->repl)
    {
        tea_tab_free(T, &T->constants);
    }

    return function;
}

void tea_parser_mark_roots(TeaState* T, TeaParser* parser)
{
    tea_gc_markval(T, parser->lex->previous.value);
    tea_gc_markval(T, parser->lex->current.value);

    while(parser != NULL)
    {
        tea_gc_markobj(T, (TeaObject*)parser->function);
        parser = parser->enclosing;
    }
}