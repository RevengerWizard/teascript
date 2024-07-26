/*
** tea_parse.c
** Teascript parser (source code -> bytecode)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define tea_parse_c
#define TEA_CORE

#include "tea_def.h"
#include "tea_state.h"
#include "tea_parse.h"
#include "tea_func.h"
#include "tea_str.h"
#include "tea_gc.h"
#include "tea_lex.h"
#include "tea_vm.h"
#include "tea_bc.h"
#include "tea_tab.h"

#ifdef TEA_DEBUG_PRINT_CODE
#include "tea_debug.h"
#endif

/* -- Error handling --------------------------------------------- */

TEA_NORET TEA_NOINLINE static void error(Parser* parser, ErrMsg em)
{
    tea_lex_error(parser->lex, &parser->lex->prev, em);
}

TEA_NORET TEA_NOINLINE static void error_at_current(Parser* parser, ErrMsg em)
{
    tea_lex_error(parser->lex, &parser->lex->curr, em);
}

/* -- Lexer support ------------------------------------------------------- */

static Token lex_synthetic(Parser* parser, const char* text)
{
    Token token;
    GCstr* s = tea_str_newlen(parser->lex->T, text);
    setstrV(parser->lex->T, &token.value, s);
    return token;
}

static void lex_consume(Parser* parser, LexToken type)
{
    if(parser->lex->curr.type == type)
    {
        tea_lex_next(parser->lex);
        return;
    }

    const char* tok = tea_lex_token2str(parser->lex, type);
    tea_lex_error(parser->lex, &parser->lex->curr, TEA_ERR_XTOKEN, tok);
}

/* Check for matching token */
static bool lex_check(Parser* parser, LexToken type)
{
    return parser->lex->curr.type == type;
}

/* Check and consume token */
static bool lex_match(Parser* parser, LexToken type)
{
    if(!lex_check(parser, type))
        return false;
    tea_lex_next(parser->lex);
    return true;
}

/* -- Management of constants --------------------------------------------- */

static int const_add(tea_State* T, GCproto* f, TValue* value)
{
    copyTV(T, T->top++, value);

    if(f->k_size < f->k_count + 1)
    {
        f->k = tea_mem_growvec(T, TValue, f->k, f->k_size, INT_MAX);
    }
    copyTV(T, proto_kgc(f, f->k_count), value);
    f->k_count++;

    T->top--;

    return f->k_count - 1;
}

static uint8_t const_make(Parser* parser, TValue* value)
{
    int constant = const_add(parser->lex->T, parser->proto, value);
    if(constant > UINT8_MAX)
    {
        error(parser, TEA_ERR_XKCONST);
    }

    return (uint8_t)constant;
}

/* -- Bytecode emitter ---------------------------------------------------- */

static void bcemit_byte(Parser* parser, uint8_t byte)
{
    tea_State* T = parser->lex->T;
    GCproto* f = parser->proto;
    int line = parser->lex->prev.line;

    if(f->bc_size < f->bc_count + 1)
    {
        f->bc = tea_mem_growvec(T, BCIns, f->bc, f->bc_size, INT_MAX);
    }

    f->bc[f->bc_count] = byte;
    f->bc_count++;

    /* See if we're still on the same line */
    if(f->line_count > 0 && f->lines[f->line_count - 1].line == line)
    {
        return;
    }

    /* Append a new LineStart */
    if(f->line_size < f->line_count + 1)
    {
        f->lines = tea_mem_growvec(T, LineStart, f->lines, f->line_size, INT_MAX);
    }

    LineStart* line_start = &f->lines[f->line_count++];
    line_start->ofs = f->bc_count - 1;
    line_start->line = line;
}

static void bcemit_bytes(Parser* parser, uint8_t byte1, uint8_t byte2)
{
    bcemit_byte(parser, byte1);
    bcemit_byte(parser, byte2);
}

static const int stack_effects[] = {
#define BCEFFECT(_, effect) effect,
    BCDEF(BCEFFECT)
#undef BCEFFECT
};

/* Emit bytecode instruction */
static void bcemit_op(Parser* parser, BCOp op)
{
    bcemit_byte(parser, op);

    parser->slot_count += stack_effects[op];
    if(parser->slot_count > parser->proto->max_slots)
    {
        parser->proto->max_slots = parser->slot_count;
    }
}
/* Emit 2 bytecode instructions */
static void bcemit_ops(Parser* parser, BCOp op1, BCOp op2)
{
    bcemit_bytes(parser, op1, op2);

    parser->slot_count += stack_effects[op1] + stack_effects[op2];
    if(parser->slot_count > parser->proto->max_slots)
    {
        parser->proto->max_slots = parser->slot_count;
    }
}

static void bcemit_argued(Parser* parser, BCOp op, uint8_t byte)
{
    bcemit_bytes(parser, op, byte);

    parser->slot_count += stack_effects[op];
    if(parser->slot_count > parser->proto->max_slots)
    {
        parser->proto->max_slots = parser->slot_count;
    }
}

static void bcemit_loop(Parser* parser, int loop_start)
{
    bcemit_op(parser, BC_LOOP);

    int ofs = parser->proto->bc_count - loop_start + 2;
    if(ofs > UINT16_MAX)
        error(parser, TEA_ERR_XLOOP);

    bcemit_byte(parser, (ofs >> 8) & 0xff);
    bcemit_byte(parser, ofs & 0xff);
}

static int bcemit_jump(Parser* parser, uint8_t instruction)
{
    bcemit_op(parser, instruction);
    bcemit_bytes(parser, 0xff, 0xff);

    return parser->proto->bc_count - 2;
}

static void bcemit_return(Parser* parser)
{
    if(parser->type == PROTO_INIT)
    {
        bcemit_argued(parser, BC_GET_LOCAL, 0);
    }
    else
    {
        bcemit_op(parser, BC_NIL);
    }

    bcemit_op(parser, BC_RETURN);
}

static void bcemit_invoke(Parser* parser, int args, const char* name)
{
    TValue v;
    GCstr* str = tea_str_new(parser->lex->T, name, strlen(name));
    setstrV(parser->lex->T, &v, str);
    bcemit_argued(parser, BC_INVOKE, const_make(parser, &v));
    bcemit_byte(parser, args);
}

static void bcemit_constant(Parser* parser, TValue* value)
{
    bcemit_argued(parser, BC_CONSTANT, const_make(parser, value));
}

static void bcpatch_jump(Parser* parser, int ofs)
{
    /* -2 to adjust for the bytecode for the jump offset itself */
    int jump = parser->proto->bc_count - ofs - 2;

    if(jump > UINT16_MAX)
    {
        error(parser, TEA_ERR_XJUMP);
    }

    parser->proto->bc[ofs] = (jump >> 8) & 0xff;
    parser->proto->bc[ofs + 1] = jump & 0xff;
}

static void parser_init(Lexer* lexer, Parser* parser, Parser* parent, ProtoType type)
{
    parser->lex = lexer;
    parser->enclosing = parent;
    parser->proto = NULL;
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

    parser->proto = tea_func_newproto(lexer->T, parser->slot_count);

    switch(type)
    {
        case PROTO_FUNCTION:
        case PROTO_INIT:
        case PROTO_METHOD:
            parser->proto->name = strV(&parser->lex->prev.value);
            break;
        case PROTO_OPERATOR:
            parser->proto->name = parser->enclosing->name;
            break;
        case PROTO_ANONYMOUS:
            parser->proto->name = tea_str_newlit(lexer->T, "<anonymous>");
            break;
        case PROTO_SCRIPT:
            parser->proto->name = tea_str_newlit(lexer->T, "<script>");
            break;
        default:
            break;
    }

    Local* local = &parser->locals[0];
    local->depth = 0;
    local->is_captured = false;

    GCstr* s;
    switch(type)
    {
        case PROTO_SCRIPT:
        case PROTO_FUNCTION:
        case PROTO_ANONYMOUS:
            s = &lexer->T->strempty;
            setstrV(lexer->T, &local->name.value, s);
            break;
        case PROTO_INIT:
        case PROTO_METHOD:
        case PROTO_OPERATOR:
            s = tea_str_newlit(lexer->T, "this");
            setstrV(lexer->T, &local->name.value, s);
            break;
        default:
            break;
    }
}

static GCproto* parser_end(Parser* parser)
{
    GCproto* proto = parser->proto;

#ifdef TEA_DEBUG_PRINT_CODE
    tea_State* T = parser->lex->T;
    tea_debug_chunk(T, proto, str_data(proto->name));
#endif

    if(parser->enclosing != NULL)
    {
        TValue o;
        setprotoV(parser->lex->T, &o, proto);
        bcemit_argued(parser->enclosing, BC_CLOSURE, const_make(parser->enclosing, &o));

        for(int i = 0; i < proto->upvalue_count; i++)
        {
            bcemit_byte(parser->enclosing, parser->upvalues[i].is_local ? 1 : 0);
            bcemit_byte(parser->enclosing, parser->upvalues[i].index);
        }
    }

    parser->lex->T->parser = parser->enclosing;

    return proto;
}

/* -- Scope handling ------------------------------------------------------ */

/* Begin a scope */
static void scope_begin(Parser* parser)
{
    parser->scope_depth++;
}

static int discard_locals(Parser* parser, int depth)
{
    int local;
    for(local = parser->local_count - 1; local >= 0 && parser->locals[local].depth >= depth; local--)
    {
        if(parser->locals[local].is_captured)
        {
            bcemit_op(parser, BC_CLOSE_UPVALUE);
        }
        else
        {
            bcemit_op(parser, BC_POP);
        }
    }

    return parser->local_count - local - 1;
}

/* End a scope */
static void scope_end(Parser* parser)
{
    int effect = discard_locals(parser, parser->scope_depth);
    parser->local_count -= effect;
    parser->slot_count -= effect;
    parser->scope_depth--;
}

/* Forward declarations */
static void expr(Parser* parser);
static void parse_stmt(Parser* parser);
static void parse_decl(Parser* parser);
static ParseRule expr_rule(int type);
static void expr_precedence(Parser* parser, Precedence precedence);
static void expr_anonymous(Parser* parser, bool assign);
static void expr_grouping(Parser* parser, bool assign);
static void parse_block(Parser* parser);

static uint8_t identifier_constant(Parser* parser, Token* name)
{
    return const_make(parser, &name->value);
}

static bool identifiers_equal(Token* a, Token* b)
{
    return strV(&a->value) == strV(&b->value);
}

static int resolve_local(Parser* parser, Token* name)
{
    for(int i = parser->local_count - 1; i >= 0; i--)
    {
        Local* local = &parser->locals[i];
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

static int add_upvalue(Parser* parser, uint8_t index, bool is_local, bool constant)
{
    int upvalue_count = parser->proto->upvalue_count;

    for(int i = 0; i < upvalue_count; i++)
    {
        Upvalue* upvalue = &parser->upvalues[i];
        if(upvalue->index == index && upvalue->is_local == is_local)
        {
            return i;
        }
    }

    if(upvalue_count == TEA_MAX_UPVAL)
    {
        error(parser, TEA_ERR_XUPVAL);
    }

    parser->upvalues[upvalue_count].is_local = is_local;
    parser->upvalues[upvalue_count].index = index;
    parser->upvalues[upvalue_count].constant = constant;

    return parser->proto->upvalue_count++;
}

static int resolve_upvalue(Parser* parser, Token* name)
{
    if(parser->enclosing == NULL)
        return -1;

    bool constant;
    int local = resolve_local(parser->enclosing, name);
    if(local != -1)
    {
        constant = parser->enclosing->locals[local].constant;
        parser->enclosing->locals[local].is_captured = true;
        return add_upvalue(parser, (uint8_t)local, true, constant);
    }

    int upvalue = resolve_upvalue(parser->enclosing, name);
    if(upvalue != -1)
    {
        constant = parser->enclosing->upvalues[upvalue].constant;
        return add_upvalue(parser, (uint8_t)upvalue, false, constant);
    }

    return -1;
}

static void add_local(Parser* parser, Token name)
{
    if(parser->local_count == TEA_MAX_LOCAL)
    {
        error(parser, TEA_ERR_XLOCALS);
    }

    Local* local = &parser->locals[parser->local_count++];
    local->name = name;
    local->depth = -1;
    local->is_captured = false;
    local->constant = false;
}

static int add_init_local(Parser* parser, Token name)
{
    add_local(parser, name);

    Local* local = &parser->locals[parser->local_count - 1];
    local->depth = parser->scope_depth;

    return parser->local_count - 1;
}

static void declare_variable(Parser* parser, Token* name)
{
    if(parser->scope_depth == 0)
        return;

    add_local(parser, *name);
}

static uint8_t parse_variable(Parser* parser)
{
    lex_consume(parser, TK_NAME);

    declare_variable(parser, &parser->lex->prev);
    if(parser->scope_depth > 0)
        return 0;

    return identifier_constant(parser, &parser->lex->prev);
}

static uint8_t parse_variable_at(Parser* parser, Token name)
{
    declare_variable(parser, &name);
    if(parser->scope_depth > 0)
        return 0;

    return identifier_constant(parser, &name);
}

static void mark_initialized(Parser* parser, bool constant)
{
    if(parser->scope_depth == 0) return;

    parser->locals[parser->local_count - 1].depth = parser->scope_depth;
    parser->locals[parser->local_count - 1].constant = constant;
}

static void define_variable(Parser* parser, uint8_t global, bool constant)
{
    if(parser->scope_depth > 0)
    {
        mark_initialized(parser, constant);
        return;
    }

    GCstr* string = strV(proto_kgc(parser->proto, global));
    if(constant)
    {
        TValue* o = tea_tab_set(parser->lex->T, &parser->lex->T->constants, string, NULL);
        setnilV(o);
    }

    bcemit_argued(parser, BC_DEFINE_MODULE, global);
}

static uint8_t parse_arg_list(Parser* parser)
{
    uint8_t nargs = 0;
    if(!lex_check(parser, ')'))
    {
        do
        {
            expr(parser);
            if(nargs == 255)
            {
                error(parser, TEA_ERR_XARGS);
            }
            nargs++;
        }
        while(lex_match(parser, ','));
    }
    lex_consume(parser, ')');

    return nargs;
}

/* -- Expressions --------------------------------------------------------- */

static void expr_and(Parser* parser, bool assign)
{
    int jump = bcemit_jump(parser, BC_JUMP_IF_FALSE);
    bcemit_op(parser, BC_POP);
    expr_precedence(parser, PREC_AND);
    bcpatch_jump(parser, jump);
}

static void expr_binary(Parser* parser, bool assign)
{
    LexToken op_type = parser->lex->prev.type;

    if(op_type == TK_NOT)
    {
        lex_consume(parser, TK_IN);

        ParseRule rule = expr_rule(op_type);
        expr_precedence(parser, (Precedence)(rule.prec + 1));

        bcemit_ops(parser, BC_IN, BC_NOT);
        return;
    }

    if(op_type == TK_IS && lex_match(parser, TK_NOT))
    {
        ParseRule rule = expr_rule(op_type);
        expr_precedence(parser, (Precedence)(rule.prec + 1));

        bcemit_ops(parser, BC_IS, BC_NOT);
        return;
    }

    ParseRule rule = expr_rule(op_type);
    expr_precedence(parser, (Precedence)(rule.prec + 1));

    switch(op_type)
    {
        case TK_BANG_EQUAL:
        {
            bcemit_ops(parser, BC_EQUAL, BC_NOT);
            break;
        }
        case TK_EQUAL_EQUAL:
        {
            bcemit_op(parser, BC_EQUAL);
            break;
        }
        case TK_IS:
        {
            bcemit_op(parser, BC_IS);
            break;
        }
        case '>':
        {
            bcemit_op(parser, BC_GREATER);
            break;
        }
        case TK_GREATER_EQUAL:
        {
            bcemit_op(parser, BC_GREATER_EQUAL);
            break;
        }
        case '<':
        {
            bcemit_op(parser, BC_LESS);
            break;
        }
        case TK_LESS_EQUAL:
        {
            bcemit_op(parser, BC_LESS_EQUAL);
            break;
        }
        case '+':
        {
            bcemit_op(parser, BC_ADD);
            break;
        }
        case '-':
        {
            bcemit_op(parser, BC_SUBTRACT);
            break;
        }
        case '*':
        {
            bcemit_op(parser, BC_MULTIPLY);
            break;
        }
        case '/':
        {
            bcemit_op(parser, BC_DIVIDE);
            break;
        }
        case '%':
        {
            bcemit_op(parser, BC_MOD);
            break;
        }
        case TK_STAR_STAR:
        {
            bcemit_op(parser, BC_POW);
            break;
        }
        case '&':
        {
            bcemit_op(parser, BC_BAND);
            break;
        }
        case '|':
        {
            bcemit_op(parser, BC_BOR);
            break;
        }
        case '^':
        {
            bcemit_op(parser, BC_BXOR);
            break;
        }
        case TK_GREATER_GREATER:
        {
            bcemit_op(parser, BC_RSHIFT);
            break;
        }
        case TK_LESS_LESS:
        {
            bcemit_op(parser, BC_LSHIFT);
            break;
        }
        case TK_IN:
        {
            bcemit_op(parser, BC_IN);
            break;
        }
        default: return; /* Unreachable */
    }
}

static void expr_ternary(Parser* parser, bool assign)
{
    /* Jump to else branch if the condition is false */
    int else_jump = bcemit_jump(parser, BC_JUMP_IF_FALSE);

    /* Pop the condition */
    bcemit_op(parser, BC_POP);
    expr(parser);

    int end_jump = bcemit_jump(parser, BC_JUMP);

    bcpatch_jump(parser, else_jump);
    bcemit_op(parser, BC_POP);

    lex_consume(parser, ':');
    expr(parser);

    bcpatch_jump(parser, end_jump);
}

static void expr_call(Parser* parser, bool assign)
{
    uint8_t nargs = parse_arg_list(parser);
    bcemit_argued(parser, BC_CALL, nargs);
}

static void expr_dot(Parser* parser, bool assign)
{
#define SHORT_HAND_ASSIGNMENT(op) \
    bcemit_argued(parser, BC_PUSH_ATTR, name); \
    expr(parser); \
    bcemit_op(parser, op); \
    bcemit_argued(parser, BC_SET_ATTR, name);

#define SHORT_HAND_INCREMENT(op) \
    bcemit_argued(parser, BC_PUSH_ATTR, name); \
    TValue _v; \
    setnumV(&_v, 1); \
    bcemit_constant(parser, &_v); \
    bcemit_op(parser, op); \
    bcemit_argued(parser, BC_SET_ATTR, name);

    lex_consume(parser, TK_NAME);
    uint8_t name = identifier_constant(parser, &parser->lex->prev);

    if(lex_match(parser, '('))
    {
        uint8_t nargs = parse_arg_list(parser);
        bcemit_argued(parser, BC_INVOKE, name);
        bcemit_byte(parser, nargs);
        return;
    }

    if(assign && lex_match(parser, '='))
    {
        expr(parser);
        bcemit_argued(parser, BC_SET_ATTR, name);
    }
    else if(assign && lex_match(parser, TK_PLUS_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_ADD);
    }
    else if(assign && lex_match(parser, TK_MINUS_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_SUBTRACT);
    }
    else if(assign && lex_match(parser, TK_STAR_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_MULTIPLY);
    }
    else if(assign && lex_match(parser, TK_SLASH_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_DIVIDE);
    }
    else if(assign && lex_match(parser, TK_PERCENT_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_MOD);
    }
    else if(assign && lex_match(parser, TK_STAR_STAR_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_POW);
    }
    else if(assign && lex_match(parser, TK_AMPERSAND_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_BAND);
    }
    else if(assign && lex_match(parser, TK_PIPE_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_BOR);
    }
    else if(assign && lex_match(parser, TK_CARET_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_BXOR);
    }
    else if(assign && lex_match(parser, TK_LESS_LESS_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_LSHIFT);
    }
    else if(assign && lex_match(parser, TK_GREATER_GREATER_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_RSHIFT);
    }
    else
    {
        if(lex_match(parser, TK_PLUS_PLUS))
        {
            SHORT_HAND_INCREMENT(BC_ADD);
        }
        else if(lex_match(parser, TK_MINUS_MINUS))
        {
            SHORT_HAND_INCREMENT(BC_SUBTRACT);
        }
        else
        {
            bcemit_argued(parser, BC_GET_ATTR, name);
        }
    }
#undef SHORT_HAND_ASSIGNMENT
#undef SHORT_HAND_INCREMENT
}

static void expr_bool(Parser* parser, bool assign)
{
    bcemit_op(parser, parser->lex->prev.type == TK_FALSE ? BC_FALSE : BC_TRUE);
}

static void expr_nil(Parser* parser, bool assign)
{
    bcemit_op(parser, BC_NIL);
}

static void expr_list(Parser* parser, bool assign)
{
    bcemit_op(parser, BC_LIST);

    if(!lex_check(parser, ']'))
    {
        do
        {
            /* Traling comma */
            if(lex_check(parser, ']'))
                break;

            if(lex_match(parser, TK_DOT_DOT_DOT))
            {
                expr(parser);
                bcemit_op(parser, BC_LIST_EXTEND);
                continue;
            }

            expr(parser);
            bcemit_op(parser, BC_LIST_ITEM);
        }
        while(lex_match(parser, ','));
    }

    lex_consume(parser, ']');
}

static void expr_map(Parser* parser, bool assign)
{
    bcemit_op(parser, BC_MAP);

    if(!lex_check(parser, '}'))
    {
        do
        {
            /* Traling comma */
            if(lex_check(parser, '}'))
                break;

            if(lex_match(parser, '['))
            {
                expr(parser);
                lex_consume(parser, ']');
                lex_consume(parser, '=');
                expr(parser);
            }
            else
            {
                lex_consume(parser, TK_NAME);
                bcemit_constant(parser, &parser->lex->prev.value);
                lex_consume(parser, '=');
                expr(parser);
            }

            bcemit_op(parser, BC_MAP_FIELD);
        }
        while(lex_match(parser, ','));
    }

    lex_consume(parser, '}');
}

static bool parse_slice(Parser* parser)
{
    expr(parser);

    /* It's a slice */
    if(lex_match(parser, ':'))
    {
        /* [n:] */
        if(lex_check(parser, ']'))
        {
            TValue v1, v2;
            setnumV(&v1, INFINITY);
            setnumV(&v2, 1);
            bcemit_constant(parser, &v1);
            bcemit_constant(parser, &v2);
        }
        else
        {
            /* [n::n] */
            if(lex_match(parser, ':'))
            {
                TValue v;
                setnumV(&v, INFINITY);
                bcemit_constant(parser, &v);
                expr(parser);
            }
            else
            {
                expr(parser);
                if(lex_match(parser, ':'))
                {
                    /* [n:n:n] */
                    expr(parser);
                }
                else
                {
                    TValue v;
                    setnumV(&v, 1);
                    bcemit_constant(parser, &v);
                }
            }
        }
        return true;
    }
    return false;
}

static void expr_subscript(Parser* parser, bool assign)
{
#define SHORT_HAND_ASSIGNMENT(op) \
    bcemit_op(parser, BC_PUSH_INDEX); \
    expr(parser); \
    bcemit_op(parser, op); \
    bcemit_op(parser, BC_SET_INDEX);

#define SHORT_HAND_INCREMENT(op) \
    bcemit_op(parser, BC_PUSH_INDEX); \
    TValue _v; \
    setnumV(&_v, 1); \
    bcemit_constant(parser, &_v); \
    bcemit_op(parser, op); \
    bcemit_op(parser, BC_SET_INDEX);

    if(parse_slice(parser))
    {
        bcemit_op(parser, BC_RANGE);
    }

    lex_consume(parser, ']');

    if(assign && lex_match(parser, '='))
    {
        expr(parser);
        bcemit_op(parser, BC_SET_INDEX);
    }
    else if(assign && lex_match(parser, TK_PLUS_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_ADD);
    }
    else if(assign && lex_match(parser, TK_MINUS_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_SUBTRACT);
    }
    else if(assign && lex_match(parser, TK_STAR_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_MULTIPLY);
    }
    else if(assign && lex_match(parser, TK_SLASH_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_DIVIDE);
    }
    else if(assign && lex_match(parser, TK_PERCENT_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_MOD);
    }
    else if(assign && lex_match(parser, TK_STAR_STAR_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_POW);
    }
    else if(assign && lex_match(parser, TK_AMPERSAND_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_BAND);
    }
    else if(assign && lex_match(parser, TK_PIPE_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_BOR);
    }
    else if(assign && lex_match(parser, TK_CARET_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_BXOR);
    }
    else if(assign && lex_match(parser, TK_LESS_LESS_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_LSHIFT);
    }
    else if(assign && lex_match(parser, TK_GREATER_GREATER_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_RSHIFT);
    }
    else
    {
        if(lex_match(parser, TK_PLUS_PLUS))
        {
            SHORT_HAND_INCREMENT(BC_ADD);
        }
        else if(lex_match(parser, TK_MINUS_MINUS))
        {
            SHORT_HAND_INCREMENT(BC_SUBTRACT);
        }
        else
        {
            bcemit_op(parser, BC_GET_INDEX);
        }
    }

#undef SHORT_HAND_ASSIGNMENT
#undef SHORT_HAND_INCREMENT
}

static void expr_or(Parser* parser, bool assign)
{
    int else_jump = bcemit_jump(parser, BC_JUMP_IF_FALSE);
    int jump = bcemit_jump(parser, BC_JUMP);
    bcpatch_jump(parser, else_jump);
    bcemit_op(parser, BC_POP);

    expr_precedence(parser, PREC_OR);
    bcpatch_jump(parser, jump);
}

static void expr_literal(Parser* parser, bool assign)
{
    bcemit_constant(parser, &parser->lex->prev.value);
}

static void expr_interpolation(Parser* parser, bool assign)
{
    bcemit_op(parser, BC_LIST);

    do
    {
        expr_literal(parser, false);
        bcemit_op(parser, BC_LIST_ITEM);

        expr(parser);
        bcemit_op(parser, BC_LIST_ITEM);
    }
    while(lex_match(parser, TK_INTERPOLATION));

    lex_consume(parser, TK_STRING);
    
    expr_literal(parser, false);
    bcemit_op(parser, BC_LIST_ITEM);

    bcemit_invoke(parser, 0, "join");
}

static void check_const(Parser* parser, uint8_t set_op, int arg)
{
    switch(set_op)
    {
        case BC_SET_LOCAL:
        {
            if(parser->locals[arg].constant)
            {
                error(parser, TEA_ERR_XVCONST);
            }
            break;
        }
        case BC_SET_UPVALUE:
        {
            if(parser->upvalues[arg].constant)
            {
                error(parser, TEA_ERR_XVCONST);
            }
            break;
        }
        case BC_SET_MODULE:
        {
            GCstr* string = strV(proto_kgc(parser->proto, arg));
            TValue* _ = tea_tab_get(&parser->lex->T->constants, string);
            if(_)
            {
                error(parser, TEA_ERR_XVCONST);
            }
            break;
        }
        default:
            break;
    }
}

static void named_variable(Parser* parser, Token name, bool assign)
{
#define SHORT_HAND_ASSIGNMENT(op) \
    check_const(parser, set_op, arg); \
    bcemit_argued(parser, get_op, (uint8_t)arg); \
    expr(parser); \
    bcemit_op(parser, op); \
    bcemit_argued(parser, set_op, (uint8_t)arg);

#define SHORT_HAND_INCREMENT(op) \
    check_const(parser, set_op, arg); \
    bcemit_argued(parser, get_op, (uint8_t)arg); \
    TValue _v; \
    setnumV(&_v, 1); \
    bcemit_constant(parser, &_v); \
    bcemit_op(parser, op); \
    bcemit_argued(parser, set_op, (uint8_t)arg);

    uint8_t get_op, set_op;
    int arg = resolve_local(parser, &name);
    if(arg != -1)
    {
        get_op = BC_GET_LOCAL;
        set_op = BC_SET_LOCAL;
    }
    else if((arg = resolve_upvalue(parser, &name)) != -1)
    {
        get_op = BC_GET_UPVALUE;
        set_op = BC_SET_UPVALUE;
    }
    else
    {
        arg = identifier_constant(parser, &name);
        get_op = BC_GET_MODULE;
        set_op = BC_SET_MODULE;
    }

    if(assign && lex_match(parser, '='))
    {
        check_const(parser, set_op, arg);
        expr(parser);
        bcemit_argued(parser, set_op, (uint8_t)arg);
    }
    else if(assign && lex_match(parser, TK_PLUS_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_ADD);
    }
    else if(assign && lex_match(parser, TK_MINUS_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_SUBTRACT);
    }
    else if(assign && lex_match(parser, TK_STAR_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_MULTIPLY);
    }
    else if(assign && lex_match(parser, TK_SLASH_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_DIVIDE);
    }
    else if(assign && lex_match(parser, TK_PERCENT_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_MOD);
    }
    else if(assign && lex_match(parser, TK_STAR_STAR_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_POW);
    }
    else if(assign && lex_match(parser, TK_AMPERSAND_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_BAND);
    }
    else if(assign && lex_match(parser, TK_PIPE_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_BOR);
    }
    else if(assign && lex_match(parser, TK_CARET_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_BXOR);
    }
    else if(assign && lex_match(parser, TK_LESS_LESS_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_LSHIFT);
    }
    else if(assign && lex_match(parser, TK_GREATER_GREATER_EQUAL))
    {
        SHORT_HAND_ASSIGNMENT(BC_RSHIFT);
    }
    else
    {
        if(lex_match(parser, TK_PLUS_PLUS))
        {
            SHORT_HAND_INCREMENT(BC_ADD);
        }
        else if(lex_match(parser, TK_MINUS_MINUS))
        {
            SHORT_HAND_INCREMENT(BC_SUBTRACT);
        }
        else
        {
            bcemit_argued(parser, get_op, (uint8_t)arg);
        }
    }
#undef SHORT_HAND_ASSIGNMENT
#undef SHORT_HAND_INCREMENT
}

static void expr_name(Parser* parser, bool assign)
{
    Token name = parser->lex->prev;

    if(lex_match(parser, TK_ARROW))
    {
        Parser fn_parser;

        parser_init(parser->lex, &fn_parser, parser, PROTO_ANONYMOUS);
        fn_parser.proto->numparams = 1;

        uint8_t constant = parse_variable_at(&fn_parser, name);
        define_variable(&fn_parser, constant, false);

        if(lex_match(&fn_parser, '{'))
        {
            /* Brace so expect a block */
            parse_block(&fn_parser);
            bcemit_return(&fn_parser);
        }
        else
        {
            /* No brace, so expect single expression */
            expr(&fn_parser);
            bcemit_op(&fn_parser, BC_RETURN);
        }
        parser_end(&fn_parser);
        return;
    }

    named_variable(parser, name, assign);
}

static void expr_super(Parser* parser, bool assign)
{
    if(parser->klass == NULL)
    {
        error(parser, TEA_ERR_XSUPERO);
    }
    else if(parser->klass->is_static)
    {
        error(parser, TEA_ERR_XTHISM);
    }
    else if(!parser->klass->has_superclass)
    {
        error(parser, TEA_ERR_XSUPERK);
    }

    /* super */
    if(!lex_check(parser, '(') && !lex_check(parser, '.'))
    {
        named_variable(parser, lex_synthetic(parser, "super"), false);
        return;
    }

    /* super() -> super.init() */
    if(lex_match(parser, '('))
    {
        Token token = lex_synthetic(parser, "new");

        uint8_t name = identifier_constant(parser, &token);
        named_variable(parser, lex_synthetic(parser, "this"), false);
        uint8_t nargs = parse_arg_list(parser);
        named_variable(parser, lex_synthetic(parser, "super"), false);
        bcemit_argued(parser, BC_SUPER, name);
        bcemit_byte(parser, nargs);
        return;
    }

    /* super.name */
    lex_consume(parser, '.');
    lex_consume(parser, TK_NAME);
    uint8_t name = identifier_constant(parser, &parser->lex->prev);

    named_variable(parser, lex_synthetic(parser, "this"), false);

    if(lex_match(parser, '('))
    {
        /* super.name() */
        uint8_t nargs = parse_arg_list(parser);
        named_variable(parser, lex_synthetic(parser, "super"), false);
        bcemit_argued(parser, BC_SUPER, name);
        bcemit_byte(parser, nargs);
    }
    else
    {
        /* super.name */
        named_variable(parser, lex_synthetic(parser, "super"), false);
        bcemit_argued(parser, BC_GET_SUPER, name);
    }
}

static void expr_this(Parser* parser, bool assign)
{
    if(parser->klass == NULL)
    {
        error(parser, TEA_ERR_XTHISO);
    }
    else if(parser->klass->is_static)
    {
        error(parser, TEA_ERR_XTHISM);
    }

    expr_name(parser, false);
}

static void expr_static(Parser* parser, bool assign)
{
    if(parser->klass == NULL)
    {
        error(parser, TEA_ERR_XSTATIC);
    }
}

static void expr_unary(Parser* parser, bool assign)
{
    int op = parser->lex->prev.type;
    expr_precedence(parser, PREC_UNARY);

    /* Emit the operator instruction */
    switch(op)
    {
        case TK_NOT:
        case '!':
        {
            bcemit_op(parser, BC_NOT);
            break;
        }
        case '-':
        {
            bcemit_op(parser, BC_NEGATE);
            break;
        }
        case '~':
        {
            bcemit_op(parser, BC_BNOT);
            break;
        }
        default:
            return; /* Unreachable */
    }
}

static void expr_range(Parser* parser, bool assign)
{
    LexToken op_type = parser->lex->prev.type;
    ParseRule rule = expr_rule(op_type);
    expr_precedence(parser, (Precedence)(rule.prec + 1));

    if(lex_match(parser, TK_DOT_DOT))
    {
        expr(parser);
    }
    else
    {
        TValue v;
        setnumV(&v, 1);
        bcemit_constant(parser, &v);
    }

    bcemit_op(parser, BC_RANGE);
}

#define NONE                    (ParseRule){ NULL, NULL, PREC_NONE }
#define RULE(pr, in, prec)      (ParseRule){ pr, in, prec }
#define INFIX(in)               (ParseRule){ NULL, in, PREC_NONE }
#define PREFIX(pr)              (ParseRule){ pr, NULL, PREC_NONE }
#define OPERATOR(in, prec)      (ParseRule){ NULL, in, prec }

static ParseRule expr_rule(int type)
{
    switch(type)
    {
        case '(':
            return RULE(expr_grouping, expr_call, PREC_CALL);
        case '[':
            return RULE(expr_list, expr_subscript, PREC_SUBSCRIPT);
        case '{':
            return PREFIX(expr_map);
        case '.':
            return OPERATOR(expr_dot, PREC_CALL);
        case '?':
            return OPERATOR(expr_ternary, PREC_ASSIGNMENT);
        case '-':
            return RULE(expr_unary, expr_binary, PREC_TERM);
        case '+':
            return OPERATOR(expr_binary, PREC_TERM);
        case '/':
        case '*':
            return OPERATOR(expr_binary, PREC_FACTOR);
        case TK_BANG_EQUAL:
        case TK_EQUAL_EQUAL:
            return OPERATOR(expr_binary, PREC_EQUALITY);
        case '<':
        case '>':
        case TK_GREATER_EQUAL:
        case TK_LESS_EQUAL:
        case TK_IN:
            return OPERATOR(expr_binary, PREC_COMPARISON);
        case '%':
            return OPERATOR(expr_binary, PREC_FACTOR);
        case TK_STAR_STAR:
            return OPERATOR(expr_binary, PREC_INDICES);
        case TK_NOT:
            return RULE(expr_unary, expr_binary, PREC_IS);
        case TK_DOT_DOT:
            return OPERATOR(expr_range, PREC_RANGE);
        case '&':
            return OPERATOR(expr_binary, PREC_BAND);
        case '|':
            return OPERATOR(expr_binary, PREC_BOR);
        case '^':
            return OPERATOR(expr_binary, PREC_BXOR);
        case '~':
        case '!':
            return PREFIX(expr_unary);
        case TK_LESS_LESS:
        case TK_GREATER_GREATER:
            return OPERATOR(expr_binary, PREC_SHIFT);
        case TK_NAME:
            return PREFIX(expr_name);
        case TK_NUMBER:
        case TK_STRING:
            return PREFIX(expr_literal);
        case TK_INTERPOLATION:
            return PREFIX(expr_interpolation);
        case TK_AND:
            return OPERATOR(expr_and, PREC_AND);
        case TK_STATIC:
            return PREFIX(expr_static);
        case TK_FUNCTION:
            return PREFIX(expr_anonymous);
        case TK_NIL:
            return PREFIX(expr_nil);
        case TK_OR:
            return OPERATOR(expr_or, PREC_OR);
        case TK_IS:
            return OPERATOR(expr_binary, PREC_IS);
        case TK_SUPER:
            return PREFIX(expr_super);
        case TK_THIS:
            return PREFIX(expr_this);
        case TK_TRUE:
        case TK_FALSE:
            return PREFIX(expr_bool);
        default:
            return NONE;
    }
}

#undef NONE
#undef RULE
#undef INFIX
#undef PREFIX
#undef OPERATOR

static void expr_precedence(Parser* parser, Precedence prec)
{
    tea_lex_next(parser->lex);
    ParseFn prefix_rule = expr_rule(parser->lex->prev.type).prefix;
    if(prefix_rule == NULL)
    {
        error(parser, TEA_ERR_XEXPR);
    }

    bool assign = prec <= PREC_ASSIGNMENT;
    prefix_rule(parser, assign);

    while(prec <= expr_rule(parser->lex->curr.type).prec)
    {
        tea_lex_next(parser->lex);
        ParseFn infix_rule = expr_rule(parser->lex->prev.type).infix;
        infix_rule(parser, assign);
    }

    if(assign && lex_match(parser, '='))
    {
        error(parser, TEA_ERR_XASSIGN);
    }
}

static void expr(Parser* parser)
{
    expr_precedence(parser, PREC_ASSIGNMENT);
}

/* Parse a block */
static void parse_block(Parser* parser)
{
    while(!lex_check(parser, '}') && !lex_check(parser, TK_EOF))
    {
        parse_decl(parser);
    }

    lex_consume(parser, '}');
}

static void check_parameters(Parser* parser, Token* name)
{
    for(int i = parser->local_count - 1; i >= 0; i--)
    {
        Local* local = &parser->locals[i];
        if(identifiers_equal(name, &local->name))
        {
            error(parser, TEA_ERR_XDUPARGS);
        }
    }
}

static void begin_function(Parser* parser, Parser* fn_parser)
{
    scope_begin(fn_parser);

    if(!lex_check(fn_parser, ')'))
    {
        bool optional = false;
        bool spread = false;

        do
        {
            if(spread)
            {
                error(fn_parser, TEA_ERR_XSPREADARGS);
            }

            spread = lex_match(fn_parser, TK_DOT_DOT_DOT);
            lex_consume(fn_parser, TK_NAME);

            Token name = fn_parser->lex->prev;
            check_parameters(fn_parser, &name);

            if(spread)
            {
                fn_parser->proto->variadic = spread;
            }

            if(lex_match(fn_parser, '='))
            {
                if(spread)
                {
                    error(fn_parser, TEA_ERR_XSPREADOPT);
                }
                fn_parser->proto->numopts++;
                optional = true;
                expr(fn_parser);
            }
            else
            {
                fn_parser->proto->numparams++;

                if(optional)
                {
                    error(fn_parser, TEA_ERR_XOPT);
                }
            }

            if(fn_parser->proto->numparams + fn_parser->proto->numopts > 255)
            {
                error(fn_parser, TEA_ERR_XMAXARGS);
            }

            uint8_t constant = parse_variable_at(fn_parser, name);
            define_variable(fn_parser, constant, false);
        }
        while(lex_match(fn_parser, ','));

        if(fn_parser->proto->numopts > 0)
        {
            bcemit_op(fn_parser, BC_DEFINE_OPTIONAL);
            bcemit_bytes(fn_parser, fn_parser->proto->numparams, fn_parser->proto->numopts);
        }
    }

    lex_consume(fn_parser, ')');
}

static void function(Parser* parser, ProtoType type)
{
    Parser fn_parser;

    parser_init(parser->lex, &fn_parser, parser, type);
    lex_consume(&fn_parser, '(');
    begin_function(parser, &fn_parser);

    lex_consume(&fn_parser, '{');
    parse_block(&fn_parser);
    bcemit_return(&fn_parser);
    parser_end(&fn_parser);
}

static void expr_anonymous(Parser* parser, bool assign)
{
    function(parser, PROTO_FUNCTION);
}

static void parse_arrow(Parser* parser)
{
    Parser fn_parser;

    parser_init(parser->lex, &fn_parser, parser, PROTO_ANONYMOUS);
    begin_function(parser, &fn_parser);

    lex_consume(&fn_parser, TK_ARROW);
    if(lex_match(&fn_parser, '{'))
    {
        /* Brace so expect a block */
        parse_block(&fn_parser);
        bcemit_return(&fn_parser);
    }
    else
    {
        /* No brace, so expect single expression */
        expr(&fn_parser);
        bcemit_op(&fn_parser, BC_RETURN);
    }
    parser_end(&fn_parser);
}

static void expr_grouping(Parser* parser, bool assign)
{
    /* () => ...; (...v) => ... */
    if(lex_check(parser, ')') || lex_check(parser, TK_DOT_DOT_DOT))
    {
        parse_arrow(parser);
        return;
    }

    LexToken curr = parser->lex->curr.type;
    LexToken next = parser->lex->next.type;

    /* (a) => ...; (a, ) => ... */
    if((curr == TK_NAME && (next == ',' || next == ')')) || (curr == ')' && next == TK_ARROW))
    {
        parse_arrow(parser);
        return;
    }

    expr(parser);
    lex_consume(parser, ')');
}

static const LexToken ops[] = {
    '+', '-', '*', '/', '%',
    TK_STAR_STAR,        /* ** */
    '&', '|', '~', '^',
    TK_LESS_LESS,        /* << */
    TK_GREATER_GREATER,  /* >> */
	'<',
	TK_LESS_EQUAL,       /* <= */
	'>',
	TK_GREATER_EQUAL,    /* >= */
	TK_EQUAL_EQUAL,      /* == */
    '[',
    TK_EOF
};

#define SENTINEL 18

static void parse_operator(Parser* parser)
{
    int i = 0;
    while(ops[i] != TK_EOF)
    {
        if(lex_match(parser, ops[i]))
        {
            break;
        }

        i++;
    }

    if(i == SENTINEL)
    {
        error_at_current(parser, TEA_ERR_XMETHOD);
    }

    GCstr* name = NULL;

    if(parser->lex->prev.type == '[')
    {
        parser->klass->is_static = false;
        lex_consume(parser, ']');
        if(lex_match(parser, '='))
            name = mmname_str(parser->lex->T, MM_SETINDEX);
        else
            name = mmname_str(parser->lex->T, MM_GETINDEX);
    }
    else
    {
        parser->klass->is_static = true;
        name = mmname_str(parser->lex->T, i);
    }

    TValue v;
    setstrV(parser->lex->T, &v, name);
    uint8_t constant = const_make(parser, &v);

    parser->name = name;
    function(parser, PROTO_OPERATOR);
    bcemit_argued(parser, BC_METHOD, constant);
    parser->klass->is_static = false;
}

static void parse_method(Parser* parser, ProtoType type)
{
    uint8_t constant = identifier_constant(parser, &parser->lex->prev);

    if(strV(&parser->lex->prev.value) == parser->lex->T->init_str)
    {
        type = PROTO_INIT;
    }

    function(parser, type);
    bcemit_argued(parser, BC_METHOD, constant);
}

static void parserclass_init(Parser* parser, ClassParser* classparser)
{
    classparser->is_static = false;
    classparser->has_superclass = false;
    classparser->enclosing = parser->klass;
    parser->klass = classparser;
}

static void parse_class_body(Parser* parser)
{
    while(!lex_check(parser, '}') && !lex_check(parser, TK_EOF))
    {
        if(lex_match(parser, TK_NAME))
        {
            parse_method(parser, PROTO_METHOD);
        }
        else
        {
            parse_operator(parser);
        }
    }
}

/* Parse 'class' declaration */
static void parse_class(Parser* parser)
{
    tea_lex_next(parser->lex);  /* Skip 'class' */
    lex_consume(parser, TK_NAME);
    Token class_name = parser->lex->prev;
    uint8_t name_constant = identifier_constant(parser, &parser->lex->prev);
    declare_variable(parser, &parser->lex->prev);

    bcemit_argued(parser, BC_CLASS, name_constant);
    define_variable(parser, name_constant, false);

    ClassParser class_compiler;
    parserclass_init(parser, &class_compiler);

    if(lex_match(parser, ':'))
    {
        expr(parser);

        scope_begin(parser);
        add_local(parser, lex_synthetic(parser, "super"));
        define_variable(parser, 0, false);

        named_variable(parser, class_name, false);
        bcemit_op(parser, BC_INHERIT);
        class_compiler.has_superclass = true;
    }

    named_variable(parser, class_name, false);

    lex_consume(parser, '{');
    parse_class_body(parser);
    lex_consume(parser, '}');

    bcemit_op(parser, BC_POP);

    if(class_compiler.has_superclass)
    {
        scope_end(parser);
    }

    parser->klass = parser->klass->enclosing;
}

static void parse_function_assign(Parser* parser)
{
    if(lex_match(parser, '.'))
    {
        lex_consume(parser, TK_NAME);
        uint8_t expr_dot = identifier_constant(parser, &parser->lex->prev);
        if(!lex_check(parser, '('))
        {
            bcemit_argued(parser, BC_GET_ATTR, expr_dot);
            parse_function_assign(parser);
        }
        else
        {
            function(parser, PROTO_FUNCTION);
            bcemit_argued(parser, BC_SET_ATTR, expr_dot);
            bcemit_op(parser, BC_POP);
            return;
        }
    }
    else if(lex_match(parser, ':'))
    {
        lex_consume(parser, TK_NAME);
        uint8_t constant = identifier_constant(parser, &parser->lex->prev);

        ClassParser class_compiler;
        parserclass_init(parser, &class_compiler);

        function(parser, PROTO_METHOD);

        parser->klass = parser->klass->enclosing;

        bcemit_argued(parser, BC_EXTENSION_METHOD, constant);
        bcemit_op(parser, BC_POP);
        return;
    }
}

/* Parse 'function' declaration */
static void parse_function(Parser* parser)
{
    tea_lex_next(parser->lex);  /* Skip 'function' */
    lex_consume(parser, TK_NAME);
    Token name = parser->lex->prev;

    if(lex_check(parser, '.') || lex_check(parser, ':'))
    {
        named_variable(parser, name, false);
        parse_function_assign(parser);
        return;
    }

    uint8_t global = parse_variable_at(parser, name);
    mark_initialized(parser, false);
    function(parser, PROTO_FUNCTION);
    define_variable(parser, global, false);
}

/* Parse 'var' or 'const' declaration */
static void parse_var(Parser* parser, bool constant)
{
    Token variables[255];
    int var_count = 0;
    int expr_count = 0;
    bool rest = false;
    int rest_count = 0;
    int rest_pos = 0;

    do
    {
        if(rest_count > 1)
        {
            error(parser, TEA_ERR_XDOTS);
        }

        if(lex_match(parser, TK_DOT_DOT_DOT))
        {
            rest = true;
            rest_count++;
        }

        lex_consume(parser, TK_NAME);
        variables[var_count] = parser->lex->prev;
        var_count++;

        if(rest)
        {
            rest_pos = var_count;
            rest = false;
        }

        if(var_count == 1 && lex_match(parser, '='))
        {
            if(rest_count)
            {
                error(parser, TEA_ERR_XSINGLEREST);
            }

            uint8_t global = parse_variable_at(parser, variables[0]);
            expr(parser);
            define_variable(parser, global, constant);

            if(lex_match(parser, ','))
            {
                do
                {
                    uint8_t global = parse_variable(parser);
                    lex_consume(parser, '=');
                    expr(parser);
                    define_variable(parser, global, constant);
                }
                while(lex_match(parser, ','));
            }
            return;
        }
    }
    while(lex_match(parser, ','));

    if(rest_count)
    {
        lex_consume(parser, '=');
        expr(parser);
        bcemit_op(parser, BC_UNPACK_REST);
        bcemit_bytes(parser, var_count, rest_pos - 1);
        goto finish;
    }

    if(lex_match(parser, '='))
    {
        do
        {
            expr(parser);
            expr_count++;
            if(expr_count == 1 && (!lex_check(parser, ',')))
            {
                bcemit_argued(parser, BC_UNPACK, var_count);
                goto finish;
            }

        }
        while(lex_match(parser, ','));

        if(expr_count != var_count)
        {
            error(parser, TEA_ERR_XVALASSIGN);
        }
    }
    else
    {
        for(int i = 0; i < var_count; i++)
        {
            bcemit_op(parser, BC_NIL);
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

/* Parse an expression statement */
static void parse_expr_stmt(Parser* parser)
{
    expr(parser);
    if(parser->lex->T->repl && parser->type == PROTO_SCRIPT)
    {
        bcemit_op(parser, BC_PRINT);
    }
    else
    {
        bcemit_op(parser, BC_POP);
    }
}

/* Get the number of bytes for the arguments of bytecode instructions */
static int get_arg_count(uint8_t* code, const TValue* constants, int ip)
{
    switch(code[ip])
    {
        case BC_NIL:
        case BC_TRUE:
        case BC_FALSE:
        case BC_RANGE:
        case BC_GET_INDEX:
        case BC_SET_INDEX:
        case BC_PUSH_INDEX:
        case BC_INHERIT:
        case BC_POP:
        case BC_PRINT:
        case BC_IS:
        case BC_IN:
        case BC_EQUAL:
        case BC_GREATER:
        case BC_GREATER_EQUAL:
        case BC_LESS:
        case BC_LESS_EQUAL:
        case BC_ADD:
        case BC_SUBTRACT:
        case BC_MULTIPLY:
        case BC_DIVIDE:
        case BC_MOD:
        case BC_POW:
        case BC_NOT:
        case BC_NEGATE:
        case BC_CLOSE_UPVALUE:
        case BC_RETURN:
        case BC_IMPORT_ALIAS:
        case BC_IMPORT_END:
        case BC_END:
        case BC_BAND:
        case BC_BOR:
        case BC_BXOR:
        case BC_BNOT:
        case BC_LSHIFT:
        case BC_RSHIFT:
        case BC_LIST:
        case BC_MAP:
        case BC_LIST_EXTEND:
        case BC_LIST_ITEM:
        case BC_MAP_FIELD:
            return 0;
        case BC_CONSTANT:
        case BC_GET_LOCAL:
        case BC_SET_LOCAL:
        case BC_GET_MODULE:
        case BC_SET_MODULE:
        case BC_DEFINE_MODULE:
        case BC_GET_UPVALUE:
        case BC_SET_UPVALUE:
        case BC_GET_ATTR:
        case BC_PUSH_ATTR:
        case BC_SET_ATTR:
        case BC_GET_SUPER:
        case BC_CLASS:
        case BC_CALL:
        case BC_METHOD:
        case BC_EXTENSION_METHOD:
        case BC_IMPORT_STRING:
        case BC_IMPORT_NAME:
        case BC_UNPACK:
        case BC_MULTI_CASE:
            return 1;
        case BC_IMPORT_VARIABLE:
        case BC_UNPACK_REST:
        case BC_DEFINE_OPTIONAL:
        case BC_COMPARE_JUMP:
        case BC_JUMP:
        case BC_JUMP_IF_FALSE:
        case BC_JUMP_IF_NIL:
        case BC_LOOP:
        case BC_INVOKE:
        case BC_SUPER:
        case BC_GET_ITER:
        case BC_FOR_ITER:
            return 2;
        case BC_CLOSURE:
        {
            int constant = code[ip + 1];
            GCproto* loaded_fn = protoV(constants + constant);

            /* There is one byte for the constant, then two for each upvalue */
            return 1 + (loaded_fn->upvalue_count * 2);
        }
    }

    return 0;
}

static void loop_begin(Parser* parser, Loop* loop)
{
    loop->start = parser->proto->bc_count;
    loop->scope_depth = parser->scope_depth;
    loop->enclosing = parser->loop;
    parser->loop = loop;
}

static void loop_end(Parser* parser)
{
    if(parser->loop->end != -1)
    {
        bcpatch_jump(parser, parser->loop->end);
        bcemit_op(parser, BC_POP);
    }

    int i = parser->loop->body;
    while(i < parser->proto->bc_count)
    {
        if(parser->proto->bc[i] == BC_END)
        {
            parser->proto->bc[i] = BC_JUMP;
            bcpatch_jump(parser, i + 1);
            i += 3;
        }
        else
        {
            i += 1 + get_arg_count(parser->proto->bc, parser->proto->k, i);
        }
    }

    parser->loop = parser->loop->enclosing;
}

/* Parse iterable 'for' */
static void parse_for_in(Parser* parser, Token var, bool constant)
{
    Token variables[255];
    int var_count = 1;
    variables[0] = var;

    if(lex_match(parser, ','))
    {
        do
        {
            lex_consume(parser, TK_NAME);
            variables[var_count] = parser->lex->prev;
            var_count++;
        }
        while(lex_match(parser, ','));
    }

    lex_consume(parser, TK_IN);

    expr(parser);
    int seq_slot = add_init_local(parser, lex_synthetic(parser, "seq "));

    expr_nil(parser, false);
    int iter_slot = add_init_local(parser, lex_synthetic(parser, "iter "));

    lex_consume(parser, ')');

    Loop loop;
    loop_begin(parser, &loop);

    /* Get the iterator index. If it's nil, it means the loop is over */
    bcemit_op(parser, BC_GET_ITER);
    bcemit_bytes(parser, seq_slot, iter_slot);
    parser->loop->end = bcemit_jump(parser, BC_JUMP_IF_NIL);
    bcemit_op(parser, BC_POP);

    /* Get the iterator value */
    bcemit_op(parser, BC_FOR_ITER);
    bcemit_bytes(parser, seq_slot, iter_slot);

    scope_begin(parser);

    if(var_count > 1)
        bcemit_argued(parser, BC_UNPACK, var_count);

    for(int i = 0; i < var_count; i++)
    {
        declare_variable(parser, &variables[i]);
        define_variable(parser, 0, constant);
    }

    parser->loop->body = parser->proto->bc_count;
    parse_stmt(parser);

    /* Loop variable */
    scope_end(parser);

    bcemit_loop(parser, parser->loop->start);
    loop_end(parser);

    /* Hidden variables */
    scope_end(parser);
}

/* Parse generic 'for' */
static void parse_for(Parser* parser)
{
    scope_begin(parser);

    tea_lex_next(parser->lex);  /* Skip 'for' */
    lex_consume(parser, '(');

    int loop_var = -1;
    Token var_name = parser->lex->curr;

    bool constant = false;
    if(lex_match(parser, TK_VAR) || (constant = lex_match(parser, TK_CONST)))
    {
        lex_consume(parser, TK_NAME);
        Token var = parser->lex->prev;

        if(lex_check(parser, TK_IN) || lex_check(parser, ','))
        {
            /* It's a for in statement */
            parse_for_in(parser, var, constant);
            return;
        }

        /* Grab the name of the loop variable */
        var_name = var;

        uint8_t global = parse_variable_at(parser, var);

        if(lex_match(parser, '='))
        {
            expr(parser);
        }
        else
        {
            bcemit_op(parser, BC_NIL);
        }

        define_variable(parser, global, constant);
        lex_consume(parser, ';');

        /* Get loop variable slot */
        loop_var = parser->local_count - 1;
    }
    else
    {
        parse_expr_stmt(parser);
        lex_consume(parser, ';');
    }

    Loop loop;
    loop_begin(parser, &loop);

    parser->loop->end = -1;

    expr(parser);
    lex_consume(parser, ';');

    parser->loop->end = bcemit_jump(parser, BC_JUMP_IF_FALSE);
    bcemit_op(parser, BC_POP); /* Condition */

    int body_jump = bcemit_jump(parser, BC_JUMP);

    int increment_start = parser->proto->bc_count;
    expr(parser);
    bcemit_op(parser, BC_POP);
    lex_consume(parser, ')');

    bcemit_loop(parser, parser->loop->start);
    parser->loop->start = increment_start;

    bcpatch_jump(parser, body_jump);

    parser->loop->body = parser->proto->bc_count;

    int inner_var = -1;
    if(loop_var != -1)
    {
        scope_begin(parser);
        bcemit_argued(parser, BC_GET_LOCAL, (uint8_t)loop_var);
        add_local(parser, var_name);
        mark_initialized(parser, false);
        inner_var = parser->local_count - 1;
    }

    parse_stmt(parser);

    if(inner_var != -1)
    {
        bcemit_argued(parser, BC_GET_LOCAL, (uint8_t)inner_var);
        bcemit_argued(parser, BC_SET_LOCAL, (uint8_t)loop_var);
        bcemit_op(parser, BC_POP);
        scope_end(parser);
    }

    bcemit_loop(parser, parser->loop->start);

    loop_end(parser);
    scope_end(parser);
}

/* Parse 'break' statement */
static void parse_break(Parser* parser)
{
    tea_lex_next(parser->lex);  /* Skip 'break' */

    if(parser->loop == NULL)
    {
        error(parser, TEA_ERR_XBREAK);
    }

    /* Discard any locals created inside the loop */
    discard_locals(parser, parser->loop->scope_depth + 1);

    bcemit_jump(parser, BC_END);
}

/* Parse 'continue' statement */
static void parse_continue(Parser* parser)
{
    tea_lex_next(parser->lex);  /* Skip 'continue' */

    if(parser->loop == NULL)
    {
        error(parser, TEA_ERR_XCONTINUE);
    }

    /* Discard any locals created inside the loop */
    discard_locals(parser, parser->loop->scope_depth + 1);

    /* Jump to the top of the innermost loop */
    bcemit_loop(parser, parser->loop->start);
}

/* Parse 'if' statement */
static void parse_if(Parser* parser)
{
    tea_lex_next(parser->lex);  /* Skip 'if' */
    lex_consume(parser, '(');
    expr(parser);
    lex_consume(parser, ')');

    int else_jump = bcemit_jump(parser, BC_JUMP_IF_FALSE);

    bcemit_op(parser, BC_POP);
    parse_stmt(parser);

    int end_jump = bcemit_jump(parser, BC_JUMP);

    bcpatch_jump(parser, else_jump);
    bcemit_op(parser, BC_POP);

    if(lex_match(parser, TK_ELSE))
        parse_stmt(parser);

    bcpatch_jump(parser, end_jump);
}

/* Parse 'switch' statement */
static void parse_switch(Parser* parser)
{
    int case_ends[256];
    int case_count = 0;

    tea_lex_next(parser->lex);  /* Skip 'switch' */
    lex_consume(parser, '(');
    expr(parser);
    lex_consume(parser, ')');
    lex_consume(parser, '{');

    if(lex_match(parser, TK_CASE))
    {
        do
        {
            expr(parser);
            int multiple_cases = 0;
            if(lex_match(parser, ','))
            {
                do
                {
                    multiple_cases++;
                    expr(parser);
                }
                while(lex_match(parser, ','));
                bcemit_argued(parser, BC_MULTI_CASE, multiple_cases);
            }
            int compare_jump = bcemit_jump(parser, BC_COMPARE_JUMP);
            lex_consume(parser, ':');
            parse_stmt(parser);
            case_ends[case_count++] = bcemit_jump(parser, BC_JUMP);
            bcpatch_jump(parser, compare_jump);
            if(case_count > 255)
            {
                error_at_current(parser, TEA_ERR_XSWITCH);
            }

        }
        while(lex_match(parser, TK_CASE));
    }

    bcemit_op(parser, BC_POP); /* Expression */
    if(lex_match(parser, TK_DEFAULT))
    {
        lex_consume(parser, ':');
        parse_stmt(parser);
    }

    if(lex_match(parser, TK_CASE))
    {
        error(parser, TEA_ERR_XCASE);
    }

    lex_consume(parser, '}');

    for(int i = 0; i < case_count; i++)
    {
    	bcpatch_jump(parser, case_ends[i]);
    }
}

/* Parse 'return' statement */
static void parse_return(Parser* parser)
{
    tea_lex_next(parser->lex);  /* Skip 'return' */

    if(parser->type == PROTO_SCRIPT)
    {
        error(parser, TEA_ERR_XRET);
    }

    if(lex_check(parser, '}') || lex_match(parser, ';'))
    {
        bcemit_return(parser);
    }
    else
    {
        if(parser->type == PROTO_INIT)
        {
            error(parser, TEA_ERR_XINIT);
        }

        expr(parser);
        bcemit_op(parser, BC_RETURN);
    }
}

/* Parse 'import' statement */
static void parse_import(Parser* parser)
{
    if(lex_match(parser, TK_STRING))
    {
        int import_constant = const_make(parser, &parser->lex->prev.value);

        bcemit_argued(parser, BC_IMPORT_STRING, import_constant);
        bcemit_op(parser, BC_POP);

        if(lex_match(parser, TK_AS))
        {
            uint8_t import_name = parse_variable(parser);
            bcemit_op(parser, BC_IMPORT_ALIAS);
            define_variable(parser, import_name, false);
        }

        bcemit_op(parser, BC_IMPORT_END);

        if(lex_match(parser, ','))
        {
            parse_import(parser);
        }
    }
    else
    {
        lex_consume(parser, TK_NAME);
        uint8_t import_name = identifier_constant(parser, &parser->lex->prev);
        declare_variable(parser, &parser->lex->prev);

        if(lex_match(parser, TK_AS))
        {
            uint8_t import_alias = parse_variable(parser);

            bcemit_argued(parser, BC_IMPORT_NAME, import_name);
            define_variable(parser, import_alias, false);
        }
        else
        {
            bcemit_argued(parser, BC_IMPORT_NAME, import_name);
            define_variable(parser, import_name, false);
        }

        bcemit_op(parser, BC_IMPORT_END);

        if(lex_match(parser, ','))
        {
            parse_import(parser);
        }
    }
}

/* Parse 'from' statement */
static void parse_from_import(Parser* parser)
{
    if(lex_match(parser, TK_STRING))
    {
        int import_constant = const_make(parser, &parser->lex->prev.value);

        lex_consume(parser, TK_IMPORT);
        bcemit_argued(parser, BC_IMPORT_STRING, import_constant);
        bcemit_op(parser, BC_POP);
    }
    else
    {
        lex_consume(parser, TK_NAME);
        uint8_t import_name = identifier_constant(parser, &parser->lex->prev);

        lex_consume(parser, TK_IMPORT);

        bcemit_argued(parser, BC_IMPORT_NAME, import_name);
        bcemit_op(parser, BC_POP);
    }

    int var_count = 0;

    do
    {
        lex_consume(parser, TK_NAME);
        Token var_token = parser->lex->prev;
        uint8_t var_constant = identifier_constant(parser, &var_token);

        uint8_t slot;
        if(lex_match(parser, TK_AS))
        {
            slot = parse_variable(parser);
        }
        else
        {
            slot = parse_variable_at(parser, var_token);
        }

        bcemit_argued(parser, BC_IMPORT_VARIABLE, var_constant);
        define_variable(parser, slot, false);

        var_count++;
        if(var_count > 255)
        {
            error(parser, TEA_ERR_XVARS);
        }
    }
    while(lex_match(parser, ','));

    bcemit_op(parser, BC_IMPORT_END);
}

/* Parse 'while' statement */
static void parse_while(Parser* parser)
{
    Loop loop;
    loop_begin(parser, &loop);

    tea_lex_next(parser->lex);  /* Skip 'while' */
    if(!lex_check(parser, '('))
    {
        bcemit_byte(parser, BC_TRUE);
    }
    else
    {
        lex_consume(parser, '(');
        expr(parser);
        lex_consume(parser, ')');
    }

    /* Jump ot of the loop if the condition is false */
    parser->loop->end = bcemit_jump(parser, BC_JUMP_IF_FALSE);
    bcemit_op(parser, BC_POP);

    /* Compile the body */
    parser->loop->body = parser->proto->bc_count;
    parse_stmt(parser);

    /* Loop back to the start */
    bcemit_loop(parser, parser->loop->start);
    loop_end(parser);
}

/* Parse 'do' statement */
static void parse_do(Parser* parser)
{
    Loop loop;
    loop_begin(parser, &loop);

    tea_lex_next(parser->lex);  /* Skip 'do' */

    parser->loop->body = parser->proto->bc_count;
    parse_stmt(parser);

    lex_consume(parser, TK_WHILE);

    if(!lex_check(parser, '('))
    {
        bcemit_op(parser, BC_TRUE);
    }
    else
    {
        lex_consume(parser, '(');
        expr(parser);
        lex_consume(parser, ')');
    }

    parser->loop->end = bcemit_jump(parser, BC_JUMP_IF_FALSE);
    bcemit_op(parser, BC_POP);

    bcemit_loop(parser, parser->loop->start);
    loop_end(parser);
}

static void parse_multiple_assign(Parser* parser)
{
    int expr_count = 0;
    int var_count = 0;
    Token variables[255];

    do
    {
        lex_consume(parser, TK_NAME);
        variables[var_count] = parser->lex->prev;
        var_count++;
    }
    while(lex_match(parser, ','));

    lex_consume(parser, '=');

    do
    {
        expr(parser);
        expr_count++;
        if(expr_count == 1 && (!lex_check(parser, ',')))
        {
            bcemit_argued(parser, BC_UNPACK, var_count);
            goto finish;
        }
    }
    while(lex_match(parser, ','));

    if(expr_count != var_count)
    {
        error(parser, TEA_ERR_XVASSIGN);
    }

finish:
    for(int i = var_count - 1; i >= 0; i--)
    {
        Token token = variables[i];

        uint8_t set_op;
        int arg = resolve_local(parser, &token);
        if(arg != -1)
        {
            set_op = BC_SET_LOCAL;
        }
        else if((arg = resolve_upvalue(parser, &token)) != -1)
        {
            set_op = BC_SET_UPVALUE;
        }
        else
        {
            arg = identifier_constant(parser, &token);
            set_op = BC_SET_MODULE;
        }
        check_const(parser, set_op, arg);
        bcemit_argued(parser, set_op, (uint8_t)arg);
        bcemit_op(parser, BC_POP);
    }
}

/* -- Parse statements and declarations ---------------------------------------------------- */

/* Parse a declaration */
static void parse_decl(Parser* parser)
{
    LexToken tok = parser->lex->curr.type;
    switch(tok)
    {
        case TK_CLASS:
            parse_class(parser);
            break;
        case TK_FUNCTION:
            parse_function(parser);
            break;
        case TK_CONST:
        case TK_VAR:
            tea_lex_next(parser->lex);  /* Skip 'const' or 'var' */
            parse_var(parser, tok == TK_CONST);
            break;
        default:
            parse_stmt(parser);
            break;
    }
}

/* Parse a statement */
static void parse_stmt(Parser* parser)
{
    switch(parser->lex->curr.type)
    {
        case ';':
            tea_lex_next(parser->lex);
            break;
        case TK_FOR:
            parse_for(parser);
            break;
        case TK_IF:
            parse_if(parser);
            break;
        case TK_SWITCH:
            parse_switch(parser);
            break;
        case TK_RETURN:
            parse_return(parser);
            break;
        case TK_WHILE:
            parse_while(parser);
            break;
        case TK_DO:
            parse_do(parser);
            break;
        case TK_IMPORT:
            tea_lex_next(parser->lex);  /* Skip 'import' */
            parse_import(parser);
            break;
        case TK_FROM:
            tea_lex_next(parser->lex);  /* Skip 'from' */
            parse_from_import(parser);
            break;
        case TK_BREAK:
            parse_break(parser);
            break;
        case TK_CONTINUE:
            parse_continue(parser);
            break;
        case TK_NAME:
        {
            if(parser->lex->next.type == ',')
                parse_multiple_assign(parser);
            else
                parse_expr_stmt(parser);
            break;
        }
        case '{':
        {
            tea_lex_next(parser->lex);  /* Skip '{' */
            scope_begin(parser);
            parse_block(parser);
            scope_end(parser);
            break;
        }
        default:
            parse_expr_stmt(parser);
            break;
    }
}

/* Entry point of bytecode parser */
GCproto* tea_parse(Lexer* lexer, bool isexpr)
{
    tea_State* T = lexer->T;

    Parser parser;
    parser_init(lexer, &parser, NULL, PROTO_SCRIPT);

    tea_lex_next(parser.lex);   /* Read the first token into "next" */
    tea_lex_next(parser.lex);   /* Copy "next" -> "curr" */

    if(isexpr)
    {
        expr(&parser);
        bcemit_op(&parser, BC_RETURN);
        lex_consume(&parser, TK_EOF);
    }
    else
    {
        while(!lex_match(&parser, TK_EOF))
        {
            parse_decl(&parser);
        }
        bcemit_return(&parser);
    }

    GCproto* proto = parser_end(&parser);

    if(!T->repl)
    {
        tea_tab_free(T, &T->constants);
    }

    return proto;
}

void tea_parse_mark(tea_State* T, Parser* parser)
{
    tea_gc_markval(T, &parser->lex->prev.value);
    tea_gc_markval(T, &parser->lex->curr.value);
    tea_gc_markval(T, &parser->lex->next.value);

    while(parser != NULL)
    {
        tea_gc_markobj(T, (GCobj*)parser->proto);
        parser = parser->enclosing;
    }
}