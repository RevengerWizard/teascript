/* 
** tea_compiler.c
** Teascript compiler and parser
*/ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tea_common.h"
#include "tea_state.h"
#include "tea_compiler.h"
#include "tea_memory.h"
#include "tea_scanner.h"
#include "tea_module.h"

#ifdef DEBUG_PRINT_CODE
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

static const int stack_effects[] = {
    #define OPCODE(_, effect) effect
    #include "tea_opcodes.h"
    #undef OPCODE
};

static void emit_op(TeaCompiler* compiler, TeaOpCode op)
{
    emit_byte(compiler, op);

    compiler->slot_count += stack_effects[op];
    if(compiler->slot_count > compiler->function->max_slots)
    {
        compiler->function->max_slots = compiler->slot_count;
    }
}

static void emit_ops(TeaCompiler* compiler, TeaOpCode op1, TeaOpCode op2)
{
    emit_bytes(compiler, op1, op2);

    compiler->slot_count += stack_effects[op1] + stack_effects[op2];
    if(compiler->slot_count > compiler->function->max_slots)
    {
        compiler->function->max_slots = compiler->slot_count;
    }
}

static void emit_argued(TeaCompiler* compiler, TeaOpCode op, uint8_t byte)
{
    emit_bytes(compiler, op, byte);

    compiler->slot_count += stack_effects[op];
    if(compiler->slot_count > compiler->function->max_slots)
    {
        compiler->function->max_slots = compiler->slot_count;
    }
}

static void emit_loop(TeaCompiler* compiler, int loop_start)
{
    emit_op(compiler, OP_LOOP);

    int offset = current_chunk(compiler)->count - loop_start + 2;
    if(offset > UINT16_MAX)
        error(compiler, "Loop body too large");

    emit_byte(compiler, (offset >> 8) & 0xff);
    emit_byte(compiler, offset & 0xff);
}

static int emit_jump(TeaCompiler* compiler, uint8_t instruction)
{
    emit_op(compiler, instruction);
    emit_bytes(compiler, 0xff, 0xff);

    return current_chunk(compiler)->count - 2;
}

static void emit_return(TeaCompiler* compiler)
{
    if(compiler->type == TYPE_CONSTRUCTOR)
    {
        emit_argued(compiler, OP_GET_LOCAL, 0);
    }
    else
    {
        emit_op(compiler, OP_NULL);
    }

    emit_byte(compiler, OP_RETURN);
}

static uint8_t make_constant(TeaCompiler* compiler, TeaValue value)
{
    int constant = tea_add_constant(compiler->state, current_chunk(compiler), value);
    if(constant > UINT8_MAX)
    {
        error(compiler, "Too many constants in one chunk");
        return 0;
    }

    return (uint8_t)constant;
}

static void invoke_method(TeaCompiler* compiler, int args, const char* name)
{
    emit_argued(compiler, OP_INVOKE, make_constant(compiler, OBJECT_VAL(tea_copy_string(compiler->state, name, strlen(name)))));
    emit_byte(compiler, args);
}

static void emit_constant(TeaCompiler* compiler, TeaValue value)
{
    emit_argued(compiler, OP_CONSTANT, make_constant(compiler, value));
}

static void patch_jump(TeaCompiler* compiler, int offset)
{
    // -2 to adjust for the bytecode for the jump offset itself.
    int jump = current_chunk(compiler)->count - offset - 2;

    if(jump > UINT16_MAX)
    {
        error(compiler, "Too much code to jump over");
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
    compiler->local_count = 1;
    compiler->slot_count = compiler->local_count;
    compiler->scope_depth = 0;

    compiler->function = tea_new_function(state, type, parser->module, compiler->slot_count);

    if(type != TYPE_SCRIPT)
    {
        compiler->function->name = tea_copy_string(state, parser->previous.start, parser->previous.length);
    }

    TeaLocal* local = &compiler->locals[0];
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

#ifdef DEBUG_PRINT_CODE
    if(!compiler->parser->had_error)
    {
        tea_disassemble_chunk(current_chunk(compiler), function->name != NULL ? function->name->chars : "<script>");
    }
#endif

    if(compiler->enclosing != NULL)
    {
        emit_argued(compiler->enclosing, OP_CLOSURE, make_constant(compiler->enclosing, OBJECT_VAL(function)));

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
static void arrow(TeaCompiler* compiler, TeaCompiler* fn_compiler, TeaToken name);
static void grouping(TeaCompiler* compiler, bool can_assign);

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
        TeaUpvalue* upvalue = &compiler->upvalues[i];
        if(upvalue->index == index && upvalue->is_local == is_local)
        {
            return i;
        }
    }

    if(upvalue_count == UINT8_COUNT)
    {
        error(compiler, "Too many closure variables in function");
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

static int add_local(TeaCompiler* compiler, TeaToken name)
{
    if(compiler->local_count == UINT8_COUNT)
    {
        error(compiler, "Too many local variables in function");
        return -1;
    }

    TeaLocal* local = &compiler->locals[compiler->local_count++];
    local->name = name;
    local->depth = compiler->scope_depth;
    local->is_captured = false;

    return compiler->local_count - 1;
}

static void declare_variable(TeaCompiler* compiler, TeaToken* name)
{
    if(compiler->scope_depth == 0)
        return;

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

    emit_argued(compiler, OP_DEFINE_MODULE, global);
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
                error(compiler, "Can't have more than 255 arguments");
            }
            arg_count++;
        } 
        while(match(compiler, TOKEN_COMMA));
    }
    consume(compiler, TOKEN_RIGHT_PAREN, "Expect ')' after arguments");

    return arg_count;
}

static void and_(TeaCompiler* compiler, bool can_assign)
{
    int jump = emit_jump(compiler, OP_AND);
    parse_precendence(compiler, PREC_AND);
    patch_jump(compiler, jump);
}

static void binary(TeaCompiler* compiler, bool can_assign)
{
    TeaTokenType operator_type = compiler->parser->previous.type;
    TeaParseRule* rule = get_rule(operator_type);
    parse_precendence(compiler, (TeaPrecedence)(rule->precedence + 1));

    switch(operator_type)
    {
        case TOKEN_BANG_EQUAL:
        {
            emit_ops(compiler, OP_EQUAL, OP_NOT);
            break;
        }
        case TOKEN_EQUAL_EQUAL:
        {
            emit_op(compiler, OP_EQUAL);
            break;
        }
        case TOKEN_IS:
        {
            emit_op(compiler, OP_IS);
            break;
        }
        case TOKEN_GREATER:
        {
            emit_op(compiler, OP_GREATER);
            break;
        }
        case TOKEN_GREATER_EQUAL:
        {
            emit_op(compiler, OP_GREATER_EQUAL);
            break;
        }
        case TOKEN_LESS:
        {
            emit_op(compiler, OP_LESS);
            break;
        }
        case TOKEN_LESS_EQUAL:
        {
            emit_op(compiler, OP_LESS_EQUAL);
            break;
        }
        case TOKEN_PLUS:
        {
            emit_op(compiler, OP_ADD);
            break;
        }
        case TOKEN_MINUS:
        {
            emit_op(compiler, OP_SUBTRACT);
            break;
        }
        case TOKEN_STAR:
        {
            emit_op(compiler, OP_MULTIPLY);
            break;
        }
        case TOKEN_SLASH:
        {
            emit_op(compiler, OP_DIVIDE);
            break;
        }
        case TOKEN_PERCENT:
        {
            emit_op(compiler, OP_MOD);
            break;
        }
        case TOKEN_STAR_STAR:
        {
            emit_op(compiler, OP_POW);
            break;
        }
        case TOKEN_AMPERSAND:
        {
            emit_op(compiler, OP_BAND);
            break;
        }
        case TOKEN_PIPE:
        {
            emit_op(compiler, OP_BOR);
            break;
        }
        case TOKEN_CARET:
        {
            emit_op(compiler, OP_BXOR);
            break;
        }
        case TOKEN_GREATER_GREATER:
        {
            emit_op(compiler, OP_RSHIFT);
            break;
        }
        case TOKEN_LESS_LESS:
        {
            emit_op(compiler, OP_LSHIFT);
            break;
        }
        default: return; // Unreachable.
    }
}

static void ternary(TeaCompiler* compiler, bool can_assign)
{
    // Jump to else branch if the condition is false
    int else_jump = emit_jump(compiler, OP_JUMP_IF_FALSE);

    // Pop the condition
    emit_op(compiler, OP_POP);
    expression(compiler);

    int end_jump = emit_jump(compiler, OP_JUMP);

    patch_jump(compiler, else_jump);
    emit_op(compiler, OP_POP);

    consume(compiler, TOKEN_COLON, "Expected colon after ternary expression");
    expression(compiler);

    patch_jump(compiler, end_jump);
}

static void call(TeaCompiler* compiler, bool can_assign)
{
    uint8_t arg_count = argument_list(compiler);
    emit_argued(compiler, OP_CALL, arg_count);
}

static void dot(TeaCompiler* compiler, bool can_assign)
{
#define SHORT_HAND_ASSIGNMENT(op) \
    emit_argued(compiler, OP_GET_PROPERTY_NO_POP, name); \
    expression(compiler); \
    emit_op(compiler, op); \
    emit_argued(compiler, OP_SET_PROPERTY, name);

#define SHORT_HAND_INCREMENT(op) \
    emit_argued(compiler, OP_GET_PROPERTY_NO_POP, name); \
    emit_constant(compiler, NUMBER_VAL(1)); \
    emit_op(compiler, op); \
    emit_argued(compiler, OP_SET_PROPERTY, name);

    consume(compiler, TOKEN_NAME, "Expect property name after '.'");
    uint8_t name = identifier_constant(compiler, &compiler->parser->previous);

    if(match(compiler, TOKEN_LEFT_PAREN))
    {
        uint8_t arg_count = argument_list(compiler);
        emit_argued(compiler, OP_INVOKE, name);
        emit_byte(compiler, arg_count);
        return;
    }

    if(can_assign && match(compiler, TOKEN_EQUAL))
    {
        expression(compiler);
        emit_argued(compiler, OP_SET_PROPERTY, name);
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
    else if(can_assign && match(compiler, TOKEN_STAR_STAR_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_POW);
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
            emit_argued(compiler, OP_GET_PROPERTY, name);
        }
    }
#undef SHORT_HAND_ASSIGNMENT
#undef SHORT_HAND_INCREMENT
}

static void boolean(TeaCompiler* compiler, bool can_assign)
{
    emit_op(compiler, compiler->parser->previous.type == TOKEN_FALSE ? OP_FALSE : OP_TRUE);
}

static void null(TeaCompiler* compiler, bool can_assign)
{
    emit_op(compiler, OP_NULL);
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
                error(compiler, "Cannot have more than 256 items in a list literal");
            }
            item_count++;
        }
        while(match(compiler, TOKEN_COMMA));
    }

    consume(compiler, TOKEN_RIGHT_BRACKET, "Expect ']' after list literal");

    emit_argued(compiler, OP_LIST, item_count);

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

            if(match(compiler, TOKEN_LEFT_BRACKET))
            {
                expression(compiler);
                consume(compiler, TOKEN_RIGHT_BRACKET, "Expect ']' after key expression");
                consume(compiler, TOKEN_EQUAL, "Expected '=' after key expression");
                expression(compiler);
            }
            else if(match(compiler, TOKEN_NAME))
            {
                emit_constant(compiler, OBJECT_VAL(tea_copy_string(compiler->state, compiler->parser->previous.start, compiler->parser->previous.length)));
                consume(compiler, TOKEN_EQUAL, "Expected '=' after key name");
                expression(compiler);
            }

            if(item_count == UINT8_COUNT)
            {
                error(compiler, "Cannot have more than 256 items in a map literal");
            }
            item_count++;
        }
        while(match(compiler, TOKEN_COMMA));
    }

    consume(compiler, TOKEN_RIGHT_BRACE, "Expect '} after map literal'");

    emit_argued(compiler, OP_MAP, item_count);

    return;
}

static void subscript(TeaCompiler* compiler, bool can_assign)
{
#define SHORT_HAND_ASSIGNMENT(op) \
    expression(compiler); \
    emit_ops(compiler, OP_SUBSCRIPT_PUSH, op); \
    emit_op(compiler, OP_SUBSCRIPT_STORE);

#define SHORT_HAND_INCREMENT(op) \
    emit_constant(compiler, NUMBER_VAL(1)); \
    emit_ops(compiler, OP_SUBSCRIPT_PUSH, op); \
    emit_op(compiler, OP_SUBSCRIPT_STORE);

    expression(compiler);

    consume(compiler, TOKEN_RIGHT_BRACKET, "Expect ']' after closing");

    if(can_assign && match(compiler, TOKEN_EQUAL))
    {
        expression(compiler);
        emit_op(compiler, OP_SUBSCRIPT_STORE);
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
    else if(can_assign && match(compiler, TOKEN_STAR_STAR_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_POW);
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
            emit_op(compiler, OP_SUBSCRIPT);
        }
    }

    return;
#undef SHORT_HAND_ASSIGNMENT
#undef SHORT_HAND_INCREMENT
}

static void or_(TeaCompiler* compiler, bool can_assign)
{
    int jump = emit_jump(compiler, OP_OR);
    parse_precendence(compiler, PREC_OR);
    patch_jump(compiler, jump);
}

static void literal(TeaCompiler* compiler, bool can_assign)
{
    emit_constant(compiler, compiler->parser->previous.value);
}

static void interpolation(TeaCompiler* compiler, bool can_assign)
{
    emit_argued(compiler, OP_LIST, 0);

    do
    {
        literal(compiler, false);
        invoke_method(compiler, 1, "add");

        expression(compiler);

        invoke_method(compiler, 1, "add");
    } 
    while(match(compiler, TOKEN_INTERPOLATION));
    
    consume(compiler, TOKEN_STRING, "Expect end of string interpolation");
    literal(compiler, false);
    invoke_method(compiler, 1, "add");

    invoke_method(compiler, 0, "join");
}

static void named_variable(TeaCompiler* compiler, TeaToken name, bool can_assign)
{
#define SHORT_HAND_ASSIGNMENT(op) \
    emit_argued(compiler, get_op, (uint8_t)arg); \
    expression(compiler); \
    emit_op(compiler, op); \
    emit_argued(compiler, set_op, (uint8_t)arg);

#define SHORT_HAND_INCREMENT(op) \
    emit_argued(compiler, get_op, (uint8_t)arg); \
    emit_constant(compiler, NUMBER_VAL(1)); \
    emit_op(compiler, op); \
    emit_argued(compiler, set_op, (uint8_t)arg);

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
        emit_argued(compiler, set_op, (uint8_t)arg);
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
    else if(can_assign && match(compiler, TOKEN_STAR_STAR_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(OP_POW);
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
            emit_argued(compiler, get_op, (uint8_t)arg);
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
        error(compiler, "Can't use 'super' outside of a class");
        return;
    }
    else if(compiler->klass->is_static)
    {
        error(compiler, "Can't use 'this' inside a static method");
        return;
    }
    else if(!compiler->klass->has_superclass)
    {
        error(compiler, "Can't use 'super' in a class with no superclass");
        return;
    }

    // constructor super
    if(match(compiler, TOKEN_LEFT_PAREN))
    {
        TeaToken token = synthetic_token("constructor");

        uint8_t name = identifier_constant(compiler, &token);
        named_variable(compiler, synthetic_token("this"), false);
        uint8_t arg_count = argument_list(compiler);
        named_variable(compiler, synthetic_token("super"), false);
        emit_argued(compiler, OP_SUPER, name);
        emit_byte(compiler, arg_count);
        return;
    }

    consume(compiler, TOKEN_DOT, "Expect '.' after 'super'");
    consume(compiler, TOKEN_NAME, "Expect superclass method name");
    uint8_t name = identifier_constant(compiler, &compiler->parser->previous);

    named_variable(compiler, synthetic_token("this"), false);

    if(match(compiler, TOKEN_LEFT_PAREN))
    {
        uint8_t arg_count = argument_list(compiler);
        named_variable(compiler, synthetic_token("super"), false);
        emit_argued(compiler, OP_SUPER, name);
        emit_byte(compiler, arg_count);
    }
    else
    {
        named_variable(compiler, synthetic_token("super"), false);
        emit_argued(compiler, OP_GET_SUPER, name);
    }
}

static void this_(TeaCompiler* compiler, bool can_assign)
{
    if(compiler->klass == NULL)
    {
        error(compiler, "Can't use 'this' outside of a class");
        return;
    }
    else if(compiler->klass->is_static)
    {
        error(compiler, "Can't use 'this' inside a static method");
        return;
    }

    variable(compiler, false);
}

static void static_(TeaCompiler* compiler, bool can_assign)
{
    if(compiler->klass == NULL)
    {
        error(compiler, "Can't use 'static' outside of a class");
    }
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
            emit_op(compiler, OP_NOT);
            break;
        }
        case TOKEN_MINUS:
        {
            emit_op(compiler, OP_NEGATE);
            break;
        }
        case TOKEN_TILDE:
        {
            emit_op(compiler, OP_BNOT);
            break;
        }
        default:
            return; // Unreachable.
    }
}

static void in_(TeaCompiler* compiler, bool can_assign)
{
    TeaTokenType operator_type = compiler->parser->previous.type;
    TeaParseRule* rule = get_rule(operator_type);
    parse_precendence(compiler, (TeaPrecedence)(rule->precedence + 1));

    emit_op(compiler, OP_IN);
}

static void range(TeaCompiler* compiler, bool can_assign)
{
    bool inclusive = compiler->parser->previous.type == TOKEN_DOT_DOT ? false : true;

    TeaTokenType operator_type = compiler->parser->previous.type;
    TeaParseRule* rule = get_rule(operator_type);
    parse_precendence(compiler, (TeaPrecedence)(rule->precedence + 1));

    emit_argued(compiler, OP_RANGE, inclusive);
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
    NONE,                                   // TOKEN_SEMICOLON
    OPERATOR(dot, CALL),                    // TOKEN_DOT
    NONE,                                   // TOKEN_COLON
    OPERATOR(ternary, ASSIGNMENT),          // TOKEN_QUESTION
    RULE(unary, binary, TERM),              // TOKEN_MINUS
    OPERATOR(binary, TERM),                 // TOKEN_PLUS
    OPERATOR(binary, FACTOR),               // TOKEN_SLASH
    OPERATOR(binary, FACTOR),               // TOKEN_STAR
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
    OPERATOR(binary, INDICES),              // TOKEN_STAR_STAR
    NONE,                                   // TOKEN_STAR_STAR_EQUAL
    OPERATOR(range, RANGE),                 // TOKEN_DOT_DOT
    OPERATOR(range, RANGE),                 // TOKEN_DOT_DOT_DOT
    OPERATOR(binary, BAND),                 // TOKEN_AMPERSAND, 
    NONE,                                   // TOKEN_AMPERSAND_EQUAL,
    OPERATOR(binary, BOR),                  // TOKEN_PIPE, 
    NONE,                                   // TOKEN_PIPE_EQUAL,
    OPERATOR(binary, BXOR),                 // TOKEN_CARET, 
    NONE,                                   // TOKEN_CARET_EQUAL,
    PREFIX(unary),                          // TOKEN_TILDE
    NONE,                                   // TOKEN_ARROW
    OPERATOR(binary, SHIFT),                // TOKEN_GREATER_GREATER,
    OPERATOR(binary, SHIFT),                // TOKEN_LESS_LESS,
    PREFIX(variable),                       // TOKEN_NAME
    PREFIX(literal),                        // TOKEN_STRING
    PREFIX(interpolation),                  // TOKEN_INTERPOLATION
    PREFIX(literal),                        // TOKEN_NUMBER
    OPERATOR(and_, AND),                    // TOKEN_AND
    NONE,                                   // TOKEN_CLASS
    PREFIX(static_),                        // TOKEN_STATIC
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
    OPERATOR(binary, IS),                   // TOKEN_IS
    NONE,                                   // TOKEN_IMPORT
    NONE,                                   // TOKEN_FROM
    NONE,                                   // TOKEN_AS
    NONE,                                   // TOKEN_RETURN
    PREFIX(super_),                         // TOKEN_SUPER
    PREFIX(this_),                          // TOKEN_THIS
    NONE,                                   // TOKEN_CONTINUE
    NONE,                                   // TOKEN_BREAK
    OPERATOR(in_, COMPARISON),              // TOKEN_IN
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
    TeaParseFn prefix_rule = get_rule(compiler->parser->previous.type)->prefix;
    if(prefix_rule == NULL)
    {
        error(compiler, "Expect expression");
        return;
    }

    bool can_assign = precedence <= PREC_ASSIGNMENT;
    prefix_rule(compiler, can_assign);

    while(precedence <= get_rule(compiler->parser->current.type)->precedence)
    {
        TeaToken token = compiler->parser->previous;
        advance(compiler);
        TeaParseFn infix_rule = get_rule(compiler->parser->previous.type)->infix;
        infix_rule(compiler, can_assign);
    }

    if(can_assign && match(compiler, TOKEN_EQUAL))
    {
        error(compiler, "Invalid assignment target");
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

    consume(compiler, TOKEN_RIGHT_BRACE, "Expect '}' after block");
}

static void begin_function(TeaCompiler* compiler, TeaCompiler* fn_compiler, TeaFunctionType type)
{
    init_compiler(compiler->state, compiler->parser, fn_compiler, compiler, type);
    begin_scope(fn_compiler);

    consume(fn_compiler, TOKEN_LEFT_PAREN, "Expect '(' after function name");

    if(!check(fn_compiler, TOKEN_RIGHT_PAREN))
    {
        do
        {
            fn_compiler->function->arity++;
            if(fn_compiler->function->arity > 255)
            {
                error_at_current(fn_compiler, "Can't have more than 255 parameters");
            }
            uint8_t constant = parse_variable(fn_compiler, "Expect parameter name");
            define_variable(fn_compiler, constant);
        } 
        while(match(fn_compiler, TOKEN_COMMA));
    }

    consume(fn_compiler, TOKEN_RIGHT_PAREN, "Expect ')' after parameters");
}

static void function(TeaCompiler* compiler, TeaFunctionType type)
{
    TeaCompiler fn_compiler;

    begin_function(compiler, &fn_compiler, type);
    statement(&fn_compiler);
    end_compiler(&fn_compiler);
}

static void grouping(TeaCompiler* compiler, bool can_assign)
{
    if(match(compiler, TOKEN_RIGHT_PAREN))
    {
        TeaCompiler fn_compiler;
        init_compiler(compiler->state, compiler->parser, &fn_compiler, compiler, TYPE_FUNCTION);
        begin_scope(&fn_compiler);
        consume(compiler, TOKEN_ARROW, "Expected arrow function");
        if(match(&fn_compiler, TOKEN_LEFT_BRACE))
        {
            block(&fn_compiler);
        }
        else
        {
            begin_scope(&fn_compiler);
            expression(&fn_compiler);
            emit_op(&fn_compiler, OP_RETURN);
            end_scope(&fn_compiler);
        }
        end_compiler(&fn_compiler);
        return;
    }

    const char* start = compiler->parser->previous.start;
	int line = compiler->parser->previous.line;
    
    if(match(compiler, TOKEN_NAME))
    {
        TeaToken name = compiler->parser->previous;
        if(match(compiler, TOKEN_COMMA))
        {
            TeaCompiler fn_compiler;
            arrow(compiler, &fn_compiler, name);
            end_compiler(&fn_compiler);
            return;
        }
        else if(match(compiler, TOKEN_RIGHT_PAREN) && match(compiler, TOKEN_ARROW))
        {
            TeaCompiler fn_compiler;
            init_compiler(compiler->state, compiler->parser, &fn_compiler, compiler, TYPE_FUNCTION);
            begin_scope(&fn_compiler);
            fn_compiler.function->arity = 1;
            uint8_t constant = parse_variable_at(&fn_compiler, name);
            define_variable(&fn_compiler, constant);
            if(match(&fn_compiler, TOKEN_LEFT_BRACE))
            {
                block(&fn_compiler);
            }
            else
            {
                begin_scope(&fn_compiler);
                expression(&fn_compiler);
                emit_op(&fn_compiler, OP_RETURN);
                end_scope(&fn_compiler);
            }
            end_compiler(&fn_compiler);
            return;
        }
        else
        {
            TeaScanner* scanner = compiler->state->scanner;

			scanner->current = start;
			scanner->line = line;

			compiler->parser->current = tea_scan_token(scanner);
			advance(compiler);
        }
    }

    expression(compiler);
    consume(compiler, TOKEN_RIGHT_PAREN, "Expect ')' after grouping expression");
}

static void arrow(TeaCompiler* compiler, TeaCompiler* fn_compiler, TeaToken name)
{
    init_compiler(compiler->state, compiler->parser, fn_compiler, compiler, TYPE_FUNCTION);
    begin_scope(fn_compiler);

    fn_compiler->function->arity = 1;
    uint8_t constant = parse_variable_at(fn_compiler, name);
    define_variable(fn_compiler, constant);
    if(!check(fn_compiler, TOKEN_RIGHT_PAREN))
    {
        do
        {
            fn_compiler->function->arity++;
            if(fn_compiler->function->arity > 255)
            {
                error_at_current(fn_compiler, "Can't have more than 255 parameters");
            }
            uint8_t constant = parse_variable(fn_compiler, "Expect parameter name");
            define_variable(fn_compiler, constant);
        } 
        while(match(fn_compiler, TOKEN_COMMA));
    }

    consume(fn_compiler, TOKEN_RIGHT_PAREN, "Expect ')' after parameters");
    consume(fn_compiler, TOKEN_ARROW, "Expect '=>' after function arguments");
    if(match(fn_compiler, TOKEN_LEFT_BRACE))
    {
        // Brace so expend function body
        block(fn_compiler);
    }
    else
    {
        // No brace so expect single expression
        begin_scope(fn_compiler);
        expression(fn_compiler);
        emit_op(fn_compiler, OP_RETURN);
        end_scope(fn_compiler);
    }
}

static void anonymous(TeaCompiler* compiler, bool can_assign)
{
    function(compiler, TYPE_FUNCTION);
}

static TeaTokenType operators[] = {
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    
    TOKEN_PERCENT,
    TOKEN_STAR_STAR,

    TOKEN_AMPERSAND,
    TOKEN_PIPE,
    //TOKEN_TILDE,
    TOKEN_CARET,
    TOKEN_LESS_LESS,
    TOKEN_GREATER_GREATER,

	TOKEN_LESS,
	TOKEN_LESS_EQUAL,
	TOKEN_GREATER,
	TOKEN_GREATER_EQUAL,
	TOKEN_EQUAL_EQUAL,

    TOKEN_LEFT_BRACKET,

    TOKEN_EOF
};

static void operator(TeaCompiler* compiler)
{
    int i = 0;
    while(operators[i] != TOKEN_EOF) 
    {
        if(match(compiler, operators[i])) 
        {
            break;
        }

        i++;
    }

    TeaObjectString* name = NULL;

    if(compiler->parser->previous.type == TOKEN_LEFT_BRACKET)
    {
        consume(compiler, TOKEN_RIGHT_BRACKET, "Expected ']' after '[' operator method");
        name = tea_copy_string(compiler->state, "[]", 2);
    } 
    else
    {
        name = tea_copy_string(compiler->state, compiler->parser->previous.start, compiler->parser->previous.length);
    }

    uint8_t constant = make_constant(compiler, OBJECT_VAL(name));

    function(compiler, TYPE_METHOD);
    emit_argued(compiler, OP_METHOD, constant);
}

static void method(TeaCompiler* compiler, TeaFunctionType type)
{
    uint8_t constant = identifier_constant(compiler, &compiler->parser->previous);

    if(compiler->parser->previous.length == 11 && memcmp(compiler->parser->previous.start, "constructor", 11) == 0)
    {
        type = TYPE_CONSTRUCTOR;
    }

    function(compiler, type);
    emit_argued(compiler, OP_METHOD, constant);
}

static void class_body(TeaCompiler* compiler)
{
    while(!check(compiler, TOKEN_RIGHT_BRACE) && !check(compiler, TOKEN_EOF))
    {
        if(match(compiler, TOKEN_VAR))
        {
            consume(compiler, TOKEN_NAME, "Expect class variable name");
            uint8_t name = identifier_constant(compiler, &compiler->parser->previous);

            if(match(compiler, TOKEN_EQUAL))
            {
                expression(compiler);
            }
            else
            {
                emit_op(compiler, OP_NULL);
            }
            emit_argued(compiler, OP_SET_CLASS_VAR, name);
        }
        else if(match(compiler, TOKEN_STATIC))
        {
            compiler->klass->is_static = true;
            consume(compiler, TOKEN_NAME, "Expect method name after 'static' keyword");
            method(compiler, TYPE_STATIC);
            compiler->klass->is_static = false;
        }
        else if(match(compiler, TOKEN_NAME))
        {
            method(compiler, TYPE_METHOD);
        }
        else
        {
            operator(compiler);
        }
    }
}

static void class_declaration(TeaCompiler* compiler)
{
    consume(compiler, TOKEN_NAME, "Expect class name");
    TeaToken class_name = compiler->parser->previous;
    uint8_t name_constant = identifier_constant(compiler, &compiler->parser->previous);
    declare_variable(compiler, &compiler->parser->previous);

    emit_argued(compiler, OP_CLASS, name_constant);
    define_variable(compiler, name_constant);

    TeaClassCompiler class_compiler;
    class_compiler.is_static = false;
    class_compiler.has_superclass = false;
    class_compiler.enclosing = compiler->klass;
    compiler->klass = &class_compiler;

    if(match(compiler, TOKEN_COLON))
    {
        consume(compiler, TOKEN_NAME, "Expect superclass name");
        variable(compiler, false);

        if(identifiers_equal(&class_name, &compiler->parser->previous))
        {
            error(compiler, "A class can't inherit from itself");
        }

        begin_scope(compiler);
        add_local(compiler, synthetic_token("super"));
        define_variable(compiler, 0);
    
        named_variable(compiler, class_name, false);
        emit_op(compiler, OP_INHERIT);
        class_compiler.has_superclass = true;
    }

    named_variable(compiler, class_name, false);

    consume(compiler, TOKEN_LEFT_BRACE, "Expect '{' before class body");

    class_body(compiler);

    consume(compiler, TOKEN_RIGHT_BRACE, "Expect '}' after class body");

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
        consume(compiler, TOKEN_RIGHT_BRACKET, "Expect ']' after closing");
        if(!check(compiler, TOKEN_LEFT_PAREN))
        {
            emit_op(compiler, OP_SUBSCRIPT);
            function_assignment(compiler);
        }
        else
        {
            function(compiler, TYPE_FUNCTION);
            emit_op(compiler, OP_SUBSCRIPT_STORE);
            emit_op(compiler, OP_POP);
            return;
        }
    }
    else if(match(compiler, TOKEN_DOT))
    {
        consume(compiler, TOKEN_NAME, "Expect property name");
        uint8_t dot = identifier_constant(compiler, &compiler->parser->previous);
        if(!check(compiler, TOKEN_LEFT_PAREN))
        {
            emit_argued(compiler, OP_GET_PROPERTY, dot);
            function_assignment(compiler);
        }
        else
        {
            function(compiler, TYPE_FUNCTION);
            emit_argued(compiler, OP_SET_PROPERTY, dot);
            emit_op(compiler, OP_POP);
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
    uint8_t global = parse_variable(compiler, "Expect variable name");
    mark_initialized(compiler);

    if(match(compiler, TOKEN_EQUAL))
    {
        expression(compiler);
    }
    else
    {
        emit_op(compiler, OP_NULL);
    }

    define_variable(compiler, global);
}

static void expression_statement(TeaCompiler* compiler)
{
    if(compiler->state->repl)
    {
        TeaToken previous = compiler->parser->previous;
        advance(compiler);
        TeaTokenType t = compiler->parser->current.type;

        for(int i = 0; i < compiler->parser->current.length; i++) 
        {
            tea_back_track(compiler->state->scanner);
        }
        compiler->parser->current = compiler->parser->previous;
        compiler->parser->previous = previous;

        expression(compiler);
        if(t != TOKEN_EQUAL) 
        {
            emit_op(compiler, OP_POP_REPL);
        }
        else
        {
            emit_op(compiler, OP_POP);
        }
        return;
    }
    
    expression(compiler);
    emit_op(compiler, OP_POP);
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
        case OP_POP:
        case OP_POP_REPL:
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
        case OP_NOT:
        case OP_NEGATE:
        case OP_CLOSE_UPVALUE:
        case OP_RETURN:
        case OP_IMPORT_VARIABLE:
        case OP_IMPORT_END:
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
            return 1;
        case OP_JUMP:
        case OP_JUMP_IF_FALSE:
        case OP_JUMP_IF_NULL:
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
        emit_op(compiler, OP_POP);
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

static void for_in_statement(TeaCompiler* compiler, TeaToken var)
{
    if(compiler->local_count + 2 > 256)
    {
        error(compiler, "Cannot declare more than 256 variables in one scope. (Not enough space for for-loops internal variables)");
        return;
    }

    expression(compiler);
    int seq_slot = add_local(compiler, synthetic_token("seq "));

    null(compiler, false);
    int iter_slot = add_local(compiler, synthetic_token("iter "));

    consume(compiler, TOKEN_RIGHT_PAREN, "Expect ')' after loop expression");

    TeaLoop loop;
    begin_loop(compiler, &loop);

    // Get the iterator index. If it's null, it means the loop is over
    emit_argued(compiler, OP_GET_LOCAL, seq_slot);
    emit_argued(compiler, OP_GET_LOCAL, iter_slot);
    invoke_method(compiler, 1, "iterate");
    emit_argued(compiler, OP_SET_LOCAL, iter_slot);
    compiler->loop->end = emit_jump(compiler, OP_JUMP_IF_NULL);
    emit_op(compiler, OP_POP);

    // Get the iterator value
    emit_argued(compiler, OP_GET_LOCAL, seq_slot);
    emit_argued(compiler, OP_GET_LOCAL, iter_slot);
    invoke_method(compiler, 1, "iteratorvalue");

    begin_scope(compiler);

    int var_slot = add_local(compiler, var);
    emit_argued(compiler, OP_SET_LOCAL, var_slot);

    compiler->loop->body = compiler->function->chunk.count;
    statement(compiler);

    // Loop variable
    end_scope(compiler);
    
    emit_loop(compiler, compiler->loop->start);
    end_loop(compiler);

    // Hidden variables
    end_scope(compiler);
}

static void for_statement(TeaCompiler* compiler)
{
    begin_scope(compiler);
    consume(compiler, TOKEN_LEFT_PAREN, "Expect '(' after 'for'");

    if(match(compiler, TOKEN_VAR))
    {
        consume(compiler, TOKEN_NAME, "Expect variable name");
        TeaToken var = compiler->parser->previous;

        if(match(compiler, TOKEN_IN))
        {
            // It's a for in statement
            for_in_statement(compiler, var);
            return;
        }

        uint8_t global = parse_variable_at(compiler, var);

        if(match(compiler, TOKEN_EQUAL))
        {
            expression(compiler);
        }
        else
        {
            emit_op(compiler, OP_NULL);
        }

        define_variable(compiler, global);
        consume(compiler, TOKEN_COMMA, "Expect ',' after loop variable");
    }
    else
    {
        expression_statement(compiler);
        consume(compiler, TOKEN_COMMA, "Expect ',' after loop expression");
    }

    TeaLoop loop;
    begin_loop(compiler, &loop);

    compiler->loop->end = -1;
    
    expression(compiler);
    consume(compiler, TOKEN_COMMA, "Expect ',' after loop condition");

    compiler->loop->end = emit_jump(compiler, OP_JUMP_IF_FALSE);
    emit_op(compiler, OP_POP); // Condition.

    int body_jump = emit_jump(compiler, OP_JUMP);

    int increment_start = current_chunk(compiler)->count;
    expression(compiler);
    emit_op(compiler, OP_POP);
    consume(compiler, TOKEN_RIGHT_PAREN, "Expect ')' after for clauses");

    emit_loop(compiler, compiler->loop->start);
    compiler->loop->start = increment_start;
    
    patch_jump(compiler, body_jump);

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
        error(compiler, "Cannot use 'break' outside of a loop");
        return;
    }

    // Discard any locals created inside the loop
    for(int i = compiler->local_count - 1; i >= 0 && compiler->locals[i].depth > compiler->loop->scope_depth; i--)
    {
        emit_op(compiler, OP_POP);
    }

    emit_jump(compiler, OP_END);
}

static void continue_statement(TeaCompiler* compiler)
{
    if(compiler->loop == NULL)
    {
        error(compiler, "Cannot use 'continue' outside of a loop");
        return;
    }

    // Discard any locals created inside the loop
    for(int i = compiler->local_count - 1; i >= 0 && compiler->locals[i].depth > compiler->loop->scope_depth; i--)
    {
        emit_op(compiler, OP_POP);
    }

    // Jump to the top of the innermost loop
    emit_loop(compiler, compiler->loop->start);
}

static void if_statement(TeaCompiler* compiler)
{
    consume(compiler, TOKEN_LEFT_PAREN, "Expect '(' after 'if'");
    expression(compiler);
    consume(compiler, TOKEN_RIGHT_PAREN, "Expect ')' after condition");

    int else_jump = emit_jump(compiler, OP_JUMP_IF_FALSE);

    emit_op(compiler, OP_POP);
    statement(compiler);

    int end_jump = emit_jump(compiler, OP_JUMP);

    patch_jump(compiler, else_jump);
    emit_op(compiler, OP_POP);

    if(match(compiler, TOKEN_ELSE))
        statement(compiler);

    patch_jump(compiler, end_jump);
}

static void switch_statement(TeaCompiler* compiler)
{
    int case_ends[256];
    int case_count = 0;

    consume(compiler, TOKEN_LEFT_PAREN, "Expect '(' after switch");
    expression(compiler);
    consume(compiler, TOKEN_RIGHT_PAREN, "Expect ')' after expression");
    consume(compiler, TOKEN_LEFT_BRACE, "Expect '{' before switch body");

    if(match(compiler, TOKEN_CASE))
    {
        do
        {
            expression(compiler);
            int multiple_cases = 0;
            if(match(compiler, TOKEN_COMMA)) 
            {
                do
                {
                    multiple_cases++;
                    expression(compiler);
                } 
                while(match(compiler, TOKEN_COMMA));
                emit_argued(compiler, OP_MULTI_CASE, multiple_cases);
            }
            int compare_jump = emit_jump(compiler, OP_COMPARE_JUMP);
            consume(compiler, TOKEN_COLON, "Expect ':' after expression");
            statement(compiler);
            case_ends[case_count++] = emit_jump(compiler, OP_JUMP);
            patch_jump(compiler, compare_jump);
            if(case_count > 255)
            {
                error_at_current(compiler, "Switch statement can not have more than 256 case blocks");
            }

        } 
        while(match(compiler, TOKEN_CASE));
    }

    if(match(compiler,TOKEN_DEFAULT))
    {
        emit_op(compiler, OP_POP); // expression.
        consume(compiler, TOKEN_COLON, "Expect ':' after default");
        statement(compiler);
    }

    if(match(compiler,TOKEN_CASE))
    {
        error(compiler, "Unexpected case after default");
    }

    consume(compiler, TOKEN_RIGHT_BRACE, "Expect '}' after switch body");

    for(int i = 0; i < case_count; i++) 
    {
    	patch_jump(compiler, case_ends[i]);
    } 
}

static void return_statement(TeaCompiler* compiler)
{
    if(compiler->type == TYPE_SCRIPT)
    {
        error(compiler, "Can't return from top-level code");
    }

    if(check(compiler, TOKEN_RIGHT_BRACE))
    {
        emit_return(compiler);
    }
    else
    {
        if(compiler->type == TYPE_CONSTRUCTOR)
        {
            error(compiler, "Can't return a value from a constructor");
        }

        expression(compiler);

        emit_op(compiler, OP_RETURN);
    }
}

static void import_statement(TeaCompiler* compiler)
{
    if(match(compiler, TOKEN_STRING))
    {
        int import_constant = make_constant(compiler, OBJECT_VAL(tea_copy_string(compiler->state, compiler->parser->previous.start + 1, compiler->parser->previous.length - 2)));

        emit_argued(compiler, OP_IMPORT, import_constant);
        emit_op(compiler, OP_POP);

        if(match(compiler, TOKEN_AS)) 
        {
            uint8_t import_name = parse_variable(compiler, "Expect import alias");
            emit_op(compiler, OP_IMPORT_VARIABLE);
            define_variable(compiler, import_name);
        }

        emit_op(compiler, OP_IMPORT_END);

        if(match(compiler, TOKEN_COMMA))
        {
            import_statement(compiler);
        }
    }
    else
    {
        consume(compiler, TOKEN_NAME, "Expect import identifier");
        uint8_t import_name = identifier_constant(compiler, &compiler->parser->previous);
        declare_variable(compiler, &compiler->parser->previous);

        int index = tea_find_native_module((char*)compiler->parser->previous.start, compiler->parser->previous.length);

        if(index == -1) 
        {
            error(compiler, "Unknown module");
        }

        if(match(compiler, TOKEN_AS)) 
        {
            uint8_t import_alias = parse_variable(compiler, "Expect import alias");
            emit_argued(compiler, OP_IMPORT_NATIVE, index);
            emit_op(compiler, import_alias);
            define_variable(compiler, import_alias);
        }
        else
        {
            emit_argued(compiler, OP_IMPORT_NATIVE, index);
            emit_op(compiler, import_name);

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

        consume(compiler, TOKEN_IMPORT, "Expect 'import' after import path");
        emit_argued(compiler, OP_IMPORT, import_constant);
        emit_op(compiler, OP_POP);

        uint8_t variables[255];
        TeaToken tokens[255];
        int var_count = 0;

        do 
        {
            consume(compiler, TOKEN_NAME, "Expect variable name");
            tokens[var_count] = compiler->parser->previous;
            variables[var_count] = identifier_constant(compiler, &compiler->parser->previous);
            var_count++;

            if(var_count > 255) 
            {
                error(compiler, "Cannot have more than 255 variables");
            }
        } 
        while(match(compiler, TOKEN_COMMA));

        emit_argued(compiler, OP_IMPORT_FROM, var_count);

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

        emit_op(compiler, OP_IMPORT_END);
    }
    else
    {
        consume(compiler, TOKEN_NAME, "Expect import identifier");
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
            consume(compiler, TOKEN_NAME, "Expect variable name");
            tokens[var_count] = compiler->parser->previous;
            variables[var_count] = identifier_constant(compiler, &compiler->parser->previous);
            var_count++;

            if(var_count > 255) 
            {
                error(compiler, "Cannot have more than 255 variables");
            }
        } 
        while(match(compiler, TOKEN_COMMA));

        emit_argued(compiler, OP_IMPORT_NATIVE, index);
        emit_byte(compiler, import_name);
        emit_op(compiler, OP_POP);

        emit_op(compiler, OP_IMPORT_NATIVE_VARIABLE);
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
        consume(compiler, TOKEN_LEFT_PAREN, "Expect '(' after 'while'");
        expression(compiler);
        consume(compiler, TOKEN_RIGHT_PAREN, "Expect ')' after condition");
    }

    // Jump ot of the loop if the condition is false
    compiler->loop->end = emit_jump(compiler, OP_JUMP_IF_FALSE);
    emit_op(compiler, OP_POP);

    // Compile the body
    compiler->loop->body = compiler->function->chunk.count;
    statement(compiler);

    // Loop back to the start
    emit_loop(compiler, compiler->loop->start);
    end_loop(compiler);
}

static void do_statement(TeaCompiler* compiler)
{
    TeaLoop loop;
    begin_loop(compiler, &loop);

    compiler->loop->body = compiler->function->chunk.count;
    statement(compiler);

    consume(compiler, TOKEN_WHILE, "Expect while after do statement");

    if(!check(compiler, TOKEN_LEFT_PAREN))
    {
        emit_op(compiler, OP_TRUE);
    }
    else
    {
        consume(compiler, TOKEN_LEFT_PAREN, "Expect '(' after 'while'");
        expression(compiler);
        consume(compiler, TOKEN_RIGHT_PAREN, "Expect ')' after condition");
    }

    compiler->loop->end = emit_jump(compiler, OP_JUMP_IF_FALSE);
    emit_op(compiler, OP_POP);

    emit_loop(compiler, compiler->loop->start);
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
            case TOKEN_STATIC:
            case TOKEN_FUNCTION:
            case TOKEN_SWITCH:
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