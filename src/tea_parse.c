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
#include "tea_map.h"

/* -- Parser structures and definitions ----------------------------------- */

typedef enum
{
    PREC_NONE,
    PREC_ASSIGNMENT, /*  =  */
    PREC_OR,         /*  or  */
    PREC_AND,        /*  and  */
    PREC_EQUALITY,   /*  == !=  */
    PREC_IS,         /*  is  */
    PREC_COMPARISON, /*  < > <= >=  */
    PREC_BOR,        /*  |  */
    PREC_BXOR,       /*  ^  */
    PREC_BAND,       /*  &  */
    PREC_SHIFT,      /*  << >>  */
    PREC_RANGE,      /*  .. ...  */
    PREC_TERM,       /*  + -  */
    PREC_FACTOR,     /*  * /  */
    PREC_INDICES,    /*  **  */
    PREC_UNARY,      /*  not ! - ~  */
    PREC_SUBSCRIPT,  /*  []  */
    PREC_CALL,       /*  . ()  */
    PREC_PRIMARY
} Precedence;

typedef struct
{
    Token name;
    int depth;
    bool is_captured;
    bool isconst;
    bool init;
} Local;

typedef struct
{
    uint8_t index;
    bool islocal;
    bool isconst;
} Upvalue;

typedef struct KlassState
{
    struct KlassState* prev; /* Enclosing class parser */
    bool is_static;
    bool has_superclass;
} KlassState;

typedef struct Loop
{
    struct Loop* prev; /* Enclosing loop */
    int start;
    int body;
    int end;
    int scope_depth;
} Loop;

typedef enum
{
    PROTO_FUNCTION,
    PROTO_ANONYMOUS,
    PROTO_INIT,
    PROTO_METHOD,
    PROTO_OPERATOR,
    PROTO_SCRIPT
} ProtoType;

typedef struct ParseState
{
    GCmap* kt;
    LexState* ls; /* Lexer state */
    tea_State* T;   /* Teascript state */
    struct ParseState* prev;   /* Enclosing parser */
    KlassState* klass; /* Current class parser */
    Loop* loop; /* Current loop context */
    GCproto* proto; /* Current prototype function */
    GCstr* name;    /* Name of prototype function */
    ProtoType type;
    Local locals[TEA_MAX_LOCAL];  /* Current scoped locals */
    int local_count;    /* Number of local variables in scope */
    Upvalue upvalues[TEA_MAX_UPVAL];  /* Saved upvalues */
    int slot_count; /* Stack max size */
    int scope_depth;    /* Current scope depth */
} ParseState;

typedef void (*ParseFn)(ParseState* parser, bool assign);

typedef struct
{
    ParseFn prefix;
    ParseFn infix;
    Precedence prec;
} ParseRule;

/* -- Error handling --------------------------------------------- */

TEA_NORET TEA_NOINLINE static void error(ParseState* ps, ErrMsg em)
{
    tea_lex_error(ps->ls, &ps->ls->prev, em);
}

TEA_NORET TEA_NOINLINE static void error_at_current(ParseState* ps, ErrMsg em)
{
    tea_lex_error(ps->ls, &ps->ls->curr, em);
}

/* -- Lexer support ------------------------------------------------------- */

static Token lex_synthetic(ParseState* ps, const char* text)
{
    Token tok;
    GCstr* s = tea_str_newlen(ps->ls->T, text);
    setstrV(ps->ls->T, &tok.tv, s);
    return tok;
}

static void lex_consume(ParseState* ps, LexToken type)
{
    if(ps->ls->curr.t == type)
    {
        tea_lex_next(ps->ls);
        return;
    }
    const char* tok = tea_lex_token2str(ps->ls, type);
    tea_lex_error(ps->ls, &ps->ls->curr, TEA_ERR_XTOKEN, tok);
}

/* Check for matching token */
static bool lex_check(ParseState* ps, LexToken type)
{
    return ps->ls->curr.t == type;
}

/* Check and consume token */
static bool lex_match(ParseState* ps, LexToken type)
{
    if(!lex_check(ps, type))
        return false;
    tea_lex_next(ps->ls);
    return true;
}

/* -- Management of constants --------------------------------------------- */

static int const_add(tea_State* T, GCproto* f, TValue* o)
{
    copyTV(T, T->top++, o);
    if(f->k_size < f->k_count + 1)
    {
        f->k = tea_mem_growvec(T, TValue, f->k, f->k_size, INT_MAX);
    }
    copyTV(T, proto_kgc(f, f->k_count), o);
    f->k_count++;
    T->top--;
    return f->k_count - 1;
}

static uint8_t const_make(ParseState* ps, TValue* o)
{
    int idx = const_add(ps->ls->T, ps->proto, o);
    if(idx > UINT8_MAX)
    {
        error(ps, TEA_ERR_XKCONST);
    }
    return (uint8_t)idx;
}

/* Add a number constant */
static uint8_t const_num(ParseState* ps, TValue* n)
{
    tea_State* T = ps->T;
    GCmap* kt = ps->kt;
    cTValue* o = tea_map_get(kt, n);
    if(o)
        return (uint8_t)numV(o);

    TValue tv;
    setnumV(&tv, numV(n));
    uint8_t idx = const_make(ps, &tv);
    setnumV(tea_map_set(T, kt, n), idx);
    return idx;
}

/* Add a string constant */
static uint8_t const_str(ParseState* ps, GCstr* str)
{
    tea_State* T = ps->T;
    GCmap* kt = ps->kt;
    cTValue* o = tea_map_getstr(T, kt, str);
    if(o)
        return (uint8_t)numV(o);

    TValue tv;
    setstrV(T, &tv, str);
    uint8_t idx = const_make(ps, &tv);
    setnumV(tea_map_setstr(T, kt, str), idx);
    return idx;
}

/* -- Bytecode emitter ---------------------------------------------------- */

static void bcemit_byte(ParseState* ps, uint8_t byte)
{
    tea_State* T = ps->ls->T;
    GCproto* f = ps->proto;
    int line = ps->ls->prev.line;

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

static void bcemit_bytes(ParseState* ps, uint8_t byte1, uint8_t byte2)
{
    bcemit_byte(ps, byte1);
    bcemit_byte(ps, byte2);
}

static const int stack_effects[] = {
#define BCEFFECT(_, effect) effect,
    BCDEF(BCEFFECT)
#undef BCEFFECT
};

/* Emit bytecode instruction */
static void bcemit_op(ParseState* ps, BCOp op)
{
    bcemit_byte(ps, op);
    ps->slot_count += stack_effects[op];
    if(ps->slot_count > ps->proto->max_slots)
    {
        ps->proto->max_slots = ps->slot_count;
    }
}

/* Emit 2 bytecode instructions */
static void bcemit_ops(ParseState* ps, BCOp op1, BCOp op2)
{
    bcemit_bytes(ps, op1, op2);
    ps->slot_count += stack_effects[op1] + stack_effects[op2];
    if(ps->slot_count > ps->proto->max_slots)
    {
        ps->proto->max_slots = ps->slot_count;
    }
}

static void bcemit_argued(ParseState* ps, BCOp op, uint8_t byte)
{
    bcemit_bytes(ps, op, byte);
    ps->slot_count += stack_effects[op];
    if(ps->slot_count > ps->proto->max_slots)
    {
        ps->proto->max_slots = ps->slot_count;
    }
}

static void bcemit_loop(ParseState* ps, int loop_start)
{
    bcemit_op(ps, BC_LOOP);
    int ofs = ps->proto->bc_count - loop_start + 2;
    if(ofs > UINT16_MAX)
        error(ps, TEA_ERR_XLOOP);
    bcemit_byte(ps, (ofs >> 8) & 0xff);
    bcemit_byte(ps, ofs & 0xff);
}

static int bcemit_jump(ParseState* ps, BCOp op)
{
    bcemit_op(ps, op);
    bcemit_bytes(ps, 0xff, 0xff);
    return ps->proto->bc_count - 2;
}

static void bcemit_return(ParseState* ps)
{
    if(ps->type == PROTO_INIT)
    {
        bcemit_argued(ps, BC_GET_LOCAL, 0);
    }
    else
    {
        bcemit_op(ps, BC_NIL);
    }
    bcemit_op(ps, BC_RETURN);
}

static void bcemit_invoke(ParseState* ps, int args, const char* name)
{
    GCstr* str = tea_str_new(ps->T, name, strlen(name));
    bcemit_argued(ps, BC_INVOKE, const_str(ps, str));
    bcemit_byte(ps, args);
}

static void bcemit_num(ParseState* ps, TValue* o)
{
    bcemit_argued(ps, BC_CONSTANT, const_num(ps, o));
}

static void bcemit_str(ParseState* ps, TValue* o)
{
    bcemit_argued(ps, BC_CONSTANT, const_str(ps, strV(o)));
}

static void bcpatch_jump(ParseState* ps, int ofs)
{
    /* -2 to adjust for the bytecode for the jump offset itself */
    int jmp = ps->proto->bc_count - ofs - 2;
    if(jmp > UINT16_MAX)
    {
        error(ps, TEA_ERR_XJUMP);
    }
    ps->proto->bc[ofs] = (jmp >> 8) & 0xff;
    ps->proto->bc[ofs + 1] = jmp & 0xff;
}

/* -- Parser state management ------------------------------------------- */

static void parser_init(LexState* ls, ParseState* ps, ParseState* parent, ProtoType type)
{
    tea_State* T = ls->T;
    ps->ls = ls;
    ps->T = T;
    ps->prev = parent;
    ps->proto = NULL;
    ps->klass = NULL;
    ps->loop = NULL;
    if(parent != NULL)
    {
        ps->kt = parent->kt;
        ps->klass = parent->klass;
    }
    ps->type = type;
    ps->local_count = 1;
    ps->slot_count = ps->local_count;
    ps->scope_depth = 0;
    T->parser = ps;
    ps->proto = tea_func_newproto(T, ps->slot_count);
    ps->kt = tea_map_new(T);
    /* Anchor table of constants in stack to avoid being collected */
    setmapV(T, T->top, ps->kt);
    incr_top(T);

    switch(type)
    {
        case PROTO_FUNCTION:
        case PROTO_INIT:
        case PROTO_METHOD:
            ps->proto->name = strV(&ps->ls->prev.tv);
            break;
        case PROTO_OPERATOR:
            ps->proto->name = ps->prev->name;
            break;
        case PROTO_ANONYMOUS:
            ps->proto->name = tea_str_newlit(T, "<anonymous>");
            break;
        case PROTO_SCRIPT:
            ps->proto->name = tea_str_newlit(T, "<script>");
            break;
        default:
            break;
    }

    Local* local = &ps->locals[0];
    local->depth = 0;
    local->is_captured = false;

    GCstr* s;
    switch(type)
    {
        case PROTO_SCRIPT:
        case PROTO_FUNCTION:
        case PROTO_ANONYMOUS:
            s = &T->strempty;
            setstrV(T, &local->name.tv, s);
            break;
        case PROTO_INIT:
        case PROTO_METHOD:
        case PROTO_OPERATOR:
            s = tea_str_newlit(T, "this");
            setstrV(T, &local->name.tv, s);
            break;
        default:
            break;
    }
}

static GCproto* parser_end(ParseState* ps)
{
    GCproto* proto = ps->proto;
    if(ps->prev != NULL)
    {
        TValue o;
        setprotoV(ps->ls->T, &o, proto);
        bcemit_argued(ps->prev, BC_CLOSURE, const_make(ps->prev, &o));

        for(int i = 0; i < proto->upvalue_count; i++)
        {
            bcemit_byte(ps->prev, ps->upvalues[i].islocal ? 1 : 0);
            bcemit_byte(ps->prev, ps->upvalues[i].index);
        }
    }
    ps->ls->T->top--;   /* Pop table of constants */
    ps->ls->T->parser = ps->prev;
    return proto;
}

/* -- Scope handling ------------------------------------------------------ */

/* Begin a scope */
static void scope_begin(ParseState* ps)
{
    ps->scope_depth++;
}

static int discard_locals(ParseState* ps, int depth)
{
    int local;
    for(local = ps->local_count - 1; local >= 0 && ps->locals[local].depth >= depth; local--)
    {
        if(ps->locals[local].is_captured)
        {
            bcemit_op(ps, BC_CLOSE_UPVALUE);
        }
        else
        {
            bcemit_op(ps, BC_POP);
        }
    }
    return ps->local_count - local - 1;
}

/* End a scope */
static void scope_end(ParseState* ps)
{
    int effect = discard_locals(ps, ps->scope_depth);
    ps->local_count -= effect;
    ps->slot_count -= effect;
    ps->scope_depth--;
}

/* Forward declarations */
static void expr(ParseState* ps);
static void expr_arrow(ParseState* ps);
static void parse_stmt(ParseState* ps);
static void parse_decl(ParseState* ps, bool export);
static ParseRule expr_rule(int type);
static void expr_precedence(ParseState* ps, Precedence precedence);
static void expr_anonymous(ParseState* ps, bool assign);
static void expr_grouping(ParseState* ps, bool assign);
static void parse_block(ParseState* ps);

/* -- Variable handling --------------------------------------------------- */

static int var_lookup_local(ParseState* ps, Token* name)
{
    for(int i = ps->local_count - 1; i >= 0; i--)
    {
        Local* local = &ps->locals[i];
        if(strV(&name->tv) == strV(&local->name.tv))
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

static int var_add_uv(ParseState* ps, uint8_t index, bool islocal, bool isconst)
{
    int upvalue_count = ps->proto->upvalue_count;

    for(int i = 0; i < upvalue_count; i++)
    {
        Upvalue* upvalue = &ps->upvalues[i];
        if(upvalue->index == index && upvalue->islocal == islocal)
        {
            return i;
        }
    }

    if(upvalue_count == TEA_MAX_UPVAL)
    {
        error(ps, TEA_ERR_XUPVAL);
    }

    ps->upvalues[upvalue_count].islocal = islocal;
    ps->upvalues[upvalue_count].index = index;
    ps->upvalues[upvalue_count].isconst = isconst;

    return ps->proto->upvalue_count++;
}

static int var_lookup_uv(ParseState* ps, Token* name)
{
    if(ps->prev == NULL)
        return -1;

    bool isconst;
    int local = var_lookup_local(ps->prev, name);
    if(local != -1)
    {
        isconst = ps->prev->locals[local].isconst;
        ps->prev->locals[local].is_captured = true;
        return var_add_uv(ps, (uint8_t)local, true, isconst);
    }

    int upvalue = var_lookup_uv(ps->prev, name);
    if(upvalue != -1)
    {
        isconst = ps->prev->upvalues[upvalue].isconst;
        return var_add_uv(ps, (uint8_t)upvalue, false, isconst);
    }

    return -1;
}

static void var_mark(ParseState* ps, bool isconst)
{
    if(ps->scope_depth == 0) return;
    ps->locals[ps->local_count - 1].depth = ps->scope_depth;
    ps->locals[ps->local_count - 1].isconst = isconst;
}

static int var_add_local(ParseState* ps, Token name)
{
    if(ps->local_count == TEA_MAX_LOCAL)
    {
        error(ps, TEA_ERR_XLOCALS);
    }
    int found = var_lookup_local(ps, &name);
    if(found != -1 && ps->locals[found].init && 
        ps->locals[found].depth == ps->scope_depth)
    {
        GCstr* name = strV(&ps->locals[found].name.tv);
        tea_lex_error(ps->ls, &ps->ls->prev, TEA_ERR_XDECL, str_data(name));
    }
    Local* local = &ps->locals[ps->local_count++];
    local->name = name;
    local->depth = -1;
    local->is_captured = false;
    local->isconst = false;
    local->init = true;
    return ps->local_count - 1;
}

static void var_declare(ParseState* ps, Token* name)
{
    if(ps->scope_depth == 0)
        return;
    var_add_local(ps, *name);
}

static void var_define(ParseState* ps, Token* name, bool isconst, uint8_t export)
{
    if(ps->scope_depth > 0)
    {
        var_mark(ps, isconst);
        return;
    }
    GCstr* str = strV(&(name->tv));
    uint8_t k = const_str(ps, str);
    if(isconst)
    {
        TValue* o = tea_tab_set(ps->ls->T, &ps->ls->T->constants, str, NULL);
        setnilV(o);
    }
    bcemit_argued(ps, BC_DEFINE_MODULE, k);
    bcemit_byte(ps, export);
}

/* -- Expressions --------------------------------------------------------- */

static void expr_and(ParseState* ps, bool assign)
{
    int jmp = bcemit_jump(ps, BC_JUMP_IF_FALSE);
    bcemit_op(ps, BC_POP);
    expr_precedence(ps, PREC_AND);
    bcpatch_jump(ps, jmp);
}

static void expr_binary(ParseState* ps, bool assign)
{
    LexToken op = ps->ls->prev.t;
    if(op == TK_NOT)
    {
        lex_consume(ps, TK_IN);

        ParseRule rule = expr_rule(op);
        expr_precedence(ps, (Precedence)(rule.prec + 1));

        bcemit_ops(ps, BC_IN, BC_NOT);
        return;
    }

    if(op == TK_IS && lex_match(ps, TK_NOT))
    {
        ParseRule rule = expr_rule(op);
        expr_precedence(ps, (Precedence)(rule.prec + 1));

        bcemit_ops(ps, BC_IS, BC_NOT);
        return;
    }

    ParseRule rule = expr_rule(op);
    expr_precedence(ps, (Precedence)(rule.prec + 1));
    switch(op)
    {
        case TK_BANG_EQUAL:
            bcemit_ops(ps, BC_EQUAL, BC_NOT);
            break;
        case TK_EQUAL_EQUAL:
            bcemit_op(ps, BC_EQUAL);
            break;
        case TK_IS:
            bcemit_op(ps, BC_IS);
            break;
        case '>':
            bcemit_op(ps, BC_GREATER);
            break;
        case TK_GREATER_EQUAL:
            bcemit_op(ps, BC_GREATER_EQUAL);
            break;
        case '<':
            bcemit_op(ps, BC_LESS);
            break;
        case TK_LESS_EQUAL:
            bcemit_op(ps, BC_LESS_EQUAL);
            break;
        case '+':
            bcemit_op(ps, BC_ADD);
            break;
        case '-':
            bcemit_op(ps, BC_SUBTRACT);
            break;
        case '*':
            bcemit_op(ps, BC_MULTIPLY);
            break;
        case '/':
            bcemit_op(ps, BC_DIVIDE);
            break;
        case '%':
            bcemit_op(ps, BC_MOD);
            break;
        case TK_STAR_STAR:
            bcemit_op(ps, BC_POW);
            break;
        case '&':
            bcemit_op(ps, BC_BAND);
            break;
        case '|':
            bcemit_op(ps, BC_BOR);
            break;
        case '^':
            bcemit_op(ps, BC_BXOR);
            break;
        case TK_GREATER_GREATER:
            bcemit_op(ps, BC_RSHIFT);
            break;
        case TK_LESS_LESS:
            bcemit_op(ps, BC_LSHIFT);
            break;
        case TK_IN:
            bcemit_op(ps, BC_IN);
            break;
        default: 
            return; /* Unreachable */
    }
}

static void expr_ternary(ParseState* ps, bool assign)
{
    /* Jump to else branch if the condition is false */
    int else_jmp = bcemit_jump(ps, BC_JUMP_IF_FALSE);

    /* Pop the condition */
    bcemit_op(ps, BC_POP);
    expr(ps);

    int end_jmp = bcemit_jump(ps, BC_JUMP);

    bcpatch_jump(ps, else_jmp);
    bcemit_op(ps, BC_POP);

    lex_consume(ps, ':');
    expr(ps);

    bcpatch_jump(ps, end_jmp);
}

static uint8_t parse_arg_list(ParseState* ps)
{
    uint8_t nargs = 0;
    if(!lex_check(ps, ')'))
    {
        do
        {
            expr(ps);
            if(nargs == 255)
            {
                error(ps, TEA_ERR_XARGS);
            }
            nargs++;
        }
        while(lex_match(ps, ','));
    }
    lex_consume(ps, ')');
    return nargs;
}

static void expr_call(ParseState* ps, bool assign)
{
    uint8_t nargs = parse_arg_list(ps);
    bcemit_argued(ps, BC_CALL, nargs);
}

static int bcassign(ParseState* ps)
{
    switch(ps->ls->curr.t)
    {
        case TK_PLUS_EQUAL:
            return BC_ADD;
        case TK_MINUS_EQUAL:
            return BC_SUBTRACT;
        case TK_STAR_EQUAL:
            return BC_MULTIPLY;
        case TK_SLASH_EQUAL:
            return BC_DIVIDE;
        case TK_PERCENT_EQUAL:
            return BC_MOD;
        case TK_STAR_STAR_EQUAL:
            return BC_POW;
        case TK_AMPERSAND_EQUAL:
            return BC_BAND;
        case TK_PIPE_EQUAL:
            return BC_BOR;
        case TK_CARET_EQUAL:
            return BC_BXOR;
        case TK_LESS_LESS_EQUAL:
            return BC_LSHIFT;
        case TK_GREATER_GREATER_EQUAL:
            return BC_RSHIFT;
        default:
            return 0;
    }
}

static void expr_dot(ParseState* ps, bool assign)
{
    bool isnew = false;
    if(lex_match(ps, TK_NEW))
    {
        isnew = true;
    }
    else
    {
        lex_consume(ps, TK_NAME);
    }
    uint8_t name = const_str(ps, strV(&ps->ls->prev.tv));

    if(lex_match(ps, '('))
    {
        uint8_t nargs = parse_arg_list(ps);
        if(isnew)
        {
            bcemit_argued(ps, BC_INVOKE_NEW, nargs);
        }
        else
        {
            bcemit_argued(ps, BC_INVOKE, name);
            bcemit_byte(ps, nargs);
        }
        return;
    }

    int bc;
    if(assign && lex_match(ps, '='))
    {
        expr(ps);
        bcemit_argued(ps, BC_SET_ATTR, name);
    }
    else if(assign && (bc = bcassign(ps)))
    {
        tea_lex_next(ps->ls);
        bcemit_argued(ps, BC_PUSH_ATTR, name);
        expr(ps);
        bcemit_op(ps, bc);
        bcemit_argued(ps, BC_SET_ATTR, name);
    }
    else
    {
        if(lex_match(ps, TK_PLUS_PLUS)) bc = BC_ADD;
        else if(lex_match(ps, TK_MINUS_MINUS)) bc = BC_SUBTRACT;
        else bc = 0;
        if(bc)
        {
            bcemit_argued(ps, BC_PUSH_ATTR, name);
            TValue _v;
            setnumV(&_v, 1);
            bcemit_num(ps, &_v);
            bcemit_op(ps, bc);
            bcemit_argued(ps, BC_SET_ATTR, name);
        }
        else
        {
            bcemit_argued(ps, BC_GET_ATTR, name);
        }
    }
}

static void expr_bool(ParseState* ps, bool assign)
{
    bcemit_op(ps, ps->ls->prev.t == TK_FALSE ? BC_FALSE : BC_TRUE);
}

static void expr_nil(ParseState* ps, bool assign)
{
    bcemit_op(ps, BC_NIL);
}

static void expr_list(ParseState* ps, bool assign)
{
    bcemit_op(ps, BC_LIST);
    if(!lex_check(ps, ']'))
    {
        do
        {
            /* Traling comma */
            if(lex_check(ps, ']'))
                break;

            if(lex_match(ps, TK_DOT_DOT_DOT))
            {
                expr(ps);
                bcemit_op(ps, BC_LIST_EXTEND);
                continue;
            }

            expr(ps);
            bcemit_op(ps, BC_LIST_ITEM);
        }
        while(lex_match(ps, ','));
    }
    lex_consume(ps, ']');
}

static void expr_map(ParseState* ps, bool assign)
{
    bcemit_op(ps, BC_MAP);
    if(!lex_check(ps, '}'))
    {
        do
        {
            /* Traling comma */
            if(lex_check(ps, '}'))
                break;

            if(lex_match(ps, '['))
            {
                expr(ps);
                lex_consume(ps, ']');
                lex_consume(ps, '=');
                expr(ps);
            }
            else
            {
                lex_consume(ps, TK_NAME);
                bcemit_str(ps, &ps->ls->prev.tv);
                lex_consume(ps, '=');
                expr(ps);
            }
            bcemit_op(ps, BC_MAP_FIELD);
        }
        while(lex_match(ps, ','));
    }
    lex_consume(ps, '}');
}

static bool parse_slice(ParseState* ps)
{
    expr(ps);
    /* It's a slice */
    if(lex_match(ps, ':'))
    {
        TValue tv1, tv2;
        /* [n:] */
        if(lex_check(ps, ']'))
        {
            setnumV(&tv1, INFINITY);
            setnumV(&tv2, 1);
            bcemit_num(ps, &tv1);
            bcemit_num(ps, &tv2);
        }
        else
        {
            /* [n::n] */
            if(lex_match(ps, ':'))
            {
                setnumV(&tv1, INFINITY);
                bcemit_num(ps, &tv1);
                expr(ps);
            }
            else
            {
                expr(ps);
                if(lex_match(ps, ':'))
                {
                    /* [n:n:n] */
                    expr(ps);
                }
                else
                {
                    setnumV(&tv1, 1);
                    bcemit_num(ps, &tv1);
                }
            }
        }
        return true;
    }
    return false;
}

static void expr_subscript(ParseState* ps, bool assign)
{
    if(parse_slice(ps))
    {
        bcemit_op(ps, BC_RANGE);
    }

    lex_consume(ps, ']');

    int bc;
    if(assign && lex_match(ps, '='))
    {
        expr(ps);
        bcemit_op(ps, BC_SET_INDEX);
    }
    else if(assign && (bc = bcassign(ps)))
    {
        tea_lex_next(ps->ls);
        bcemit_op(ps, BC_PUSH_INDEX);
        expr(ps);
        bcemit_op(ps, bc);
        bcemit_op(ps, BC_SET_INDEX);
    }
    else
    {
        if(lex_match(ps, TK_PLUS_PLUS)) bc = BC_ADD;
        else if(lex_match(ps, TK_MINUS_MINUS)) bc = BC_SUBTRACT;
        else bc = 0;
        if(bc)
        {
            bcemit_op(ps, BC_PUSH_INDEX);
            TValue _v;
            setnumV(&_v, 1);
            bcemit_num(ps, &_v);
            bcemit_op(ps, bc);
            bcemit_op(ps, BC_SET_INDEX);
        }
        else
        {
            bcemit_op(ps, BC_GET_INDEX);
        }
    }
}

static void expr_or(ParseState* ps, bool assign)
{
    int else_jmp = bcemit_jump(ps, BC_JUMP_IF_FALSE);
    int jmp = bcemit_jump(ps, BC_JUMP);
    bcpatch_jump(ps, else_jmp);
    bcemit_op(ps, BC_POP);
    expr_precedence(ps, PREC_OR);
    bcpatch_jump(ps, jmp);
}

static void expr_number(ParseState* ps, bool assign)
{
    bcemit_num(ps, &ps->ls->prev.tv);
}

static void expr_str(ParseState* ps, bool assign)
{
    tea_State* T = ps->T;
    TValue tv;
    copyTV(T, &tv, &ps->ls->prev.tv);
    if((ps->ls->curr.t == '+') &&
        (ps->ls->next.t == TK_STRING))
    {
        SBuf* sb = tea_buf_tmp_(T);
        tea_buf_putstr(T, sb, strV(&tv));
        while((ps->ls->curr.t == '+') && (ps->ls->next.t == TK_STRING)) 
        {
            TValue* o = &ps->ls->next.tv;
            GCstr* s2 = strV(o);
            tea_buf_putstr(T, sb, s2);
            tea_lex_next(ps->ls);
            tea_lex_next(ps->ls);
        }
        GCstr* str = tea_buf_str(T, sb);
        setstrV(T, &tv, str);
    }
    bcemit_str(ps, &tv);
}

static void expr_interpolation(ParseState* ps, bool assign)
{
    bcemit_op(ps, BC_LIST);
    do
    {
        bcemit_str(ps, &ps->ls->prev.tv);
        bcemit_op(ps, BC_LIST_ITEM);

        expr(ps);
        bcemit_op(ps, BC_LIST_ITEM);
    }
    while(lex_match(ps, TK_INTERPOLATION));

    lex_consume(ps, TK_STRING);
    bcemit_str(ps, &ps->ls->prev.tv);
    bcemit_op(ps, BC_LIST_ITEM);
    bcemit_invoke(ps, 0, "join");
}

static void check_const(ParseState* ps, uint8_t set_op, int arg)
{
    switch(set_op)
    {
        case BC_SET_LOCAL:
        {
            if(ps->locals[arg].isconst)
            {
                error(ps, TEA_ERR_XVCONST);
            }
            break;
        }
        case BC_SET_UPVALUE:
        {
            if(ps->upvalues[arg].isconst)
            {
                error(ps, TEA_ERR_XVCONST);
            }
            break;
        }
        case BC_SET_MODULE:
        {
            GCstr* string = strV(proto_kgc(ps->proto, arg));
            if(tea_tab_get(&ps->ls->T->constants, string))
            {
                error(ps, TEA_ERR_XVCONST);
            }
            break;
        }
        default:
            break;
    }
}

static void named_variable(ParseState* ps, Token name, bool assign)
{
    uint8_t get_op, set_op;
    int arg = var_lookup_local(ps, &name);
    if(arg != -1)
    {
        get_op = BC_GET_LOCAL;
        set_op = BC_SET_LOCAL;
    }
    else if((arg = var_lookup_uv(ps, &name)) != -1)
    {
        get_op = BC_GET_UPVALUE;
        set_op = BC_SET_UPVALUE;
    }
    else
    {
        arg = const_str(ps, strV(&name.tv));
        get_op = BC_GET_MODULE;
        set_op = BC_SET_MODULE;
    }

    int bc;
    if(assign && lex_match(ps, '='))
    {
        check_const(ps, set_op, arg);
        expr(ps);
        bcemit_argued(ps, set_op, (uint8_t)arg);
    }
    else if(assign && (bc = bcassign(ps)))
    {
        tea_lex_next(ps->ls);
        check_const(ps, set_op, arg);
        bcemit_argued(ps, get_op, (uint8_t)arg);
        expr(ps);
        bcemit_op(ps, bc);
        bcemit_argued(ps, set_op, (uint8_t)arg);
    }
    else
    {
        if(lex_match(ps, TK_PLUS_PLUS)) bc = BC_ADD;
        else if(lex_match(ps, TK_MINUS_MINUS)) bc = BC_SUBTRACT;
        else bc = 0;
        if(bc)
        {
            check_const(ps, set_op, arg);
            bcemit_argued(ps, get_op, (uint8_t)arg);
            TValue _v;
            setnumV(&_v, 1);
            bcemit_num(ps, &_v);
            bcemit_op(ps, bc);
            bcemit_argued(ps, set_op, (uint8_t)arg);
        }
        else
        {
            bcemit_argued(ps, get_op, (uint8_t)arg);
        }
    }
}

static void expr_name(ParseState* ps, bool assign)
{
    Token name = ps->ls->prev;

    if(lex_check(ps, TK_ARROW))
    {
        ParseState pps;
        parser_init(ps->ls, &pps, ps, PROTO_ANONYMOUS);
        pps.proto->numparams = 1;
        var_declare(&pps, &name);
        var_define(&pps, &name, false, false);
        expr_arrow(&pps);
        return;
    }

    named_variable(ps, name, assign);
}

static void expr_super(ParseState* ps, bool assign)
{
    if(ps->klass == NULL)
    {
        error(ps, TEA_ERR_XSUPERO);
    }
    else if(ps->klass->is_static)
    {
        error(ps, TEA_ERR_XTHISM);
    }
    else if(!ps->klass->has_superclass)
    {
        error(ps, TEA_ERR_XSUPERK);
    }

    /* super */
    if(!lex_check(ps, '(') && !lex_check(ps, '.'))
    {
        named_variable(ps, lex_synthetic(ps, "super"), false);
        return;
    }

    /* super() -> super.init() */
    if(lex_match(ps, '('))
    {
        Token tok = lex_synthetic(ps, "new");
        uint8_t name = const_str(ps, strV(&tok.tv));
        named_variable(ps, lex_synthetic(ps, "this"), false);
        uint8_t nargs = parse_arg_list(ps);
        named_variable(ps, lex_synthetic(ps, "super"), false);
        bcemit_argued(ps, BC_SUPER, name);
        bcemit_byte(ps, nargs);
        return;
    }

    /* super.name */
    lex_consume(ps, '.');
    lex_consume(ps, TK_NAME);
    uint8_t name = const_str(ps, strV(&ps->ls->prev.tv));

    named_variable(ps, lex_synthetic(ps, "this"), false);

    if(lex_match(ps, '('))
    {
        /* super.name() */
        uint8_t nargs = parse_arg_list(ps);
        named_variable(ps, lex_synthetic(ps, "super"), false);
        bcemit_argued(ps, BC_SUPER, name);
        bcemit_byte(ps, nargs);
    }
    else
    {
        /* super.name */
        named_variable(ps, lex_synthetic(ps, "super"), false);
        bcemit_argued(ps, BC_GET_SUPER, name);
    }
}

static void expr_this(ParseState* ps, bool assign)
{
    if(ps->klass == NULL)
    {
        error(ps, TEA_ERR_XTHISO);
    }
    else if(ps->klass->is_static)
    {
        error(ps, TEA_ERR_XTHISM);
    }
    expr_name(ps, false);
}

static void expr_unary(ParseState* ps, bool assign)
{
    int op = ps->ls->prev.t;
    expr_precedence(ps, PREC_UNARY);
    switch(op)
    {
        case TK_NOT:
        case '!':
            bcemit_op(ps, BC_NOT);
            break;
        case '-':
            bcemit_op(ps, BC_NEGATE);
            break;
        case '~':
            bcemit_op(ps, BC_BNOT);
            break;
        default:
            return; /* Unreachable */
    }
}

static void expr_range(ParseState* ps, bool assign)
{
    LexToken op_type = ps->ls->prev.t;
    ParseRule rule = expr_rule(op_type);
    expr_precedence(ps, (Precedence)(rule.prec + 1));
    if(lex_match(ps, TK_DOT_DOT))
    {
        expr(ps);
    }
    else
    {
        TValue tv;
        setnumV(&tv, 1);
        bcemit_num(ps, &tv);
    }
    bcemit_op(ps, BC_RANGE);
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
            return PREFIX(expr_number);
        case TK_STRING:
            return PREFIX(expr_str);
        case TK_INTERPOLATION:
            return PREFIX(expr_interpolation);
        case TK_AND:
            return OPERATOR(expr_and, PREC_AND);
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

static void expr_precedence(ParseState* ps, Precedence prec)
{
    tea_lex_next(ps->ls);
    ParseFn prefix_rule = expr_rule(ps->ls->prev.t).prefix;
    if(prefix_rule == NULL)
    {
        error(ps, TEA_ERR_XEXPR);
    }

    bool assign = prec <= PREC_ASSIGNMENT;
    prefix_rule(ps, assign);

    while(prec <= expr_rule(ps->ls->curr.t).prec)
    {
        tea_lex_next(ps->ls);
        ParseFn infix_rule = expr_rule(ps->ls->prev.t).infix;
        infix_rule(ps, assign);
    }

    if(assign && lex_match(ps, '='))
    {
        error(ps, TEA_ERR_XASSIGN);
    }
}

static void expr(ParseState* ps)
{
    expr_precedence(ps, PREC_ASSIGNMENT);
}

/* Parse a block */
static void parse_block(ParseState* ps)
{
    while(!lex_check(ps, '}') && !lex_check(ps, TK_EOF))
    {
        parse_decl(ps, false);
    }
    lex_consume(ps, '}');
}

static void check_parameters(ParseState* ps, Token* name)
{
    for(int i = ps->local_count - 1; i >= 0; i--)
    {
        Local* local = &ps->locals[i];
        if(strV(&name->tv) == strV(&local->name.tv))
        {
            error(ps, TEA_ERR_XDUPARGS);
        }
    }
}

static void begin_function(ParseState* ps)
{
    scope_begin(ps);
    if(!lex_check(ps, ')'))
    {
        bool optional = false;
        bool spread = false;

        do
        {
            if(spread)
            {
                error(ps, TEA_ERR_XSPREADARGS);
            }

            spread = lex_match(ps, TK_DOT_DOT_DOT);
            lex_consume(ps, TK_NAME);

            Token name = ps->ls->prev;
            check_parameters(ps, &name);

            if(spread)
            {
                ps->proto->variadic = spread;
            }

            if(lex_match(ps, '='))
            {
                if(spread)
                {
                    error(ps, TEA_ERR_XSPREADOPT);
                }
                ps->proto->numopts++;
                optional = true;
                expr(ps);
            }
            else
            {
                ps->proto->numparams++;
                if(optional)
                {
                    error(ps, TEA_ERR_XOPT);
                }
            }

            if(ps->proto->numparams + ps->proto->numopts > 255)
            {
                error(ps, TEA_ERR_XMAXARGS);
            }

            var_declare(ps, &name);
            var_define(ps, &name, false, false);
        }
        while(lex_match(ps, ','));

        if(ps->proto->numopts > 0)
        {
            bcemit_op(ps, BC_DEFINE_OPTIONAL);
            bcemit_bytes(ps, ps->proto->numparams, ps->proto->numopts);
        }
    }
    lex_consume(ps, ')');
}

static void function(ParseState* ps)
{
    lex_consume(ps, '(');
    begin_function(ps);
    lex_consume(ps, '{');
    parse_block(ps);
    bcemit_return(ps);
    parser_end(ps);
}

static void expr_anonymous(ParseState* ps, bool assign)
{
    ParseState pps;
    parser_init(ps->ls, &pps, ps, PROTO_FUNCTION);
    function(&pps);
}

static void expr_arrow(ParseState* ps)
{
    lex_consume(ps, TK_ARROW);
    if(lex_match(ps, '{'))
    {
        /* Brace so expect a block */
        parse_block(ps);
        bcemit_return(ps);
    }
    else
    {
        /* No brace, so expect single expression */
        expr(ps);
        bcemit_op(ps, BC_RETURN);
    }
    parser_end(ps);
}

static void expr_grouping(ParseState* ps, bool assign)
{
    LexToken curr = ps->ls->curr.t;
    LexToken next = ps->ls->next.t;

    /* () => ...; (...v) => ... */
    /* (a) => ...; (a, ) => ... */
    if((curr == ')' && curr == TK_DOT_DOT_DOT) || 
        (curr == TK_NAME && (next == ',' || next == ')')) || 
        (curr == ')' && next == TK_ARROW))
    {
        ParseState pps;
        parser_init(ps->ls, &pps, ps, PROTO_ANONYMOUS);
        begin_function(&pps);
        expr_arrow(&pps);
        return;
    }

    expr(ps);
    lex_consume(ps, ')');
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

static void parse_operator(ParseState* ps)
{
    int i = 0;
    while(ops[i] != TK_EOF)
    {
        if(lex_match(ps, ops[i]))
            break;
        i++;
    }

    if(i == SENTINEL)
    {
        error_at_current(ps, TEA_ERR_XMETHOD);
    }

    GCstr* name = NULL;

    if(ps->ls->prev.t == '[')
    {
        ps->klass->is_static = false;
        lex_consume(ps, ']');
        if(lex_match(ps, '='))
            name = mmname_str(ps->ls->T, MM_SETINDEX);
        else
            name = mmname_str(ps->ls->T, MM_GETINDEX);
    }
    else
    {
        ps->klass->is_static = true;
        name = mmname_str(ps->ls->T, i);
    }

    TValue tv;
    setstrV(ps->ls->T, &tv, name);
    uint8_t k = const_str(ps, strV(&tv));

    ps->name = name;
    ParseState pps;
    parser_init(ps->ls, &pps, ps, PROTO_OPERATOR);
    function(&pps);
    bcemit_argued(ps, BC_METHOD, k);
    ps->klass->is_static = false;
}

static void parse_method(ParseState* ps, ProtoType type)
{
    uint8_t k = const_str(ps, strV(&ps->ls->prev.tv));
    if(strV(&ps->ls->prev.tv) == ps->ls->T->init_str)
    {
        type = PROTO_INIT;
    }
    ParseState pps;
    parser_init(ps->ls, &pps, ps, type);
    function(&pps);
    bcemit_argued(ps, BC_METHOD, k);
}

static void parserclass_init(ParseState* ps, KlassState* ks)
{
    ks->is_static = false;
    ks->has_superclass = false;
    ks->prev = ps->klass;
    ps->klass = ks;
}

static void parse_class_body(ParseState* ps)
{
    while(!lex_check(ps, '}') && !lex_check(ps, TK_EOF))
    {
        if(lex_match(ps, TK_NAME) || lex_match(ps, TK_NEW))
        {
            parse_method(ps, PROTO_METHOD);
        }
        else
        {
            parse_operator(ps);
        }
    }
}

/* Parse 'class' declaration */
static void parse_class(ParseState* ps, bool export)
{
    tea_lex_next(ps->ls);  /* Skip 'class' */
    lex_consume(ps, TK_NAME);
    Token class_name = ps->ls->prev;
    uint8_t k = const_str(ps, strV(&class_name.tv));
    var_declare(ps, &class_name);

    bcemit_argued(ps, BC_CLASS, k);
    var_define(ps, &class_name, false, export);

    KlassState ks;
    parserclass_init(ps, &ks);

    if(lex_match(ps, ':'))
    {
        expr(ps);

        scope_begin(ps);
        var_add_local(ps, lex_synthetic(ps, "super"));
        var_define(ps, 0, false, false);

        named_variable(ps, class_name, false);
        bcemit_op(ps, BC_INHERIT);
        ks.has_superclass = true;
    }

    named_variable(ps, class_name, false);

    lex_consume(ps, '{');
    parse_class_body(ps);
    lex_consume(ps, '}');

    bcemit_op(ps, BC_POP);

    if(ks.has_superclass)
    {
        scope_end(ps);
    }

    ps->klass = ps->klass->prev;
}

static void parse_function_assign(ParseState* ps)
{
    ParseState pps;
    if(lex_match(ps, '.'))
    {
        lex_consume(ps, TK_NAME);
        uint8_t k = const_str(ps, strV(&ps->ls->prev.tv));
        if(!lex_check(ps, '('))
        {
            bcemit_argued(ps, BC_GET_ATTR, k);
            parse_function_assign(ps);
        }
        else
        {
            parser_init(ps->ls, &pps, ps, PROTO_FUNCTION);
            function(&pps);
            bcemit_argued(ps, BC_SET_ATTR, k);
            bcemit_op(ps, BC_POP);
            return;
        }
    }
    else if(lex_match(ps, ':'))
    {
        lex_consume(ps, TK_NAME);
        uint8_t k = const_str(ps, strV(&ps->ls->prev.tv));

        KlassState ks;
        parserclass_init(ps, &ks);

        parser_init(ps->ls, &pps, ps, PROTO_METHOD);
        function(&pps);

        ps->klass = ps->klass->prev;

        bcemit_argued(ps, BC_EXTENSION_METHOD, k);
        bcemit_op(ps, BC_POP);
        return;
    }
}

/* Parse 'function' declaration */
static void parse_function(ParseState* ps, bool export)
{
    tea_lex_next(ps->ls);  /* Skip 'function' */
    lex_consume(ps, TK_NAME);
    Token name = ps->ls->prev;

    if((lex_check(ps, '.') || lex_check(ps, ':')) && !export)
    {
        named_variable(ps, name, false);
        parse_function_assign(ps);
        return;
    }

    var_declare(ps, &name);
    var_mark(ps, false);
    ParseState pps;
    parser_init(ps->ls, &pps, ps, PROTO_FUNCTION);
    function(&pps);
    var_define(ps, &name, false, export);
}

/* Parse 'var' or 'const' declaration */
static void parse_var(ParseState* ps, bool isconst, bool export)
{
    Token vars[255];
    int var_count = 0;
    int expr_count = 0;
    bool rest = false;
    int rest_count = 0;
    int rest_pos = 0;

    do
    {
        if(rest_count > 1)
        {
            error(ps, TEA_ERR_XDOTS);
        }

        if(lex_match(ps, TK_DOT_DOT_DOT))
        {
            rest = true;
            rest_count++;
        }

        lex_consume(ps, TK_NAME);
        vars[var_count] = ps->ls->prev;
        var_count++;

        if(rest)
        {
            rest_pos = var_count;
            rest = false;
        }

        if(var_count == 1 && lex_match(ps, '='))
        {
            if(rest_count)
            {
                error(ps, TEA_ERR_XSINGLEREST);
            }

            var_declare(ps, &vars[0]);
            expr(ps);
            var_define(ps, &vars[0], isconst, export);

            if(lex_match(ps, ','))
            {
                do
                {
                    lex_consume(ps, TK_NAME);
                    Token tok = ps->ls->prev;
                    var_declare(ps, &tok);
                    lex_consume(ps, '=');
                    expr(ps);
                    var_define(ps, &tok, isconst, export);
                }
                while(lex_match(ps, ','));
            }
            return;
        }
    }
    while(lex_match(ps, ','));

    if(rest_count)
    {
        lex_consume(ps, '=');
        expr(ps);
        bcemit_op(ps, BC_UNPACK_REST);
        bcemit_bytes(ps, var_count, rest_pos - 1);
        goto finish;
    }

    if(lex_match(ps, '='))
    {
        do
        {
            expr(ps);
            expr_count++;
            if(expr_count == 1 && (!lex_check(ps, ',')))
            {
                bcemit_argued(ps, BC_UNPACK, var_count);
                goto finish;
            }

        }
        while(lex_match(ps, ','));

        if(expr_count != var_count)
        {
            error(ps, TEA_ERR_XVALASSIGN);
        }
    }
    else
    {
        for(int i = 0; i < var_count; i++)
        {
            bcemit_op(ps, BC_NIL);
        }
    }

finish:
    if(ps->scope_depth == 0)
    {
        for(int i = var_count - 1; i >= 0; i--)
        {
            var_declare(ps, &vars[i]);
            var_define(ps, &vars[i], isconst, export);
        }
    }
    else
    {
        for(int i = 0; i < var_count; i++)
        {
            var_declare(ps, &vars[i]);
            var_define(ps, &vars[i], isconst, export);
        }
    }
}

/* Parse an expression statement */
static void parse_expr_stmt(ParseState* ps)
{
    expr(ps);
    bcemit_op(ps, BC_POP);
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
        case BC_INVOKE_NEW:
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
        case BC_DEFINE_MODULE:
            return 2;
        case BC_CLOSURE:
        {
            int constant = code[ip + 1];
            GCproto* pt = protoV(constants + constant);

            /* There is one byte for the constant, then two for each upvalue */
            return 1 + (pt->upvalue_count * 2);
        }
    }
    return 0;
}

static void loop_begin(ParseState* ps, Loop* loop)
{
    loop->start = ps->proto->bc_count;
    loop->scope_depth = ps->scope_depth;
    loop->prev = ps->loop;
    ps->loop = loop;
}

static void loop_end(ParseState* ps)
{
    if(ps->loop->end != -1)
    {
        bcpatch_jump(ps, ps->loop->end);
        bcemit_op(ps, BC_POP);
    }

    int i = ps->loop->body;
    while(i < ps->proto->bc_count)
    {
        if(ps->proto->bc[i] == BC_END)
        {
            ps->proto->bc[i] = BC_JUMP;
            bcpatch_jump(ps, i + 1);
            i += 3;
        }
        else
        {
            i += 1 + get_arg_count(ps->proto->bc, ps->proto->k, i);
        }
    }
    ps->loop = ps->loop->prev;
}

/* Parse iterable 'for' */
static void parse_for_in(ParseState* ps, Token var, bool isconst)
{
    Token vars[255];
    int var_count = 1;
    vars[0] = var;

    if(lex_match(ps, ','))
    {
        do
        {
            lex_consume(ps, TK_NAME);
            vars[var_count] = ps->ls->prev;
            var_count++;
        }
        while(lex_match(ps, ','));
    }

    lex_consume(ps, TK_IN);

    expr(ps);
    int seq_slot = var_add_local(ps, lex_synthetic(ps, "seq "));
    var_mark(ps, false);

    expr_nil(ps, false);
    int iter_slot = var_add_local(ps, lex_synthetic(ps, "iter "));
    var_mark(ps, false);

    lex_consume(ps, ')');

    Loop loop;
    loop_begin(ps, &loop);

    /* Get the iterator index. If it's nil, it means the loop is over */
    bcemit_op(ps, BC_GET_ITER);
    bcemit_bytes(ps, seq_slot, iter_slot);
    ps->loop->end = bcemit_jump(ps, BC_JUMP_IF_NIL);
    bcemit_op(ps, BC_POP);

    /* Get the iterator value */
    bcemit_op(ps, BC_FOR_ITER);
    bcemit_bytes(ps, seq_slot, iter_slot);

    scope_begin(ps);

    if(var_count > 1)
        bcemit_argued(ps, BC_UNPACK, var_count);

    for(int i = 0; i < var_count; i++)
    {
        var_declare(ps, &vars[i]);
        var_define(ps, &vars[i], isconst, false);
    }

    ps->loop->body = ps->proto->bc_count;
    parse_stmt(ps);

    /* Loop variable */
    scope_end(ps);

    bcemit_loop(ps, ps->loop->start);
    loop_end(ps);

    /* Hidden variables */
    scope_end(ps);
}

/* Parse generic 'for' */
static void parse_for(ParseState* ps)
{
    scope_begin(ps);

    tea_lex_next(ps->ls);  /* Skip 'for' */
    lex_consume(ps, '(');

    int loop_var = -1;
    Token var_name = ps->ls->curr;

    bool isconst = false;
    if(lex_match(ps, TK_VAR) || (isconst = lex_match(ps, TK_CONST)))
    {
        lex_consume(ps, TK_NAME);
        Token var = ps->ls->prev;

        if(lex_check(ps, TK_IN) || lex_check(ps, ','))
        {
            /* It's a for in statement */
            parse_for_in(ps, var, isconst);
            return;
        }

        /* Grab the name of the loop variable */
        var_name = var;

        var_declare(ps, &var);

        if(lex_match(ps, '='))
        {
            expr(ps);
        }
        else
        {
            bcemit_op(ps, BC_NIL);
        }

        var_define(ps, &var, isconst, false);
        lex_consume(ps, ';');

        /* Get loop variable slot */
        loop_var = ps->local_count - 1;
    }
    else
    {
        parse_expr_stmt(ps);
        lex_consume(ps, ';');
    }

    Loop loop;
    loop_begin(ps, &loop);

    ps->loop->end = -1;

    expr(ps);
    lex_consume(ps, ';');

    ps->loop->end = bcemit_jump(ps, BC_JUMP_IF_FALSE);
    bcemit_op(ps, BC_POP); /* Condition */

    int body_jmp = bcemit_jump(ps, BC_JUMP);

    int increment_start = ps->proto->bc_count;
    expr(ps);
    bcemit_op(ps, BC_POP);
    lex_consume(ps, ')');

    bcemit_loop(ps, ps->loop->start);
    ps->loop->start = increment_start;

    bcpatch_jump(ps, body_jmp);

    ps->loop->body = ps->proto->bc_count;

    int inner_var = -1;
    if(loop_var != -1)
    {
        scope_begin(ps);
        bcemit_argued(ps, BC_GET_LOCAL, (uint8_t)loop_var);
        var_add_local(ps, var_name);
        var_mark(ps, false);
        inner_var = ps->local_count - 1;
    }

    parse_stmt(ps);

    if(inner_var != -1)
    {
        bcemit_argued(ps, BC_GET_LOCAL, (uint8_t)inner_var);
        bcemit_argued(ps, BC_SET_LOCAL, (uint8_t)loop_var);
        bcemit_op(ps, BC_POP);
        scope_end(ps);
    }

    bcemit_loop(ps, ps->loop->start);

    loop_end(ps);
    scope_end(ps);
}

/* Parse 'break' statement */
static void parse_break(ParseState* ps)
{
    tea_lex_next(ps->ls);  /* Skip 'break' */
    if(ps->loop == NULL)
    {
        error(ps, TEA_ERR_XBREAK);
    }

    /* Discard any locals created inside the loop */
    discard_locals(ps, ps->loop->scope_depth + 1);
    bcemit_jump(ps, BC_END);
}

/* Parse 'continue' statement */
static void parse_continue(ParseState* ps)
{
    tea_lex_next(ps->ls);  /* Skip 'continue' */
    if(ps->loop == NULL)
    {
        error(ps, TEA_ERR_XCONTINUE);
    }

    /* Discard any locals created inside the loop */
    discard_locals(ps, ps->loop->scope_depth + 1);

    /* Jump to the top of the innermost loop */
    bcemit_loop(ps, ps->loop->start);
}

/* Parse 'if' statement */
static void parse_if(ParseState* ps)
{
    tea_lex_next(ps->ls);  /* Skip 'if' */
    lex_consume(ps, '(');
    expr(ps);
    lex_consume(ps, ')');

    int else_jmp = bcemit_jump(ps, BC_JUMP_IF_FALSE);

    bcemit_op(ps, BC_POP);
    parse_stmt(ps);

    int end_jmp = bcemit_jump(ps, BC_JUMP);

    bcpatch_jump(ps, else_jmp);
    bcemit_op(ps, BC_POP);

    if(lex_match(ps, TK_ELSE))
        parse_stmt(ps);

    bcpatch_jump(ps, end_jmp);
}

/* Parse 'switch' statement */
static void parse_switch(ParseState* ps)
{
    int case_ends[256];
    int case_count = 0;

    tea_lex_next(ps->ls);  /* Skip 'switch' */
    lex_consume(ps, '(');
    expr(ps);
    lex_consume(ps, ')');
    lex_consume(ps, '{');

    if(lex_match(ps, TK_CASE))
    {
        do
        {
            expr(ps);
            int multiple_cases = 0;
            if(lex_match(ps, ','))
            {
                do
                {
                    multiple_cases++;
                    expr(ps);
                }
                while(lex_match(ps, ','));
                bcemit_argued(ps, BC_MULTI_CASE, multiple_cases);
            }
            int compare_jmp = bcemit_jump(ps, BC_COMPARE_JUMP);
            lex_consume(ps, ':');
            parse_stmt(ps);
            case_ends[case_count++] = bcemit_jump(ps, BC_JUMP);
            bcpatch_jump(ps, compare_jmp);
            if(case_count > 255)
            {
                error_at_current(ps, TEA_ERR_XSWITCH);
            }

        }
        while(lex_match(ps, TK_CASE));
    }

    bcemit_op(ps, BC_POP); /* Expression */
    if(lex_match(ps, TK_DEFAULT))
    {
        lex_consume(ps, ':');
        parse_stmt(ps);
    }

    if(lex_match(ps, TK_CASE))
    {
        error(ps, TEA_ERR_XCASE);
    }

    lex_consume(ps, '}');

    for(int i = 0; i < case_count; i++)
    {
    	bcpatch_jump(ps, case_ends[i]);
    }
}

/* Parse 'return' statement */
static void parse_return(ParseState* ps)
{
    tea_lex_next(ps->ls);  /* Skip 'return' */

    if(ps->type == PROTO_SCRIPT)
    {
        error(ps, TEA_ERR_XRET);
    }

    if(lex_check(ps, '}') || lex_match(ps, ';'))
    {
        bcemit_return(ps);
    }
    else
    {
        if(ps->type == PROTO_INIT)
        {
            error(ps, TEA_ERR_XINIT);
        }

        expr(ps);
        bcemit_op(ps, BC_RETURN);
    }
}

/* Parse 'import name' statement */
static void parse_import_name(ParseState* ps)
{
    lex_consume(ps, TK_NAME);
    Token name = ps->ls->prev;
    uint8_t k = const_str(ps, strV(&name.tv));
    bcemit_argued(ps, BC_IMPORT_NAME, k);
    bcemit_op(ps, BC_POP);

    if(lex_match(ps, TK_AS))
    {
        lex_consume(ps, TK_NAME);
        name = ps->ls->prev;
    }
    
    var_declare(ps, &name);
    bcemit_op(ps, BC_IMPORT_ALIAS);
    var_define(ps, &name, false, false);

    bcemit_op(ps, BC_IMPORT_END);

    if(lex_match(ps, ','))
    {
        parse_import_name(ps);
    }
}

/* Parse 'import <string>' statement */
static void parse_import_string(ParseState* ps)
{
    lex_consume(ps, TK_STRING);
    uint8_t k = const_str(ps, strV(&ps->ls->prev.tv));

    bcemit_argued(ps, BC_IMPORT_STRING, k);
    bcemit_op(ps, BC_POP);

    if(lex_match(ps, TK_AS))
    {
        lex_consume(ps, TK_NAME);
        Token name = ps->ls->prev;
        var_declare(ps, &name);
        bcemit_op(ps, BC_IMPORT_ALIAS);
        var_define(ps, &name, false, false);
    }

    bcemit_op(ps, BC_IMPORT_END);

    if(lex_match(ps, ','))
    {
        parse_import_string(ps);
    }
}

/* Parse 'from' statement */
static void parse_from(ParseState* ps)
{
    if(lex_match(ps, TK_STRING))
    {
        uint8_t k = const_str(ps, strV(&ps->ls->prev.tv));
        lex_consume(ps, TK_IMPORT);
        bcemit_argued(ps, BC_IMPORT_STRING, k);
        bcemit_op(ps, BC_POP);
    }
    else
    {
        lex_consume(ps, TK_NAME);
        uint8_t k = const_str(ps, strV(&ps->ls->prev.tv));
        lex_consume(ps, TK_IMPORT);
        bcemit_argued(ps, BC_IMPORT_NAME, k);
        bcemit_op(ps, BC_POP);
    }

    do
    {
        lex_consume(ps, TK_NAME);
        Token name = ps->ls->prev;
        uint8_t k = const_str(ps, strV(&name.tv));

        if(lex_match(ps, TK_AS))
        {
            lex_consume(ps, TK_NAME);
            name = ps->ls->prev;
        }
        var_declare(ps, &name);
        bcemit_argued(ps, BC_IMPORT_VARIABLE, k);
        var_define(ps, &name, false, false);
    }
    while(lex_match(ps, ','));

    bcemit_op(ps, BC_IMPORT_END);
}

/* Parse 'while' statement */
static void parse_while(ParseState* ps)
{
    Loop loop;
    loop_begin(ps, &loop);

    tea_lex_next(ps->ls);  /* Skip 'while' */
    if(!lex_check(ps, '('))
    {
        bcemit_byte(ps, BC_TRUE);
    }
    else
    {
        lex_consume(ps, '(');
        expr(ps);
        lex_consume(ps, ')');
    }

    /* Jump ot of the loop if the condition is false */
    ps->loop->end = bcemit_jump(ps, BC_JUMP_IF_FALSE);
    bcemit_op(ps, BC_POP);

    /* Compile the body */
    ps->loop->body = ps->proto->bc_count;
    parse_stmt(ps);

    /* Loop back to the start */
    bcemit_loop(ps, ps->loop->start);
    loop_end(ps);
}

/* Parse 'do' statement */
static void parse_do(ParseState* ps)
{
    Loop loop;
    loop_begin(ps, &loop);

    tea_lex_next(ps->ls);  /* Skip 'do' */

    ps->loop->body = ps->proto->bc_count;
    parse_stmt(ps);

    lex_consume(ps, TK_WHILE);

    if(!lex_check(ps, '('))
    {
        bcemit_op(ps, BC_TRUE);
    }
    else
    {
        lex_consume(ps, '(');
        expr(ps);
        lex_consume(ps, ')');
    }

    ps->loop->end = bcemit_jump(ps, BC_JUMP_IF_FALSE);
    bcemit_op(ps, BC_POP);

    bcemit_loop(ps, ps->loop->start);
    loop_end(ps);
}

static void parse_multiple_assign(ParseState* ps)
{
    int expr_count = 0;
    int var_count = 0;
    Token vars[255];

    do
    {
        lex_consume(ps, TK_NAME);
        vars[var_count] = ps->ls->prev;
        var_count++;
    }
    while(lex_match(ps, ','));

    lex_consume(ps, '=');

    do
    {
        expr(ps);
        expr_count++;
        if(expr_count == 1 && (!lex_check(ps, ',')))
        {
            bcemit_argued(ps, BC_UNPACK, var_count);
            goto finish;
        }
    }
    while(lex_match(ps, ','));

    if(expr_count != var_count)
    {
        error(ps, TEA_ERR_XVASSIGN);
    }

finish:
    for(int i = var_count - 1; i >= 0; i--)
    {
        Token tok = vars[i];
        uint8_t set_op;
        int arg = var_lookup_local(ps, &tok);
        if(arg != -1)
        {
            set_op = BC_SET_LOCAL;
        }
        else if((arg = var_lookup_uv(ps, &tok)) != -1)
        {
            set_op = BC_SET_UPVALUE;
        }
        else
        {
            arg = const_str(ps, strV(&tok.tv));
            set_op = BC_SET_MODULE;
        }
        check_const(ps, set_op, arg);
        bcemit_argued(ps, set_op, (uint8_t)arg);
        bcemit_op(ps, BC_POP);
    }
}

static void parse_export(ParseState* ps)
{
    tea_lex_next(ps->ls);
    if(ps->type != PROTO_SCRIPT)
    {
        error(ps, TEA_ERR_XRET);
    }
    LexToken t = ps->ls->curr.t;
    if(t == TK_CLASS || t == TK_FUNCTION || t == TK_CONST || t == TK_VAR)
    {
        parse_decl(ps, true);
        return;
    }
    lex_consume(ps, '{');
    while(!lex_check(ps, '}') && !lex_check(ps, TK_EOF))
    {
        do
        {
            Token name;
            lex_consume(ps, TK_NAME);
            name = ps->ls->prev;
            uint8_t k = const_str(ps, strV(&name.tv));
            bcemit_argued(ps, BC_GET_MODULE, k);
            bcemit_argued(ps, BC_DEFINE_MODULE, k);
            bcemit_byte(ps, 2);
        }
        while(lex_match(ps, ','));
    }
    lex_consume(ps, '}');
}

/* -- Parse statements and declarations ---------------------------------------------------- */

/* Parse a declaration */
static void parse_decl(ParseState* ps, bool export)
{
    LexToken t = ps->ls->curr.t;
    switch(t)
    {
        case TK_CLASS:
            parse_class(ps, export);
            break;
        case TK_FUNCTION:
            parse_function(ps, export);
            break;
        case TK_CONST:
        case TK_VAR:
            tea_lex_next(ps->ls);  /* Skip 'const' or 'var' */
            parse_var(ps, t == TK_CONST, export);
            break;
        default:
            parse_stmt(ps);
            break;
    }
}

/* Parse a statement */
static void parse_stmt(ParseState* ps)
{
    switch(ps->ls->curr.t)
    {
        case ';':
            tea_lex_next(ps->ls);
            break;
        case TK_FOR:
            parse_for(ps);
            break;
        case TK_IF:
            parse_if(ps);
            break;
        case TK_SWITCH:
            parse_switch(ps);
            break;
        case TK_EXPORT:
            parse_export(ps);
            break;
        case TK_RETURN:
            parse_return(ps);
            break;
        case TK_WHILE:
            parse_while(ps);
            break;
        case TK_DO:
            parse_do(ps);
            break;
        case TK_IMPORT:
            tea_lex_next(ps->ls);  /* Skip 'import' */
            if(lex_check(ps, TK_STRING))
                parse_import_string(ps);
            else
                parse_import_name(ps);
            break;
        case TK_FROM:
            tea_lex_next(ps->ls);  /* Skip 'from' */
            parse_from(ps);
            break;
        case TK_BREAK:
            parse_break(ps);
            break;
        case TK_CONTINUE:
            parse_continue(ps);
            break;
        case TK_NAME:
        {
            if(ps->ls->next.t == ',')
                parse_multiple_assign(ps);
            else
                parse_expr_stmt(ps);
            break;
        }
        case '{':
        {
            tea_lex_next(ps->ls);  /* Skip '{' */
            scope_begin(ps);
            parse_block(ps);
            scope_end(ps);
            break;
        }
        default:
            parse_expr_stmt(ps);
            break;
    }
}

/* Entry point of bytecode parser */
GCproto* tea_parse(LexState* ls, bool isexpr)
{
    ParseState ps;
    parser_init(ls, &ps, NULL, PROTO_SCRIPT);

    tea_lex_next(ps.ls);   /* Read the first token into "next" */
    tea_lex_next(ps.ls);   /* Copy "next" -> "curr" */

    if(isexpr)
    {
        expr(&ps);
        bcemit_op(&ps, BC_RETURN);
        lex_consume(&ps, TK_EOF);
    }
    else
    {
        while(!lex_match(&ps, TK_EOF))
        {
            parse_decl(&ps, false);
        }
        bcemit_return(&ps);
    }
    return parser_end(&ps);
}

void tea_parse_mark(tea_State* T, ParseState* ps)
{
    tea_gc_markval(T, &ps->ls->prev.tv);
    tea_gc_markval(T, &ps->ls->curr.tv);
    tea_gc_markval(T, &ps->ls->next.tv);
    while(ps != NULL)
    {
        tea_gc_markobj(T, (GCobj*)ps->proto);
        ps = ps->prev;
    }
}