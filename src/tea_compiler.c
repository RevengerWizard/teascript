#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tea_common.h"
#include "tea_state.h"
#include "tea_compiler.h"
#include "tea_memory.h"
#include "tea_scanner.h"
#include "tea_module.h"

#ifdef TEA_DEBUG_PRINT_CODE
#include "tea_debug.h"
#endif

static TeaChunk* current_chunk(TeaCompiler* compiler)
{
    return &compiler->function->chunk;
}

static void error_at(TeaCompiler* compiler, TeaToken* token, const char* message)
{
    if(compiler->parser->panic_mode)
        return;
    compiler->parser->panic_mode = true;
    fprintf(stderr, "File %s, [line %d] Error", compiler->parser->module->name->chars, token->line);

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
    compiler->parser->had_error = true;
}

static void error(TeaCompiler* compiler, const char* message)
{
    error_at(compiler, &compiler->parser->previous, message);
}

static void error_at_current(TeaCompiler* compiler, const char* message)
{
    error_at(compiler, &compiler->parser->current, message);
}

static void advance(TeaCompiler* compiler)
{
    compiler->parser->previous = compiler->parser->current;

    while(true)
    {
        compiler->parser->current = tea_scan_token(compiler->state->scanner);
        if(compiler->parser->current.type != TOKEN_ERROR)
            break;

        error_at_current(compiler, compiler->parser->current.start);
    }
}

static void consume(TeaCompiler* compiler, TeaTokenType type, const char* message)
{
    if(compiler->parser->current.type == type)
    {
        advance(compiler);
        return;
    }

    error_at_current(compiler, message);
}

static bool check(TeaCompiler* compiler, TeaTokenType type)
{
    return compiler->parser->current.type == type;
}

static bool match(TeaCompiler* compiler, TeaTokenType type)
{
    if(!check(compiler, type))
        return false;
    advance(compiler);
    return true;
}

static void emit_byte(TeaCompiler* compiler, uint8_t byte)
{
    tea_write_chunk(compiler->state, current_chunk(compiler), byte, compiler->parser->previous.line);
}

static void emit_bytes(TeaCompiler* compiler, uint8_t byte1, uint8_t byte2)
{
    emit_byte(compiler, byte1);
    emit_byte(compiler, byte2);
}

static void emit_loop(TeaCompiler* compiler, int loop_start)
{
    emit_byte(compiler, OP_LOOP);

    int offset = current_chunk(compiler)->count - loop_start + 2;
    if(offset > UINT16_MAX)
        error(compiler, "Loop body too large.");

    emit_byte(compiler, (offset >> 8) & 0xff);
    emit_byte(compiler, offset & 0xff);
}

static int emit_jump(TeaCompiler* compiler, uint8_t instruction)
{
    emit_byte(compiler, instruction);
    emit_byte(compiler, 0xff);
    emit_byte(compiler, 0xff);

    return current_chunk(compiler)->count - 2;
}

static void emit_return(TeaCompiler* compiler)
{
    if(compiler->type == TYPE_CONSTRUCTOR)
    {
        emit_bytes(compiler, OP_GET_LOCAL, 0);
    }
    else
    {
        emit_byte(compiler, OP_NULL);
    }

    emit_byte(compiler, OP_RETURN);
}

static uint8_t make_constant(TeaCompiler* compiler, TeaValue value)
{
    int constant = tea_add_constant(compiler->state, current_chunk(compiler), value);
    if(constant > UINT8_MAX)
    {
        error(compiler, "Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

static void emit_constant(TeaCompiler* compiler, TeaValue value)
{
    emit_bytes(compiler, OP_CONSTANT, make_constant(compiler, value));
}

static void patch_jump(TeaCompiler* compiler, int offset)
{
    // -2 to adjust for the bytecode for the jump offset itself.
    int jump = current_chunk(compiler)->count - offset - 2;

    if(jump > UINT16_MAX)
    {
        error(compiler, "Too much code to jump over.");
    }

    current_chunk(compiler)->code[offset] = (jump >> 8) & 0xff;
    current_chunk(compiler)->code[offset + 1] = jump & 0xff;
}

static void init_compiler(TeaState* state, TeaParser* parser, TeaCompiler* compiler, TeaCompiler* parent, TeaFunctionType type)
{
    compiler->state = state;
    compiler->parser = parser;
    compiler->enclosing = parent;
    compiler->function = NULL;
    compiler->klass = NULL;
    compiler->loop = NULL;

    if(parent != NULL)
    {
        compiler->klass = parent->klass;
        compiler->loop = parent->loop;
    }

    compiler->type = type;
    compiler->local_count = 0;
    compiler->scope_depth = 0;

    compiler->function = tea_new_function(state, parser->module);

    if(type != TYPE_SCRIPT)
    {
        compiler->function->name = tea_copy_string(state, parser->previous.start, parser->previous.length);
    }

    TeaLocal* local = &compiler->locals[compiler->local_count++];
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

static TeaObjectFunction* end_compiler(TeaCompiler* compiler)
{
    emit_return(compiler);
    TeaObjectFunction* function = compiler->function;

#ifdef TEA_DEBUG_PRINT_CODE
    if(!compiler->parser->had_error)
    {
        tea_disassemble_chunk(current_chunk(compiler), function->name != NULL ? function->name->chars : "<script>");
    }
#endif

    if(compiler->enclosing != NULL)
    {
        emit_bytes(compiler->enclosing, OP_CLOSURE, make_constant(compiler->enclosing, OBJECT_VAL(function)));

        for(int i = 0; i < function->upvalue_count; i++)
        {
            emit_byte(compiler->enclosing, compiler->upvalues[i].is_local ? 1 : 0);
            emit_byte(compiler->enclosing, compiler->upvalues[i].index);
        }
    }

    compiler = compiler->enclosing;
    
    return function;
}

static void begin_scope(TeaCompiler* compiler)
{
    compiler->scope_depth++;
}

static void end_scope(TeaCompiler* compiler)
{
    compiler->scope_depth--;

    while(compiler->local_count > 0 && compiler->locals[compiler->local_count - 1].depth > compiler->scope_depth)
    {
        if(compiler->locals[compiler->local_count - 1].is_captured)
        {
            emit_byte(compiler, OP_CLOSE_UPVALUE);
        }
        else
        {
            emit_byte(compiler, OP_POP);
        }
        compiler->local_count--;
    }
}

static void expression(TeaCompiler* compiler);
static void expression_statement(TeaCompiler* compiler);
static void statement(TeaCompiler* compiler);
static void declaration(TeaCompiler* compiler);
static TeaParseRule* get_rule(TeaTokenType type);
static void parse_precendence(TeaCompiler* compiler, TeaPrecedence precedence);
static void anonymous(TeaCompiler* compiler, bool can_assign);

static uint8_t identifier_constant(TeaCompiler* compiler, TeaToken* name)
{
    return make_constant(compiler, OBJECT_VAL(tea_copy_string(compiler->state, name->start, name->length)));
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
                error(compiler, "Can't read local variable in its own initializer.");
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
        error(compiler, "Too many closure variables in function.");
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

static void add_local(TeaCompiler* compiler, TeaToken name)
{
    if(compiler->local_count == UINT8_COUNT)
    {
        error(compiler, "Too many local variables in function.");
        return;
    }

    TeaLocal* local = &compiler->locals[compiler->local_count++];
    local->name = name;
    local->depth = -1;
    local->is_captured = false;
}

static void declare_variable(TeaCompiler* compiler, TeaToken* name)
{
    if(compiler->scope_depth == 0)
        return;

    for(int i = compiler->local_count - 1; i >= 0; i--)
    {
        TeaLocal* local = &compiler->locals[i];
        if(local->depth != -1 && local->depth < compiler->scope_depth)
        {
            break;
        }

        /*if(identifiers_equal(name, &local->name))
        {
            error(compiler, "Already a variable with this name in this scope.");
        }*/
    }

    add_local(compiler, *name);
}

static uint8_t parse_variable(TeaCompiler* compiler, const char* error_message)
{
    consume(compiler, TOKEN_NAME, error_message);

    declare_variable(compiler, &compiler->parser->previous);
    if(compiler->scope_depth > 0)
        return 0;

    return identifier_constant(compiler, &compiler->parser->previous);
}

static uint8_t parse_variable_at(TeaCompiler* compiler, TeaToken name)
{
    declare_variable(compiler, &name);
    if(compiler->scope_depth > 0)
        return 0;

    return identifier_constant(compiler, &name);
}

static void mark_initialized(TeaCompiler* compiler)
{
    if(compiler->scope_depth == 0)
        return;

    compiler->locals[compiler->local_count - 1].depth = compiler->scope_depth;
}

static void define_variable(TeaCompiler* compiler, uint8_t global)
{
    if(compiler->scope_depth > 0)
    {
        mark_initialized(compiler);
        return;
    }

    emit_bytes(compiler, OP_DEFINE_MODULE, global);
}

static uint8_t argument_list(TeaCompiler* compiler)
{
    uint8_t arg_count = 0;
    if(!check(compiler, TOKEN_RIGHT_PAREN))
    {
        do
        {
            expression(compiler);
            if(arg_count == 255)
            {
                error(compiler, "Can't have more than 255 arguments.");
            }
            arg_count++;
        } 
        while(match(compiler, TOKEN_COMMA));
    }
    consume(compiler, TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");

    return arg_count;
}

static void and_(TeaCompiler* compiler, TeaToken previous_token, bool can_assign)
{
    int jump = emit_jump(compiler, OP_AND);
    parse_precendence(compiler, PREC_AND);
    patch_jump(compiler, jump);
}

static void binary(TeaCompiler* compiler, TeaToken previous_token, bool can_assign)
{
    TeaTokenType operator_type = compiler->parser->previous.type;
    TeaParseRule* rule = get_rule(operator_type);
    parse_precendence(compiler, (TeaPrecedence)(rule->precedence + 1));

    switch(operator_type)
    {
        case TOKEN_BANG_EQUAL:
        {
            emit_bytes(compiler, OP_EQUAL, OP_NOT);
            break;
        }
        case TOKEN_EQUAL_EQUAL:
        {
            emit_byte(compiler, OP_EQUAL);
            break;
        }
        case TOKEN_GREATER:
        {
            emit_byte(compiler, OP_GREATER);
            break;
        }
        case TOKEN_GREATER_EQUAL:
        {
            emit_byte(compiler, OP_LESS_EQUAL);
            break;
        }
        case TOKEN_LESS:
        {
            emit_byte(compiler, OP_LESS);
            break;
        }
        case TOKEN_LESS_EQUAL:
        {
            emit_byte(compiler, OP_LESS_EQUAL);
            break;
        }
        case TOKEN_PLUS:
        {
            emit_byte(compiler, OP_ADD);
            break;
        }
        case TOKEN_MINUS:
        {
            emit_byte(compiler, OP_SUBTRACT);
            break;
        }
        case TOKEN_STAR:
        {
            emit_byte(compiler, OP_MULTIPLY);
            break;
        }
        case TOKEN_SLASH:
        {
            emit_byte(compiler, OP_DIVIDE);
            break;
        }
        case TOKEN_PERCENT:
        {
            emit_byte(compiler, OP_MOD);
            break;
        }
        case TOKEN_AMPERSAND:
        {
            emit_byte(compiler, OP_BAND);
            break;
        }
        case TOKEN_PIPE:
        {
            emit_byte(compiler, OP_BOR);
            break;
        }
        case TOKEN_CARET:
        {
            emit_byte(compiler, OP_BXOR);
            break;
        }
        case TOKEN_GREATER_GREATER:
        {
            emit_byte(compiler, OP_RSHIFT);
            break;
        }
        case TOKEN_LESS_LESS:
        {
            emit_byte(compiler, OP_LSHIFT);
            break;
        }
        default: return; // Unreachable.
    }
}

static void ternary(TeaCompiler* compiler, TeaToken previous_token, bool can_assign)
{
    // Jump to else branch if the condition is false
    int else_jump = emit_jump(compiler, OP_JUMP_IF_FALSE);

    // Pop the condition
    emit_byte(compiler, OP_POP);
    expression(compiler);

    int end_jump = emit_jump(compiler, OP_JUMP);

    patch_jump(compiler, else_jump);
    emit_byte(compiler, OP_POP);

    consume(compiler, TOKEN_COLON, "Expected colon after ternary expression.");
    expression(compiler);

    patch_jump(compiler, end_jump);
}

static void call(TeaCompiler* compiler, TeaToken previous_token, bool can_assign)
{
    uint8_t arg_count = argument_list(compiler);
    emit_bytes(compiler, OP_CALL, arg_count);
}

static void dot(TeaCompiler* compiler, TeaToken previous_token, bool can_assign)
{
#define SHORT_HAND_ASSIGNMENT(op) \
    emit_bytes(compiler, OP_GET_PROPERTY_NO_POP, name); \
    expression(compiler); \
    emit_byte(compiler, op); \
    emit_bytes(compiler, OP_SET_PROPERTY, name);

#define SHORT_HAND_INCREMENT(op) \
    emit_bytes(compiler, OP_GET_PROPERTY_NO_POP, name); \
    emit_constant(compiler, NUMBER_VAL(1)); \
    emit_byte(compiler, op); \
    emit_bytes(compiler, OP_SET_PROPERTY, name);

    consume(compiler, TOKEN_NAME, "Expect property name after '.'");
    uint8_t name = identifier_constant(compiler, &compiler->parser->previous);

    if(match(compiler, TOKEN_LEFT_PAREN))
    {
        uint8_t arg_count = argument_list(compiler);
        emit_bytes(compiler, OP_INVOKE, name);
        emit_byte(compiler, arg_count);
        return;
    }

    if(can_assign && match(compiler, TOKEN_EQUAL))
    {
        expression(compiler);
        emit_bytes(compiler, OP_SET_PROPERTY, name);
    }
    else if(can_assign && match(compiler, TOKEN_PLUS_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_ADD);
    }
    else if(can_assign && match(compiler, TOKEN_MINUS_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_SUBTRACT);
    }
    else if(can_assign && match(compiler, TOKEN_STAR_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_MULTIPLY);
    }
    else if(can_assign && match(compiler, TOKEN_SLASH_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_DIVIDE);
    }
    else if(can_assign && match(compiler, TOKEN_PERCENT_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_MOD);
    }
    else if(can_assign && match(compiler, TOKEN_AMPERSAND_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_BAND);
    }
    else if(can_assign && match(compiler, TOKEN_PIPE_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_BOR);
    }
    else if(can_assign && match(compiler, TOKEN_CARET_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_BXOR);
    }
    else
    {
        if(match(compiler, TOKEN_PLUS_PLUS))
        {
            SHORT_HAND_INCREMENT(OP_ADD);
        }
        else if(match(compiler, TOKEN_MINUS_MINUS))
        {
            SHORT_HAND_INCREMENT(OP_SUBTRACT);
        }
        else
        {
            emit_bytes(compiler, OP_GET_PROPERTY, name);
        }
    }
#undef SHORT_HAND_ASSIGNMENT
#undef SHORT_HAND_INCREMENT
}

static void boolean(TeaCompiler* compiler, bool can_assign)
{
    emit_byte(compiler, compiler->parser->previous.type == TOKEN_FALSE ? OP_FALSE : OP_TRUE);
}

static void null(TeaCompiler* compiler, bool can_assign)
{
    emit_byte(compiler, OP_NULL);
}

static void grouping(TeaCompiler* compiler, bool can_assign)
{
    expression(compiler);
    consume(compiler, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void list(TeaCompiler* compiler, bool can_assign)
{
    int item_count = 0;
    if(!check(compiler, TOKEN_RIGHT_BRACKET))
    {
        do
        {
            if(check(compiler, TOKEN_RIGHT_BRACKET))
            {
                // Traling comma case
                break;
            }

            expression(compiler);

            if(item_count == UINT8_COUNT)
            {
                error(compiler, "Cannot have more than 256 items in a list literal.");
            }
            item_count++;
        }
        while(match(compiler, TOKEN_COMMA));
    }

    consume(compiler, TOKEN_RIGHT_BRACKET, "Expect ']' after list literal.");

    emit_bytes(compiler, OP_LIST, item_count);

    return;
}

static void map(TeaCompiler* compiler, bool can_assign)
{
    int item_count = 0;
    if(!check(compiler, TOKEN_RIGHT_BRACE))
    {
        do
        {
            if(check(compiler, TOKEN_RIGHT_BRACE))
            {
                // Traling comma case
                break;
            }

            consume(compiler, TOKEN_NAME, "Expected key name");
            emit_constant(compiler, OBJECT_VAL(tea_copy_string(compiler->state, compiler->parser->previous.start, compiler->parser->previous.length)));
            consume(compiler, TOKEN_EQUAL, "Expected '=' after key name.");
            expression(compiler);
            if(item_count == UINT8_COUNT)
            {
                error(compiler, "Cannot have more than 256 items in a map literal.");
            }
            item_count++;
        }
        while(match(compiler, TOKEN_COMMA));
    }

    consume(compiler, TOKEN_RIGHT_BRACE, "Expect '} after map literal.'");

    emit_bytes(compiler, OP_MAP, item_count);

    return;
}

static void subscript(TeaCompiler* compiler, TeaToken previous_token, bool can_assign)
{
#define SHORT_HAND_ASSIGNMENT(op) \
    expression(compiler); \
    emit_bytes(compiler, OP_SUBSCRIPT_PUSH, op); \
    emit_byte(compiler, OP_SUBSCRIPT_STORE);

#define SHORT_HAND_INCREMENT(op) \
    emit_constant(compiler, NUMBER_VAL(1)); \
    emit_bytes(compiler, OP_SUBSCRIPT_PUSH, op); \
    emit_byte(compiler, OP_SUBSCRIPT_STORE);

    if(match(compiler, TOKEN_COLON))
    {
        emit_byte(compiler, OP_NULL);
        expression(compiler);
        emit_byte(compiler, OP_SLICE);
        consume(compiler, TOKEN_RIGHT_BRACKET, "Expect ']' after closing.");
        return;
    }

    expression(compiler);

    if(match(compiler, TOKEN_COLON))
    {
        if(check(compiler, TOKEN_RIGHT_BRACKET))
        {
            emit_byte(compiler, OP_NULL);
        }
        else
        {
            expression(compiler);
        }
        emit_byte(compiler, OP_SLICE);
        consume(compiler, TOKEN_RIGHT_BRACKET, "Expect ']' after closing.");
        return;
    }

    consume(compiler, TOKEN_RIGHT_BRACKET, "Expect ']' after closing.");

    if(can_assign && match(compiler, TOKEN_EQUAL))
    {
        expression(compiler);
        emit_byte(compiler, OP_SUBSCRIPT_STORE);
    }
    else if(can_assign && match(compiler, TOKEN_PLUS_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_ADD);
    }
    else if(can_assign && match(compiler, TOKEN_MINUS_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_SUBTRACT);
    }
    else if(can_assign && match(compiler, TOKEN_STAR_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_MULTIPLY);
    }
    else if(can_assign && match(compiler, TOKEN_SLASH_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_DIVIDE);
    }
    else if(can_assign && match(compiler, TOKEN_PERCENT_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_MOD);
    }
    else if(can_assign && match(compiler, TOKEN_AMPERSAND_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_BAND);
    }
    else if(can_assign && match(compiler, TOKEN_PIPE_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_BOR);
    }
    else if(can_assign && match(compiler, TOKEN_CARET_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_BXOR);
    }
    else
    {
        if(match(compiler, TOKEN_PLUS_PLUS))
        {
            SHORT_HAND_INCREMENT(OP_ADD);
        }
        else if(match(compiler, TOKEN_MINUS_MINUS))
        {
            SHORT_HAND_INCREMENT(OP_SUBTRACT);
        }
        else
        {
            emit_byte(compiler, OP_SUBSCRIPT);
        }
    }

    return;
#undef SHORT_HAND_ASSIGNMENT
#undef SHORT_HAND_INCREMENT
}

static void number(TeaCompiler* compiler, bool can_assign)
{
    //double value = strtod(compiler->parser->previous.start, NULL);
    //emit_constant(compiler, NUMBER_VAL(value));
    emit_constant(compiler, compiler->parser->previous.value);
}

static void or_(TeaCompiler* compiler, TeaToken previous_token, bool can_assign)
{
    int jump = emit_jump(compiler, OP_OR);
    parse_precendence(compiler, PREC_OR);
    patch_jump(compiler, jump);
}

static void string(TeaCompiler* compiler, bool can_assign)
{
    emit_constant(compiler, compiler->parser->previous.value);
}

static void rstring(TeaCompiler* compiler, bool can_assign)
{
    if(match(compiler, TOKEN_STRING))
    {
        string(compiler, false);
        return;
    }

    consume(compiler, TOKEN_STRING, "Expected string after r keyword.");
}

static void named_variable(TeaCompiler* compiler, TeaToken name, bool can_assign)
{
#define SHORT_HAND_ASSIGNMENT(op) \
    emit_bytes(compiler, get_op, (uint8_t)arg); \
    expression(compiler); \
    emit_byte(compiler, op); \
    emit_bytes(compiler, set_op, (uint8_t)arg);

#define SHORT_HAND_INCREMENT(op) \
    emit_bytes(compiler, get_op, (uint8_t)arg); \
    emit_constant(compiler, NUMBER_VAL(1)); \
    emit_byte(compiler, op); \
    emit_bytes(compiler, set_op, (uint8_t)arg);

    uint8_t get_op, set_op;
    int arg = resolve_local(compiler, &name);
    if(arg != -1)
    {
        get_op = OP_GET_LOCAL;
        set_op = OP_SET_LOCAL;
    }
    else if((arg = resolve_upvalue(compiler, &name)) != -1)
    {
        get_op = OP_GET_UPVALUE;
        set_op = OP_SET_UPVALUE;
    }
    else
    {
        arg = identifier_constant(compiler, &name);
        //get_op = OP_GET_GLOBAL;
        //set_op = OP_SET_GLOBAL;
        TeaObjectString* string = tea_copy_string(compiler->state, name.start, name.length);
        TeaValue value;
        if(tea_table_get(&compiler->state->vm->globals, string, &value)) 
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

    if(can_assign && match(compiler, TOKEN_EQUAL))
    {
        expression(compiler);
        emit_bytes(compiler, set_op, (uint8_t)arg);
    }
    else if(can_assign && match(compiler, TOKEN_PLUS_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_ADD);
    }
    else if(can_assign && match(compiler, TOKEN_MINUS_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_SUBTRACT);
    }
    else if(can_assign && match(compiler, TOKEN_STAR_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_MULTIPLY);
    }
    else if(can_assign && match(compiler, TOKEN_SLASH_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_DIVIDE);
    }
    else if(can_assign && match(compiler, TOKEN_PERCENT_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_MOD);
    }
    else if(can_assign && match(compiler, TOKEN_AMPERSAND_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_BAND);
    }
    else if(can_assign && match(compiler, TOKEN_PIPE_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_BOR);
    }
    else if(can_assign && match(compiler, TOKEN_CARET_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_BXOR);
    }
    else
    {
        if(match(compiler, TOKEN_PLUS_PLUS))
        {
            SHORT_HAND_INCREMENT(OP_ADD);
        }
        else if(match(compiler, TOKEN_MINUS_MINUS))
        {
            SHORT_HAND_INCREMENT(OP_SUBTRACT);
        }
        else
        {
            emit_bytes(compiler, get_op, (uint8_t)arg);
        }
    }
#undef SHORT_HAND_ASSIGNMENT
#undef SHORT_HAND_INCREMENT
}

static void variable(TeaCompiler* compiler, bool can_assign)
{
    named_variable(compiler, compiler->parser->previous, can_assign);
}

static TeaToken synthetic_token(const char* text)
{
    TeaToken token;
    token.start = text;
    token.length = (int)strlen(text);

    return token;
}

static void super_(TeaCompiler* compiler, bool can_assign)
{
    if(compiler->klass == NULL)
    {
        error(compiler, "Can't use 'super' outside of a class.");
    }
    else if(!compiler->klass->has_superclass)
    {
        error(compiler, "Can't use 'super' in a class with no superclass.");
    }

    consume(compiler, TOKEN_DOT, "Expect '.' after 'super'.");
    consume(compiler, TOKEN_NAME, "Expect superclass method name.");
    uint8_t name = identifier_constant(compiler, &compiler->parser->previous);

    named_variable(compiler, synthetic_token("this"), false);

    if(match(compiler, TOKEN_LEFT_PAREN))
    {
        uint8_t arg_count = argument_list(compiler);
        named_variable(compiler, synthetic_token("super"), false);
        emit_bytes(compiler, OP_SUPER, name);
        emit_byte(compiler, arg_count);
    }
    else
    {
        named_variable(compiler, synthetic_token("super"), false);
        emit_bytes(compiler, OP_GET_SUPER, name);
    }
}

static void this_(TeaCompiler* compiler, bool can_assign)
{
    if(compiler->klass == NULL)
    {
        error(compiler, "Can't use 'this' outside of a class.");
        return;
    }

    variable(compiler, false);
}

static void unary(TeaCompiler* compiler, bool can_assign)
{
    TeaTokenType operator_type = compiler->parser->previous.type;

    parse_precendence(compiler, PREC_UNARY);

    // Emit the operator instruction.
    switch(operator_type)
    {
        case TOKEN_BANG:
        {
            emit_byte(compiler, OP_NOT);
            break;
        }
        case TOKEN_MINUS:
        {
            emit_byte(compiler, OP_NEGATE);
            break;
        }
        case TOKEN_TILDE:
        {
            emit_byte(compiler, OP_BNOT);
            break;
        }
        default:
            return; // Unreachable.
    }
}

static void range(TeaCompiler* compiler, TeaToken previous_token, bool can_assign)
{
    bool inclusive = compiler->parser->previous.type == TOKEN_DOT_DOT ? false : true;

    emit_byte(compiler, inclusive ? OP_TRUE : OP_FALSE);
    
    TeaTokenType operator_type = compiler->parser->previous.type;
    TeaParseRule* rule = get_rule(operator_type);
    parse_precendence(compiler, (TeaPrecedence)(rule->precedence + 1));

    emit_byte(compiler, OP_RANGE);
}

#define NONE                    { NULL, NULL, PREC_NONE }
#define RULE(pr, in, prec)      { pr, in, PREC_##prec }
#define INFIX(in)               { NULL, in, PREC_NONE }
#define PREFIX(pr)              { pr, NULL, PREC_NONE }
#define OPERATOR(in, prec)      { NULL, in, PREC_##prec }

static TeaParseRule rules[] = {
    RULE(grouping, call, CALL),             // TOKEN_LEFT_PAREN
    NONE,                                   // TOKEN_RIGHT_PAREN
    RULE(list, subscript, SUBSCRIPT),       // TOKEN_LEFT_BRACKET
    NONE,                                   // TOKEN_RIGHT_BRACKET
    PREFIX(map),                            // TOKEN_LEFT_BRACE
    NONE,                                   // TOKEN_RIGHT_BRACE
    NONE,                                   // TOKEN_COMMA
    OPERATOR(dot, CALL),                    // TOKEN_DOT
    NONE,                                   // TOKEN_COLON
    OPERATOR(ternary, ASSIGNMENT),          // TOKEN_QUESTION
    RULE(unary, binary, TERM),              // TOKEN_MINUS
    OPERATOR(binary, TERM),                 // TOKEN_PLUS
    OPERATOR(binary, FACTOR),               // TOKEN_SLASH
    OPERATOR(binary, FACTOR),               // TOKEN_STAR
    PREFIX(rstring),                        // TOKEN_R
    NONE,                                   // TOKEN_PLUS_PLUS
    NONE,                                   // TOKEN_MINUS_MINUS
    NONE,                                   // TOKEN_PLUS_EQUAL
    NONE,                                   // TOKEN_MINUS_EQUAL
    NONE,                                   // TOKEN_STAR_EQUAL
    NONE,                                   // TOKEN_SLASH_EQUAL
    PREFIX(unary),                          // TOKEN_BANG
    OPERATOR(binary, EQUALITY),             // TOKEN_BANG_EQUAL
    NONE,                                   // TOKEN_EQUAL
    OPERATOR(binary, EQUALITY),             // TOKEN_EQUAL_EQUAL
    OPERATOR(binary, COMPARISON),           // TOKEN_GREATER
    OPERATOR(binary, COMPARISON),           // TOKEN_GREATER_EQUAL
    OPERATOR(binary, COMPARISON),           // TOKEN_LESS
    OPERATOR(binary, COMPARISON),           // TOKEN_LESS_EQUAL
    OPERATOR(binary, FACTOR),               // TOKEN_PERCENT
    NONE,                                   // TOKEN_PERCENT_EQUAL
    OPERATOR(range, RANGE),                 // TOKEN_DOT_DOT
    OPERATOR(range, RANGE),                 // TOKEN_DOT_DOT_DOT
    OPERATOR(binary, BAND),                 // TOKEN_AMPERSAND, 
    NONE,                                   // TOKEN_AMPERSAND_EQUAL,
    OPERATOR(binary, BOR),                  // TOKEN_PIPE, 
    NONE,                                   // TOKEN_PIPE_EQUAL,
    OPERATOR(binary, BXOR),                 // TOKEN_CARET, 
    NONE,                                   // TOKEN_CARET_EQUAL,
    PREFIX(unary),                          // TOKEN_TILDE
    OPERATOR(binary, SHIFT),                // TOKEN_GREATER_GREATER,
    OPERATOR(binary, SHIFT),                // TOKEN_LESS_LESS,
    PREFIX(variable),                       // TOKEN_NAME
    PREFIX(string),                         // TOKEN_STRING
    PREFIX(number),                         // TOKEN_NUMBER
    OPERATOR(and_, AND),                    // TOKEN_AND
    NONE,                                   // TOKEN_CLASS
    NONE,                                   // TOKEN_ELSE
    PREFIX(boolean),                        // TOKEN_FALSE
    NONE,                                   // TOKEN_FOR
    PREFIX(anonymous),                      // TOKEN_FUNCTION
    NONE,                                   // TOKEN_CASE
    NONE,                                   // TOKEN_SWITCH
    NONE,                                   // TOKEN_DEFAULT
    NONE,                                   // TOKEN_IF
    PREFIX(null),                           // TOKEN_NULL
    OPERATOR(or_, OR),                      // TOKEN_OR
    NONE,                                   // TOKEN_WITH
    NONE,                                   // TOKEN_IS
    NONE,                                   // TOKEN_IMPORT
    NONE,                                   // TOKEN_FROM
    NONE,                                   // TOKEN_AS
    NONE,                                   // TOKEN_RETURN
    PREFIX(super_),                         // TOKEN_SUPER
    PREFIX(this_),                          // TOKEN_THIS
    NONE,                                   // TOKEN_CONTINUE
    NONE,                                   // TOKEN_BREAK
    NONE,                                   // TOKEN_IN
    PREFIX(boolean),                        // TOKEN_TRUE
    NONE,                                   // TOKEN_VAR
    NONE,                                   // TOKEN_CONST
    NONE,                                   // TOKEN_WHILE
    NONE,                                   // TOKEN_DO
    NONE,                                   // TOKEN_ERROR
    NONE,                                   // TOKEN_EOF
};

#undef NONE
#undef RULE
#undef INFIX
#undef PREFIX
#undef OPERATOR

static void parse_precendence(TeaCompiler* compiler, TeaPrecedence precedence)
{
    advance(compiler);
    TeaParsePrefixFn prefix_rule = get_rule(compiler->parser->previous.type)->prefix;
    if(prefix_rule == NULL)
    {
        error(compiler, "Expect expression.");
        return;
    }

    bool can_assign = precedence <= PREC_ASSIGNMENT;
    prefix_rule(compiler, can_assign);

    while(precedence <= get_rule(compiler->parser->current.type)->precedence)
    {
        TeaToken token = compiler->parser->previous;
        advance(compiler);
        TeaParseInfixFn infix_rule = get_rule(compiler->parser->previous.type)->infix;
        infix_rule(compiler, token, can_assign);
    }

    if(can_assign && match(compiler, TOKEN_EQUAL))
    {
        error(compiler, "Invalid assignment target.");
    }
}

static TeaParseRule* get_rule(TeaTokenType type)
{
    return &rules[type];
}

static void expression(TeaCompiler* compiler)
{
    parse_precendence(compiler, PREC_ASSIGNMENT);
}

static void block(TeaCompiler* compiler)
{
    while(!check(compiler, TOKEN_RIGHT_BRACE) && !check(compiler, TOKEN_EOF))
    {
        declaration(compiler);
    }

    consume(compiler, TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void begin_function(TeaCompiler* compiler, TeaCompiler* fn_compiler, TeaFunctionType type)
{
    init_compiler(compiler->state, compiler->parser, fn_compiler, compiler, type);
    begin_scope(fn_compiler);

    consume(fn_compiler, TOKEN_LEFT_PAREN, "Expect '(' after function name.");

    if(!check(fn_compiler, TOKEN_RIGHT_PAREN))
    {
        do
        {
            fn_compiler->function->arity++;
            if(fn_compiler->function->arity > 255)
            {
                error_at_current(fn_compiler, "Can't have more than 255 parameters.");
            }
            uint8_t constant = parse_variable(fn_compiler, "Expect parameter name.");
            define_variable(fn_compiler, constant);
        } 
        while(match(fn_compiler, TOKEN_COMMA));
    }

    consume(fn_compiler, TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
}

static void function(TeaCompiler* compiler, TeaFunctionType type)
{
    TeaCompiler fn_compiler;

    begin_function(compiler, &fn_compiler, type);

    if(!check(compiler, TOKEN_LEFT_BRACE))
    {
        end_compiler(&fn_compiler);
        return;
    }
    
    consume(&fn_compiler, TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block(&fn_compiler);

    end_compiler(&fn_compiler);
}

static void anonymous(TeaCompiler* compiler, bool can_assign)
{
    function(compiler, TYPE_FUNCTION);
}

static void method(TeaCompiler* compiler)
{
    consume(compiler, TOKEN_NAME, "Expect method name.");
    uint8_t constant = identifier_constant(compiler, &compiler->parser->previous);

    TeaFunctionType type = TYPE_METHOD;

    if(compiler->parser->previous.length == 11 && memcmp(compiler->parser->previous.start, "constructor", 11) == 0)
    {
        type = TYPE_CONSTRUCTOR;
    }

    function(compiler, type);
    emit_bytes(compiler, OP_METHOD, constant);
}

static void class_body(TeaCompiler* compiler)
{
    while(!check(compiler, TOKEN_RIGHT_BRACE) && !check(compiler, TOKEN_EOF))
    {
        if(match(compiler, TOKEN_VAR))
        {

        }
        else if(match(compiler, TOKEN_CONST))
        {
            
        }
        else
        {
            method(compiler);
        }
    }
}

static void class_declaration(TeaCompiler* compiler)
{
    consume(compiler, TOKEN_NAME, "Expect class name.");
    TeaToken class_name = compiler->parser->previous;
    uint8_t name_constant = identifier_constant(compiler, &compiler->parser->previous);
    declare_variable(compiler, &compiler->parser->previous);

    emit_bytes(compiler, OP_CLASS, name_constant);
    define_variable(compiler, name_constant);

    TeaClassCompiler class_compiler;
    class_compiler.has_superclass = false;
    class_compiler.enclosing = compiler->klass;
    compiler->klass = &class_compiler;

    if(match(compiler, TOKEN_COLON))
    {
        consume(compiler, TOKEN_NAME, "Expect superclass name.");
        variable(compiler, false);

        if(identifiers_equal(&class_name, &compiler->parser->previous))
        {
            error(compiler, "A class can't inherit from itself.");
        }

        begin_scope(compiler);
        add_local(compiler, synthetic_token("super"));
        define_variable(compiler, 0);

        named_variable(compiler, class_name, false);
        emit_byte(compiler, OP_INHERIT);
        class_compiler.has_superclass = true;
    }

    named_variable(compiler, class_name, false);

    if(!check(compiler, TOKEN_LEFT_BRACE))
    {
        compiler->klass = compiler->klass->enclosing;
        return;
    }

    consume(compiler, TOKEN_LEFT_BRACE, "Expect '{' before class body.");

    class_body(compiler);

    consume(compiler, TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
    emit_byte(compiler, OP_POP);

    if(class_compiler.has_superclass)
    {
        end_scope(compiler);
    }

    compiler->klass = compiler->klass->enclosing;
}

static void function_assignment(TeaCompiler* compiler)
{
    if(match(compiler, TOKEN_LEFT_BRACKET))
    {
        expression(compiler);
        consume(compiler, TOKEN_RIGHT_BRACKET, "Expect ']' after closing.");
        if(!check(compiler, TOKEN_LEFT_PAREN))
        {
            emit_byte(compiler, OP_SUBSCRIPT);
            function_assignment(compiler);
        }
        else
        {
            function(compiler, TYPE_FUNCTION);
            emit_byte(compiler, OP_SUBSCRIPT_STORE);
            emit_byte(compiler, OP_POP);
            return;
        }
    }
    else if(match(compiler, TOKEN_DOT))
    {
        consume(compiler, TOKEN_NAME, "Expect property name.");
        uint8_t dot = identifier_constant(compiler, &compiler->parser->previous);
        if(!check(compiler, TOKEN_LEFT_PAREN))
        {
            emit_bytes(compiler, OP_GET_PROPERTY, dot);
            function_assignment(compiler);
        }
        else
        {
            function(compiler, TYPE_FUNCTION);
            emit_bytes(compiler, OP_SET_PROPERTY, dot);
            emit_byte(compiler, OP_POP);
            return;
        }
    }
}

static void function_declaration(TeaCompiler* compiler)
{
    consume(compiler, TOKEN_NAME, "Expect function name.");
    
    if(check(compiler, TOKEN_LEFT_BRACKET) || check(compiler, TOKEN_DOT))
    {
        TeaToken name = compiler->parser->previous;
        named_variable(compiler, name, false);
        function_assignment(compiler);
        return;
    }

    uint8_t global = parse_variable_at(compiler, compiler->parser->previous);
    mark_initialized(compiler);
    function(compiler, TYPE_FUNCTION);
    define_variable(compiler, global);
}

static void var_declaration(TeaCompiler* compiler)
{
    uint8_t global = parse_variable(compiler, "Expect variable name.");
    mark_initialized(compiler);

    if(match(compiler, TOKEN_EQUAL))
    {
        expression(compiler);
    }
    else
    {
        emit_byte(compiler, OP_NULL);
    }

    define_variable(compiler, global);
}

static void expression_statement(TeaCompiler* compiler)
{
    expression(compiler);
    emit_byte(compiler, OP_POP);
}

static int get_arg_count(uint8_t* code, const TeaValueArray constants, int ip)
{
    switch(code[ip]) 
    {
        case OP_NULL:
        case OP_TRUE:
        case OP_FALSE:
        case OP_SUBSCRIPT:
        case OP_SUBSCRIPT_STORE:
        case OP_SUBSCRIPT_PUSH:
        case OP_SLICE:
        case OP_POP:
        case OP_EQUAL:
        case OP_GREATER:
        case OP_LESS:
        case OP_ADD:
        case OP_SUBTRACT:
        case OP_MULTIPLY:
        case OP_DIVIDE:
        case OP_MOD:
        case OP_NOT:
        case OP_NEGATE:
        case OP_CLOSE_UPVALUE:
        case OP_RETURN:
        case OP_IMPORT_VARIABLE:
        case OP_IMPORT_END:
        case OP_OPEN_CONTEXT:
        case OP_END:
        case OP_BAND:
        case OP_BXOR:
        case OP_BOR:
        case OP_LSHIFT:
        case OP_RSHIFT:
            return 0;
        case OP_CONSTANT:
        case OP_GET_LOCAL:
        case OP_SET_LOCAL:
        case OP_GET_GLOBAL:
        case OP_GET_MODULE:
        case OP_DEFINE_GLOBAL:
        case OP_DEFINE_MODULE:
        case OP_SET_MODULE:
        case OP_GET_UPVALUE:
        case OP_SET_UPVALUE:
        case OP_GET_PROPERTY:
        case OP_GET_PROPERTY_NO_POP:
        case OP_SET_PROPERTY:
        case OP_GET_SUPER:
        case OP_CALL:
        case OP_METHOD:
        case OP_IMPORT:
        case OP_LIST:
        case OP_MAP:
        case OP_CLOSE_CONTEXT:
            return 1;
        case OP_JUMP:
        case OP_JUMP_IF_FALSE:
        case OP_LOOP:
        case OP_INVOKE:
        case OP_SUPER:
        case OP_CLASS:
        case OP_IMPORT_NATIVE:
            return 2;
        case OP_IMPORT_NATIVE_VARIABLE: 
        {
            int arg_count = code[ip + 2];

            return 2 + arg_count;
        }
        case OP_CLOSURE: 
        {
            int constant = code[ip + 1];
            TeaObjectFunction* loaded_fn = AS_FUNCTION(constants.values[constant]);

            // There is one byte for the constant, then two for each upvalue.
            return 1 + (loaded_fn->upvalue_count * 2);
        }
        case OP_IMPORT_FROM: 
        {
            // 1 + amount of variables imported
            return 1 + code[ip + 1];
        }
    }

    return 0;
}

static void begin_loop(TeaCompiler* compiler, TeaLoop* loop)
{
    loop->start = current_chunk(compiler)->count;
    loop->scope_depth = compiler->scope_depth;
    loop->enclosing = compiler->loop;
    compiler->loop = loop;
}

static void end_loop(TeaCompiler* compiler)
{
    if(compiler->loop->end != -1)
    {
        patch_jump(compiler, compiler->loop->end);
        emit_byte(compiler, OP_POP);
    }

    int i = compiler->loop->body;
    while(i < compiler->function->chunk.count)
    {
        if(compiler->function->chunk.code[i] == OP_END)
        {
            compiler->function->chunk.code[i] = OP_JUMP;
            patch_jump(compiler, i + 1);
            i += 3;
        }
        else
        {
            i += 1 + get_arg_count(compiler->function->chunk.code, compiler->function->chunk.constants, i);
        }
    }

    compiler->loop = compiler->loop->enclosing;
}

static void for_statement(TeaCompiler* compiler)
{
    begin_scope(compiler);
    consume(compiler, TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");

    if(match(compiler, TOKEN_VAR))
    {
        uint8_t global = parse_variable(compiler, "Expect variable name.");

        if(match(compiler, TOKEN_EQUAL))
        {
            expression(compiler);
        }
        else
        {
            emit_byte(compiler, OP_NULL);
        }

        define_variable(compiler, global);
        consume(compiler, TOKEN_COMMA, "Expect ',' after loop variable.");
    }
    else if(match(compiler, TOKEN_COMMA))
    {
        // No initializer.
    }
    else
    {
        expression_statement(compiler);
        consume(compiler, TOKEN_COMMA, "Expect ',' after loop expression.");
    }

    TeaLoop loop;
    begin_loop(compiler, &loop);

    compiler->loop->end = -1;
    
    if(!match(compiler, TOKEN_COMMA))
    {
        expression(compiler);
        consume(compiler, TOKEN_COMMA, "Expect ',' after loop condition.");

        compiler->loop->end = emit_jump(compiler, OP_JUMP_IF_FALSE);
        emit_byte(compiler, OP_POP); // Condition.
    }

    if(!match(compiler, TOKEN_RIGHT_PAREN))
    {
        int body_jump = emit_jump(compiler, OP_JUMP);

        int increment_start = current_chunk(compiler)->count;
        expression(compiler);
        emit_byte(compiler, OP_POP);
        consume(compiler, TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        emit_loop(compiler, compiler->loop->start);
        compiler->loop->start = increment_start;
        
        patch_jump(compiler, body_jump);
    }

    compiler->loop->body = compiler->function->chunk.count;
    statement(compiler);

    emit_loop(compiler, compiler->loop->start);

    end_loop(compiler);
    end_scope(compiler);
}

static void break_statement(TeaCompiler* compiler)
{
    if(compiler->loop == NULL)
    {
        error(compiler, "Cannot use 'break' outside of a loop.");
        return;
    }

    // Discard any locals created inside the loop
    for(int i = compiler->local_count - 1; i >= 0 && compiler->locals[i].depth > compiler->loop->scope_depth; i--)
    {
        emit_byte(compiler, OP_POP);
    }

    emit_jump(compiler, OP_END);
}

static void continue_statement(TeaCompiler* compiler)
{
    if(compiler->loop == NULL)
    {
        error(compiler, "Cannot use 'continue' outside of a loop.");
        return;
    }

    // Discard any locals created inside the loop
    for(int i = compiler->local_count - 1; i >= 0 && compiler->locals[i].depth > compiler->loop->scope_depth; i--)
    {
        emit_byte(compiler, OP_POP);
    }

    // Jump to the top of the innermost loop
    emit_loop(compiler, compiler->loop->start);
}

static void if_statement(TeaCompiler* compiler)
{
    consume(compiler, TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression(compiler);
    consume(compiler, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int else_jump = emit_jump(compiler, OP_JUMP_IF_FALSE);

    emit_byte(compiler, OP_POP);
    statement(compiler);

    int end_jump = emit_jump(compiler, OP_JUMP);

    patch_jump(compiler, else_jump);
    emit_byte(compiler, OP_POP);

    if(match(compiler, TOKEN_ELSE))
        statement(compiler);

    patch_jump(compiler, end_jump);
}

static void switch_statement(TeaCompiler* compiler)
{
    consume(compiler, TOKEN_LEFT_PAREN, "Expect '(' after 'switch'.");
    expression(compiler);
    consume(compiler, TOKEN_RIGHT_PAREN, "Expect ')' after value.");
    consume(compiler, TOKEN_LEFT_BRACE, "Expect '{' before switch cases.");

    int state = 0; // 0: before all cases, 1: before default, 2: after default.
    int case_ends[256];
    int case_count = 0;
    int previous_case_skip = -1;

    while(!match(compiler, TOKEN_RIGHT_BRACE) && !check(compiler, TOKEN_EOF)) 
    {
        if(match(compiler, TOKEN_CASE) || match(compiler, TOKEN_DEFAULT)) 
        {
            TeaTokenType case_type = compiler->parser->previous.type;

            if(state == 2) 
            {
                error(compiler, "Can't have another case or default after the default case.");
            }

            if(state == 1) 
            {
                // At the end of the previous case, jump over the others.
                case_ends[case_count++] = emit_jump(compiler, OP_JUMP);

                // Patch its condition to jump to the next case (this one).
                patch_jump(compiler, previous_case_skip);
                emit_byte(compiler, OP_POP);
            }

            if(case_type == TOKEN_CASE) 
            {
                state = 1;

                // See if the case is equal to the value.
                emit_byte(compiler, OP_DUP);
                expression(compiler);

                consume(compiler, TOKEN_COLON, "Expect ':' after case value.");

                emit_byte(compiler, OP_EQUAL);
                previous_case_skip = emit_jump(compiler, OP_JUMP_IF_FALSE);

                // Pop the comparison result.
                emit_byte(compiler, OP_POP);
            } 
            else 
            {
                state = 2;
                consume(compiler, TOKEN_COLON, "Expect ':' after default.");
                previous_case_skip = -1;
            }
        } 
        else 
        {
            // Otherwise, it's a statement inside the current case.
            if(state == 0) 
            {
                error(compiler, "Can't have statements before any case.");
            }
            emit_byte(compiler, OP_POP);
            statement(compiler);
        }
    }

    // If we ended without a default case, patch its condition jump.
    if(state == 1) 
    {
        patch_jump(compiler, previous_case_skip);
        emit_byte(compiler, OP_POP);
    }

    // Patch all the case jumps to the end.
    for(int i = 0; i < case_count; i++) 
    {
        patch_jump(compiler, case_ends[i]);
    }

    emit_byte(compiler, OP_POP); // The switch value.
}

static void with_statement(TeaCompiler* compiler)
{
    compiler->with_block = true;
    consume(compiler, TOKEN_LEFT_PAREN, "Expect '(' after with statement.");
    expression(compiler);
    begin_scope(compiler);
    
    int file_index = compiler->local_count;
    TeaLocal* local = &compiler->locals[compiler->local_count++];
    local->depth = compiler->scope_depth;
    local->is_captured = false;

    consume(compiler, TOKEN_AS, "Expect alias.");
    consume(compiler, TOKEN_NAME, "Expect file alias.");
    local->name = compiler->parser->previous;
    compiler->with_file = local->name;

    consume(compiler, TOKEN_RIGHT_PAREN, "Expect ')' after with statement.");
    consume(compiler, TOKEN_LEFT_BRACE, "Expect '{' before with body.");

    emit_byte(compiler, OP_OPEN_CONTEXT);
    block(compiler);
    emit_bytes(compiler, OP_CLOSE_CONTEXT, file_index);
    end_scope(compiler);
    compiler->with_block = false;
}

static void check_close_handle(TeaCompiler* compiler)
{
    if(compiler->with_block)
    {
        TeaToken token = compiler->with_file;
        int local = resolve_local(compiler, &token);

        if(local != -1)
        {
            emit_bytes(compiler, OP_CLOSE_CONTEXT, local);
        }
        compiler->with_block = false;
    }
}

static void return_statement(TeaCompiler* compiler)
{
    if(compiler->type == TYPE_SCRIPT)
    {
        error(compiler, "Can't return from top-level code.");
    }

    if(check(compiler, TOKEN_RIGHT_BRACE))
    {
        check_close_handle(compiler);
        emit_return(compiler);
    }
    else
    {
        if(compiler->type == TYPE_CONSTRUCTOR)
        {
            error(compiler, "Can't return a value from a constructor.");
        }

        expression(compiler);

        check_close_handle(compiler);
        emit_byte(compiler, OP_RETURN);
    }
}

static void import_statement(TeaCompiler* compiler)
{
    if(match(compiler, TOKEN_STRING))
    {
        int import_constant = make_constant(compiler, OBJECT_VAL(tea_copy_string(compiler->state, compiler->parser->previous.start + 1, compiler->parser->previous.length - 2)));

        emit_bytes(compiler, OP_IMPORT, import_constant);
        emit_byte(compiler, OP_POP);

        if(match(compiler, TOKEN_AS)) 
        {
            uint8_t import_name = parse_variable(compiler, "Expect import alias.");
            emit_byte(compiler, OP_IMPORT_VARIABLE);
            define_variable(compiler, import_name);
        }

        emit_byte(compiler, OP_IMPORT_END);

        if(match(compiler, TOKEN_COMMA))
        {
            import_statement(compiler);
        }
    }
    else
    {
        consume(compiler, TOKEN_NAME, "Expect import identifier.");
        uint8_t import_name = identifier_constant(compiler, &compiler->parser->previous);
        declare_variable(compiler, &compiler->parser->previous);

        int index = tea_find_native_module((char*)compiler->parser->previous.start, compiler->parser->previous.length);

        if(index == -1) 
        {
            error(compiler, "Unknown module");
        }

        if(match(compiler, TOKEN_AS)) 
        {
            uint8_t import_alias = parse_variable(compiler, "Expect import alias.");
            emit_bytes(compiler, OP_IMPORT_NATIVE, index);
            emit_byte(compiler, import_alias);
            define_variable(compiler, import_alias);
        }
        else
        {
            emit_bytes(compiler, OP_IMPORT_NATIVE, index);
            emit_byte(compiler, import_name);

            define_variable(compiler, import_name);
        }

        if(match(compiler, TOKEN_COMMA))
        {
            import_statement(compiler);
        }
    }
}

static void from_import_statement(TeaCompiler* compiler)
{
    if(match(compiler, TOKEN_STRING))
    {
        int import_constant = make_constant(compiler, OBJECT_VAL(tea_copy_string(compiler->state, compiler->parser->previous.start + 1, compiler->parser->previous.length - 2)));

        consume(compiler, TOKEN_IMPORT, "Expect 'import' after import path.");
        emit_bytes(compiler, OP_IMPORT, import_constant);
        emit_byte(compiler, OP_POP);

        uint8_t variables[255];
        TeaToken tokens[255];
        int var_count = 0;

        do 
        {
            consume(compiler, TOKEN_NAME, "Expect variable name.");
            tokens[var_count] = compiler->parser->previous;
            variables[var_count] = identifier_constant(compiler, &compiler->parser->previous);
            var_count++;

            if(var_count > 255) 
            {
                error(compiler, "Cannot have more than 255 variables.");
            }
        } 
        while(match(compiler, TOKEN_COMMA));

        emit_bytes(compiler, OP_IMPORT_FROM, var_count);

        for(int i = 0; i < var_count; i++) 
        {
            emit_byte(compiler, variables[i]);
        }

        // This needs to be two separate loops as we need
        // all the variables popped before defining.
        if(compiler->scope_depth == 0) 
        {
            for(int i = var_count - 1; i >= 0; i--) 
            {
                define_variable(compiler, variables[i]);
            }
        } 
        else 
        {
            for(int i = 0; i < var_count; i++) 
            {
                declare_variable(compiler, &tokens[i]);
                define_variable(compiler, 0);
            }
        }

        emit_byte(compiler, OP_IMPORT_END);
    }
    else
    {
        consume(compiler, TOKEN_NAME, "Expect import identifier.");
        uint8_t import_name = identifier_constant(compiler, &compiler->parser->previous);

        int index = tea_find_native_module((char*)compiler->parser->previous.start, compiler->parser->previous.length);

        consume(compiler, TOKEN_IMPORT, "Expect 'import' after identifier");

        if(index == -1) 
        {
            error(compiler, "Unknown module");
        }

        uint8_t variables[255];
        TeaToken tokens[255];
        int var_count = 0;

        do 
        {
            consume(compiler, TOKEN_NAME, "Expect variable name.");
            tokens[var_count] = compiler->parser->previous;
            variables[var_count] = identifier_constant(compiler, &compiler->parser->previous);
            var_count++;

            if(var_count > 255) 
            {
                error(compiler, "Cannot have more than 255 variables.");
            }
        } 
        while(match(compiler, TOKEN_COMMA));

        emit_bytes(compiler, OP_IMPORT_NATIVE, index);
        emit_byte(compiler, import_name);
        emit_byte(compiler, OP_POP);

        emit_byte(compiler, OP_IMPORT_NATIVE_VARIABLE);
        emit_bytes(compiler, import_name, var_count);

        for(int i = 0; i < var_count; i++) 
        {
            emit_byte(compiler, variables[i]);
        }

        if(compiler->scope_depth == 0) 
        {
            for(int i = var_count - 1; i >= 0; i--) 
            {
                define_variable(compiler, variables[i]);
            }
        } 
        else 
        {
            for(int i = 0; i < var_count; i++) 
            {
                declare_variable(compiler, &tokens[i]);
                define_variable(compiler, 0);
            }
        }
    }
}

static void while_statement(TeaCompiler* compiler)
{
    TeaLoop loop;
    begin_loop(compiler, &loop);

    if(check(compiler, TOKEN_LEFT_BRACE))
    {
        emit_byte(compiler, OP_TRUE);
    }
    else
    {
        consume(compiler, TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
        expression(compiler);
        consume(compiler, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
    }

    // Jump ot of the loop if the condition is false
    compiler->loop->end = emit_jump(compiler, OP_JUMP_IF_FALSE);

    // Compile the body
    emit_byte(compiler, OP_POP);
    compiler->loop->body = compiler->function->chunk.count;
    statement(compiler);

    // Loop back to the start
    emit_loop(compiler, loop.start);
    end_loop(compiler);
}

static void do_statement(TeaCompiler* compiler)
{
    TeaLoop loop;
    begin_loop(compiler, &loop);

    compiler->loop->body = compiler->function->chunk.count;
    statement(compiler);

    consume(compiler, TOKEN_WHILE, "Expect while after do statement.");

    if(!check(compiler, TOKEN_LEFT_PAREN))
    {
        emit_byte(compiler, OP_TRUE);
    }
    else
    {
        consume(compiler, TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
        expression(compiler);
        consume(compiler, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
    }

    compiler->loop->end = emit_jump(compiler, OP_JUMP_IF_FALSE);

    emit_byte(compiler, OP_POP);

    emit_loop(compiler, loop.start);
    end_loop(compiler);
}

static void synchronize(TeaCompiler* compiler)
{
    compiler->parser->panic_mode = false;

    while(compiler->parser->current.type != TOKEN_EOF)
    {
        switch(compiler->parser->current.type)
        {
            case TOKEN_CLASS:
            case TOKEN_FUNCTION:
            case TOKEN_VAR:
            case TOKEN_CONST:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_DO:
            case TOKEN_WHILE:
            case TOKEN_BREAK:
            case TOKEN_RETURN:
            case TOKEN_IMPORT:
            case TOKEN_FROM:
            case TOKEN_WITH:
                return;

            default:; // Do nothing.
        }

        advance(compiler);
    }
}

static void declaration(TeaCompiler* compiler)
{
    if(match(compiler, TOKEN_CLASS))
    {
        class_declaration(compiler);
    }
    else if(match(compiler, TOKEN_FUNCTION))
    {
        function_declaration(compiler);
    }
    else if(match(compiler, TOKEN_VAR))
    {
        var_declaration(compiler);
    }
    else
    {
        statement(compiler);
    }

    if(compiler->parser->panic_mode)
        synchronize(compiler);
}

static void statement(TeaCompiler* compiler)
{
    if(match(compiler, TOKEN_FOR))
    {
        for_statement(compiler);
    }
    else if(match(compiler, TOKEN_IF))
    {
        if_statement(compiler);
    }
    else if(match(compiler, TOKEN_SWITCH))
    {
        switch_statement(compiler);
    }
    else if(match(compiler, TOKEN_RETURN))
    {
        return_statement(compiler);
    }
    else if(match(compiler, TOKEN_WITH))
    {
        with_statement(compiler);
    }
    else if(match(compiler, TOKEN_WHILE))
    {
        while_statement(compiler);
    }
    else if(match(compiler, TOKEN_DO))
    {
        do_statement(compiler);
    }
    else if(match(compiler, TOKEN_IMPORT))
    {
        import_statement(compiler);
    }
    else if(match(compiler, TOKEN_FROM))
    {
        from_import_statement(compiler);
    }
    else if(match(compiler, TOKEN_BREAK))
    {
        break_statement(compiler);
    }
    else if(match(compiler, TOKEN_CONTINUE))
    {
        continue_statement(compiler);
    }
    else if(match(compiler, TOKEN_LEFT_BRACE))
    {
        begin_scope(compiler);
        block(compiler);
        end_scope(compiler);
    }
    else
    {
        expression_statement(compiler);
    }
}

TeaObjectFunction* tea_compile(TeaState* state, TeaObjectModule* module, const char* source)
{
    TeaCompiler* compiler = state->compiler;

    TeaParser parser;
    parser.had_error = false;
    parser.panic_mode = false;
    parser.module = module;

    tea_init_scanner(state, state->scanner, source);

    init_compiler(state, &parser, compiler, NULL, TYPE_SCRIPT);

    advance(compiler);

    while(!match(compiler, TOKEN_EOF))
    {
        declaration(compiler);
    }

    TeaObjectFunction* function = end_compiler(compiler);

    return compiler->parser->had_error ? NULL : function;
}

void tea_mark_compiler_roots(TeaState* state)
{
    TeaCompiler* compiler = state->compiler;

    tea_mark_value(state->vm, compiler->parser->previous.value);
    tea_mark_value(state->vm, compiler->parser->current.value);

    while(compiler != NULL)
    {
        tea_mark_object(state->vm, (TeaObject*)compiler->function);
        compiler = compiler->enclosing;
    }
}