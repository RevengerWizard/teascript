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
#include "tea_str.h"
#include "tea_gc.h"
#include "tea_lex.h"
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
    bool isupval;
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
    struct KlassState* prev; /* Enclosing class state */
    bool has_superclass;
} KlassState;

typedef struct Loop
{
    struct Loop* prev; /* Enclosing loop */
    BCPos start;
    BCPos body;
    int end;
    int scope_depth;
} Loop;

typedef enum
{
    FUNC_NORMAL,
    FUNC_ANONYMOUS,
    FUNC_ARROW,
    FUNC_INIT,
    FUNC_METHOD,
    FUNC_OPERATOR,
    FUNC_SCRIPT
} FuncInfo;

typedef struct FuncState
{
    GCmap* kt;  /* Map for constants */
    LexState* ls; /* Lexer state */
    tea_State* T;   /* Teascript state */
    struct FuncState* prev;   /* Enclosing function */
    BCPos pc; /* Next bytecode position */
    uint32_t nk;    /* Number of number/GCobj constants */
    BCLine linedefined; /* First line of the function definition */
    BCInsLine* bcbase;  /* Base of bytecode stack */
    BCPos bclimit;    /* Limit of bytecode stack */
    uint8_t flags;  /* Prototype flags */
    uint8_t numparams;  /* Number of parameters */
    uint8_t numopts;    /* Number of optional parameters */
    uint32_t nuv;    /* Number of upvalues */
    GCstr* name;    /* Name of prototype function */
    int max_slots; /* Stack max size */
    KlassState* klass; /* Current class state */
    Loop* loop; /* Current loop context */
    FuncInfo info;  /* Info about the function */
    int local_count;    /* Number of local variables in scope */
    Local locals[TEA_MAX_LOCAL];  /* Current scoped locals */
    Upvalue upvalues[TEA_MAX_UPVAL];  /* Saved upvalues */
    uint16_t uvmap[TEA_MAX_UPVAL];  /* Temporary upvalue map */
    int scope_depth;    /* Current scope depth */
} FuncState;

typedef void (*ParseFn)(FuncState* fs, bool assign);

typedef struct
{
    ParseFn prefix;
    ParseFn infix;
    Precedence prec;
} ParseRule;

#ifdef TEA_USE_ASSERT
#define tea_assertFS(c, ...) (tea_assertT_(fs->T, (c), __VA_ARGS__))
#else
#define tea_assertFS(c, ...) ((void)fs)
#endif

/* -- Error handling --------------------------------------------- */

TEA_NORET TEA_NOINLINE static void error(FuncState* fs, ErrMsg em)
{
    tea_lex_error(fs->ls, fs->ls->prev.t, fs->ls->prev.line, em);
}

TEA_NORET static void err_limit(FuncState* fs, uint32_t limit, const char* what)
{
    BCLine line = fs->ls->prev.line;
    if(fs->linedefined == 0)
        tea_lex_error(fs->ls, 0, line, TEA_ERR_XLIMM, limit, what);
    else
        tea_lex_error(fs->ls, 0, line,
                        TEA_ERR_XLIMF, fs->linedefined, limit, what);
}

#define checklimit(fs, v, l, m)	if((v) >= (l)) err_limit(fs, l, m)

/* -- Lexer support ------------------------------------------------------- */

static Token lex_synthetic(FuncState* fs, const char* text)
{
    Token tok;
    GCstr* s = tea_str_newlen(fs->ls->T, text);
    setstrV(fs->ls->T, &tok.tv, s);
    return tok;
}

static void lex_consume(FuncState* fs, LexToken tok)
{
    if(fs->ls->curr.t == tok)
    {
        tea_lex_next(fs->ls);
        return;
    }
    const char* tokstr = tea_lex_token2str(fs->ls, tok);
    tea_lex_error(fs->ls, fs->ls->curr.t, fs->ls->curr.line, TEA_ERR_XTOKEN, tokstr);
}

/* Check for matching token */
static bool lex_check(FuncState* fs, LexToken tok)
{
    return fs->ls->curr.t == tok;
}

/* Check and consume token */
static bool lex_match(FuncState* fs, LexToken tok)
{
    if(!lex_check(fs, tok))
        return false;
    tea_lex_next(fs->ls);
    return true;
}

/* -- Management of constants --------------------------------------------- */

/* Add a number constant */
static uint8_t const_num(FuncState* fs, TValue* n)
{
    tea_State* T = fs->T;
    GCmap* kt = fs->kt;
    TValue* o = (TValue*)tea_map_get(kt, n);
    if(o && tvisnum(o))
        return (uint8_t)numV(o);
    o = tea_map_set(T, kt, n);
    setnumV(o, fs->nk);
    return fs->nk++;
}

/* Add a GC object constant */
static uint8_t const_gc(FuncState* fs, GCobj* obj, uint32_t tt)
{
    tea_State* T = fs->T;
    GCmap* kt = fs->kt;
    TValue key, *o;
    setgcV(T, &key, obj, tt);
    o = (TValue*)tea_map_get(kt, &key);
    if(o && tvisnum(o))
        return (uint8_t)numV(o);
    o = tea_map_set(T, kt, &key);
    setnumV(o, fs->nk);
    return fs->nk++;
}

/* Add a string constant */
static uint8_t const_str(FuncState* fs, GCstr* str)
{
    return const_gc(fs, (GCobj*)str, TEA_TSTR);
}

/* Anchor string constant to avoid GC */
GCstr* tea_parse_keepstr(LexState* ls, const char* str, size_t len)
{
    tea_State* T = ls->T;
    GCmap* kt = ls->fs->kt;
    GCstr* s = tea_str_new(T, str, len);
    TValue* tv = (TValue*)tea_map_getstr(T, kt, s);
    if(!tv)
    {
        tv = tea_map_setstr(T, kt, s);
        setboolV(tv, true);
    }
    return s;
}

/* -- Bytecode emitter ---------------------------------------------------- */

static void bcemit_byte(FuncState* fs, uint8_t byte)
{
    BCPos pc = fs->pc;
    LexState* ls = fs->ls;
    if(TEA_UNLIKELY(pc >= fs->bclimit))
    {
        ptrdiff_t base = fs->bcbase - ls->bcstack;
        checklimit(fs, ls->sizebcstack, TEA_MAX_BCINS, "bytecode instructions");
        ls->bcstack = tea_mem_growvec(fs->T, BCInsLine, ls->bcstack, ls->sizebcstack, TEA_MAX_BCINS);
        fs->bclimit = (BCPos)(ls->sizebcstack - base);
        fs->bcbase = ls->bcstack + base;
    }
    fs->bcbase[pc].ins = byte;
    fs->bcbase[pc].line = ls->prev.line;
    fs->pc = pc + 1;
}

static void bcemit_bytes(FuncState* fs, uint8_t byte1, uint8_t byte2)
{
    bcemit_byte(fs, byte1);
    bcemit_byte(fs, byte2);
}

/* Stack effects of each bytecode instruction */
static const int bc_effects[] = {
#define BCEFFECT(_, effect, __) effect,
    BCDEF(BCEFFECT)
#undef BCEFFECT
};

/* Emit bytecode instruction */
static void bcemit_op(FuncState* fs, BCOp op)
{
    bcemit_byte(fs, op);
    fs->max_slots += bc_effects[op];
}

/* Emit 2 bytecode instructions */
static void bcemit_ops(FuncState* fs, BCOp op1, BCOp op2)
{
    bcemit_bytes(fs, op1, op2);
    fs->max_slots += bc_effects[op1] + bc_effects[op2];
}

static void bcemit_argued(FuncState* fs, BCOp op, uint8_t byte)
{
    bcemit_bytes(fs, op, byte);
    fs->max_slots += bc_effects[op];
}

static void bcemit_loop(FuncState* fs, BCPos start)
{
    bcemit_op(fs, BC_LOOP);
    BCPos ofs = fs->pc - start + 2;
    if(ofs > UINT16_MAX)
        error(fs, TEA_ERR_XLOOP);
    bcemit_byte(fs, (ofs >> 8) & 0xff);
    bcemit_byte(fs, ofs & 0xff);
}

static BCPos bcemit_jump(FuncState* fs, BCOp op)
{
    bcemit_op(fs, op);
    bcemit_bytes(fs, 0xff, 0xff);
    return fs->pc - 2;
}

static void bcemit_return(FuncState* fs)
{
    if(fs->info == FUNC_INIT)
    {
        bcemit_argued(fs, BC_GET_LOCAL, 0);
    }
    else
    {
        bcemit_op(fs, BC_NIL);
    }
    bcemit_op(fs, BC_RETURN);
}

static void bcemit_invoke(FuncState* fs, int args, const char* name)
{
    GCstr* str = tea_str_new(fs->T, name, strlen(name));
    bcemit_argued(fs, BC_INVOKE, const_str(fs, str));
    bcemit_byte(fs, args);
}

static void bcemit_num(FuncState* fs, TValue* o)
{
    bcemit_argued(fs, BC_CONSTANT, const_num(fs, o));
}

static void bcemit_str(FuncState* fs, TValue* o)
{
    bcemit_argued(fs, BC_CONSTANT, const_str(fs, strV(o)));
}

static void bcpatch_jump(FuncState* fs, BCPos ofs)
{
    /* -2 to adjust for the bytecode for the jump offset itself */
    BCPos jmp = fs->pc - ofs - 2;
    if(jmp > UINT16_MAX)
        error(fs, TEA_ERR_XJUMP);
    fs->bcbase[ofs].ins = (jmp >> 8) & 0xff;
    fs->bcbase[ofs + 1].ins = jmp & 0xff;
}

/* -- Function state management ------------------------------------------- */

/* Fixup bytecode for prototype */
static void fs_fixup_bc(FuncState* fs, GCproto* pt, BCIns* bc, size_t n)
{
    BCInsLine* base = fs->bcbase;
    pt->sizebc = n;
    for(uint32_t i = 0; i < n; i++)
        bc[i] = base[i].ins;
}

/* Fixup constants for prototype */
static void fs_fixup_k(FuncState* fs, GCproto* pt, void* kptr)
{
    tea_State* T = fs->T;
    GCmap* kt;
    if(fs->nk > UINT8_MAX)
    {
        error(fs, TEA_ERR_XKCONST);
    }
    pt->k = (TValue*)kptr;
    pt->sizek = fs->nk;
    kt = fs->kt;
    for(int i = 0; i < kt->size; i++)
    {
        MapEntry* n = &kt->entries[i];
        if(tvisnil(&n->key)) continue;
        if(!tvisbool(&n->val))
        {
            int32_t kidx = (int32_t)numV(&n->val);
            TValue* tv = &((TValue*)kptr)[kidx];
            copyTV(T, tv, &n->key);
        }
    }
}

/* Fixup upvalues for prototype */
static void fs_fixup_uv(FuncState* fs, GCproto* pt, uint16_t* uv)
{
    pt->uv = uv;
    pt->sizeuv = (uint8_t)fs->nuv;
    memcpy(uv, fs->uvmap, fs->nuv * sizeof(uint16_t));
}

/* Prepare lineinfo for prototype */
static size_t fs_prep_line(FuncState* fs, BCLine numline)
{
    return (fs->pc - 1) << (numline < 256 ? 0 : numline < 65536 ? 1 : 2);
}

/* Fixup lineinfo for prototype */
static void fs_fixup_line(FuncState* fs, GCproto* pt, void* lineinfo, BCLine numline)
{
    BCInsLine* base = fs->bcbase + 1;
    BCLine first = fs->linedefined;
    uint32_t i = 0, n = fs->pc - 1;
    pt->firstline = fs->linedefined;
    pt->numline = numline;
    pt->lineinfo = lineinfo;
    if(TEA_LIKELY(numline < 256))
    {
        uint8_t* li = (uint8_t*)lineinfo;
        do
        {
            BCLine delta = base[i].line - first;
            tea_assertFS(delta >= 0 && delta < 256, "bad line delta");
            li[i] = (uint8_t)delta;
        }
        while(++i < n);
    }
    else if(TEA_LIKELY(numline < 65536))
    {
        uint16_t* li = (uint16_t*)lineinfo;
        do
        {
            BCLine delta = base[i].line - first;
            tea_assertFS(delta >= 0 && delta < 65536, "bad line delta");
            li[i] = (uint16_t)delta;
        }
        while(++i < n);
    }
    else
    {
        uint32_t* li = (uint32_t*)lineinfo;
        do
        {
            BCLine delta = base[i].line - first;
            tea_assertFS(delta >= 0, "bad line delta");
            li[i] = (uint32_t)delta;
        }
        while(++i < n);
    }
}

/* Finish a FuncState and return the new prototype */
static GCproto* fs_finish(LexState* ls, BCLine line)
{
    tea_State* T = ls->T;
    FuncState* fs = ls->fs;
    BCLine numline = line - fs->linedefined;
    size_t sizept, ofsk, ofsuv, ofsli;
    GCproto* pt;

    /* Calculate total size of prototype including all colocated arrays */
    sizept = sizeof(GCproto) + fs->pc * sizeof(BCIns);
    ofsk = sizept; sizept += fs->nk * sizeof(TValue);
    ofsuv = sizept; sizept += fs->nuv * sizeof(uint16_t);
    ofsli = sizept; sizept += fs_prep_line(fs, numline);

    /* Allocate new prototype and initialize fields */
    pt = (GCproto*)tea_mem_newgco(T, sizept, TEA_TPROTO);
    pt->sizept = sizept;
    pt->numparams = fs->numparams;
    pt->numopts = fs->numopts;
    pt->max_slots = fs->max_slots;
    pt->flags = fs->flags;
    pt->name = fs->name;

    fs_fixup_bc(fs, pt, (BCIns*)((char*)pt + sizeof(GCproto)), fs->pc);
    fs_fixup_k(fs, pt, (void*)((char*)pt + ofsk));
    fs_fixup_uv(fs, pt, (uint16_t*)((char*)pt + ofsuv));
    fs_fixup_line(fs, pt, (void*)((char*)pt + ofsli), numline);

    T->top--;   /* Pop table of constants */
    ls->fs = fs->prev;
    tea_assertT(ls->fs != NULL || ls->curr.t == TK_EOF, "bad parser state");
    return pt;
}

/* Initialize a new FuncState */
static void fs_init(LexState* ls, FuncState* fs, FuncInfo info)
{
    tea_State* T = ls->T;
    fs->prev = ls->fs; ls->fs = fs; /* Append to list */
    fs->ls = ls;
    fs->T = T;
    fs->klass = NULL;
    fs->loop = NULL;
    if(fs->prev)
    {
        fs->klass = fs->prev->klass;
    }
    fs->numparams = 0;
    fs->numopts = 0;
    fs->pc = 0;
    fs->nk = 0;
    fs->nuv = 0;
    fs->flags = 0;
    fs->info = info;
    fs->local_count = 1;
    fs->max_slots = 1;  /* Minimum slot size */
    fs->scope_depth = 0;
    fs->kt = tea_map_new(T);
    /* Anchor table of constants in stack to avoid being collected */
    setmapV(T, T->top, fs->kt);
    incr_top(T);

    switch(info)
    {
        case FUNC_NORMAL:
        case FUNC_INIT:
        case FUNC_METHOD:
            fs->name = strV(&fs->ls->prev.tv);
            break;
        case FUNC_OPERATOR:
            fs->name = fs->prev->name;
            break;
        case FUNC_ANONYMOUS:
        case FUNC_ARROW:
            fs->name = tea_str_newlit(T, "<anonymous>");
            break;
        case FUNC_SCRIPT:
            fs->name = tea_str_newlit(T, "<script>");
            break;
        default:
            break;
    }

    Local* local = &fs->locals[0];
    local->depth = 0;
    local->isupval = false;

    GCstr* s;
    switch(info)
    {
        case FUNC_SCRIPT:
        case FUNC_NORMAL:
        case FUNC_ANONYMOUS:
        case FUNC_ARROW:
            s = &T->strempty;
            setstrV(T, &local->name.tv, s);
            break;
        case FUNC_INIT:
        case FUNC_METHOD:
        case FUNC_OPERATOR:
            s = tea_str_newlit(T, "self");
            setstrV(T, &local->name.tv, s);
            break;
        default:
            break;
    }
}

/* -- Scope handling ------------------------------------------------------ */

/* Begin a scope */
static void scope_begin(FuncState* fs)
{
    fs->scope_depth++;
}

static int scope_discard(FuncState* fs, int depth)
{
    int local;
    for(local = fs->local_count - 1; local >= 0 && fs->locals[local].depth >= depth; local--)
    {
        if(fs->locals[local].isupval)
        {
            bcemit_op(fs, BC_CLOSE_UPVALUE);
        }
        else
        {
            bcemit_op(fs, BC_POP);
        }
    }
    return fs->local_count - local - 1;
}

/* End a scope */
static void scope_end(FuncState* fs)
{
    int effect = scope_discard(fs, fs->scope_depth);
    fs->local_count -= effect;
    fs->max_slots -= effect;
    fs->scope_depth--;
}

/* -- Loop handling -------------------------------------------------- */

/* Arg count of each bytecode instruction */
static const int bc_argcount[] = {
#define BCARG(_, __, argcount) argcount,
    BCDEF(BCARG)
#undef BCARG
};

static void loop_begin(FuncState* fs, Loop* loop)
{
    loop->start = fs->pc;
    loop->scope_depth = fs->scope_depth;
    loop->prev = fs->loop;
    fs->loop = loop;
}

static void loop_end(FuncState* fs)
{
    if(fs->loop->end != -1)
    {
        bcpatch_jump(fs, fs->loop->end);
        bcemit_op(fs, BC_POP);
    }

    int i = fs->loop->body;
    while(i < fs->pc)
    {
        if(fs->bcbase[i].ins == BC_END)
        {
            fs->bcbase[i].ins = BC_JUMP;
            bcpatch_jump(fs, i + 1);
            i += 3;
        }
        else
        {
            i += 1 + bc_argcount[fs->bcbase[i].ins];
        }
    }
    fs->loop = fs->loop->prev;
}

/* -- Variable handling --------------------------------------------------- */

static int var_lookup_local(FuncState* fs, Token* name)
{
    for(int i = fs->local_count - 1; i >= 0; i--)
    {
        Local* local = &fs->locals[i];
        if(strV(&name->tv) == strV(&local->name.tv))
        {
            if(local->depth == -1)
                break;
            return i;
        }
    }
    return -1;
}

static int var_add_uv(FuncState* fs, uint8_t index, bool islocal, bool isconst)
{
    uint32_t i, n = fs->nuv;
    for(i = 0; i < n; i++)
    {
        Upvalue* upvalue = &fs->upvalues[i];
        if(upvalue->index == index && upvalue->islocal == islocal)
        {
            return i;
        }
    }
    checklimit(fs, n, TEA_MAX_UPVAL, "closure variables");
    fs->upvalues[n].islocal = islocal;
    fs->upvalues[n].index = index;
    fs->upvalues[n].isconst = isconst;
    fs->uvmap[n] = (uint16_t)(((islocal) << 8) | (index));
    return fs->nuv++;
}

static int var_lookup_uv(FuncState* fs, Token* name)
{
    if(fs->prev == NULL)
        return -1;

    bool isconst;
    int local = var_lookup_local(fs->prev, name);
    if(local != -1)
    {
        isconst = fs->prev->locals[local].isconst;
        fs->prev->locals[local].isupval = true;
        return var_add_uv(fs, (uint8_t)local, true, isconst);
    }

    int upvalue = var_lookup_uv(fs->prev, name);
    if(upvalue != -1)
    {
        isconst = fs->prev->upvalues[upvalue].isconst;
        return var_add_uv(fs, (uint8_t)upvalue, false, isconst);
    }

    return -1;
}

static void var_mark(FuncState* fs, bool isconst)
{
    if(fs->scope_depth == 0) return;
    fs->locals[fs->local_count - 1].depth = fs->scope_depth;
    fs->locals[fs->local_count - 1].isconst = isconst;
}

static int var_add_local(FuncState* fs, Token name)
{
    checklimit(fs, fs->local_count, TEA_MAX_LOCAL, "local variables");
    int found = var_lookup_local(fs, &name);
    if(found != -1 && fs->locals[found].init && 
        fs->locals[found].depth == fs->scope_depth)
    {
        GCstr* name = strV(&fs->locals[found].name.tv);
        tea_lex_error(fs->ls, fs->ls->prev.t, fs->ls->prev.line, TEA_ERR_XDECL, str_data(name));
    }
    Local* local = &fs->locals[fs->local_count++];
    local->name = name;
    local->depth = -1;
    local->isupval = false;
    local->isconst = false;
    local->init = true;
    return fs->local_count - 1;
}

static void var_declare(FuncState* fs, Token* name)
{
    if(fs->scope_depth == 0)
        return;
    var_add_local(fs, *name);
}

static void var_define(FuncState* fs, Token* name, bool isconst, uint8_t export)
{
    if(fs->scope_depth > 0)
    {
        var_mark(fs, isconst);
        return;
    }
    GCstr* str = strV(&name->tv);
    uint8_t k = const_str(fs, str);
    if(isconst)
    {
        TValue* o = tea_tab_set(fs->ls->T, &fs->ls->T->constants, str, NULL);
        setnilV(o);
    }
    bcemit_argued(fs, BC_DEFINE_MODULE, k);
    bcemit_byte(fs, export);
}

/* -- Expressions --------------------------------------------------------- */

/* Forward declarations */
static ParseRule expr_rule(int type);
static void expr_precedence(FuncState* fs, Precedence prec);

static void expr_and(FuncState* fs, bool assign)
{
    BCPos jmp = bcemit_jump(fs, BC_JUMP_IF_FALSE);
    bcemit_op(fs, BC_POP);
    expr_precedence(fs, PREC_AND);
    bcpatch_jump(fs, jmp);
}

static void expr_binary(FuncState* fs, bool assign)
{
    LexToken op = fs->ls->prev.t;
    if(op == TK_NOT)
    {
        lex_consume(fs, TK_IN);

        ParseRule rule = expr_rule(op);
        expr_precedence(fs, (Precedence)(rule.prec + 1));

        bcemit_ops(fs, BC_IN, BC_NOT);
        return;
    }

    if(op == TK_IS && lex_match(fs, TK_NOT))
    {
        ParseRule rule = expr_rule(op);
        expr_precedence(fs, (Precedence)(rule.prec + 1));

        bcemit_ops(fs, BC_IS, BC_NOT);
        return;
    }

    ParseRule rule = expr_rule(op);
    expr_precedence(fs, (Precedence)(rule.prec + 1));
    switch(op)
    {
        case TK_BANG_EQUAL:
            bcemit_ops(fs, BC_EQUAL, BC_NOT);
            break;
        case TK_EQUAL_EQUAL:
            bcemit_op(fs, BC_EQUAL);
            break;
        case TK_IS:
            bcemit_op(fs, BC_IS);
            break;
        case '>':
            bcemit_op(fs, BC_GREATER);
            break;
        case TK_GREATER_EQUAL:
            bcemit_op(fs, BC_GREATER_EQUAL);
            break;
        case '<':
            bcemit_op(fs, BC_LESS);
            break;
        case TK_LESS_EQUAL:
            bcemit_op(fs, BC_LESS_EQUAL);
            break;
        case '+':
            bcemit_op(fs, BC_ADD);
            break;
        case '-':
            bcemit_op(fs, BC_SUBTRACT);
            break;
        case '*':
            bcemit_op(fs, BC_MULTIPLY);
            break;
        case '/':
            bcemit_op(fs, BC_DIVIDE);
            break;
        case '%':
            bcemit_op(fs, BC_MOD);
            break;
        case TK_STAR_STAR:
            bcemit_op(fs, BC_POW);
            break;
        case '&':
            bcemit_op(fs, BC_BAND);
            break;
        case '|':
            bcemit_op(fs, BC_BOR);
            break;
        case '^':
            bcemit_op(fs, BC_BXOR);
            break;
        case TK_GREATER_GREATER:
            bcemit_op(fs, BC_RSHIFT);
            break;
        case TK_LESS_LESS:
            bcemit_op(fs, BC_LSHIFT);
            break;
        case TK_IN:
            bcemit_op(fs, BC_IN);
            break;
        default: 
            return; /* Unreachable */
    }
}

/* Forward declaration */
static void expr(FuncState* fs);

static void expr_ternary(FuncState* fs, bool assign)
{
    /* Jump to else branch if the condition is false */
    BCPos else_jmp = bcemit_jump(fs, BC_JUMP_IF_FALSE);

    /* Pop the condition */
    bcemit_op(fs, BC_POP);
    expr(fs);

    BCPos end_jmp = bcemit_jump(fs, BC_JUMP);

    bcpatch_jump(fs, else_jmp);
    bcemit_op(fs, BC_POP);

    lex_consume(fs, ':');
    expr(fs);

    bcpatch_jump(fs, end_jmp);
}

/* Parse function argument list */
static uint8_t parse_args(FuncState* fs)
{
    uint8_t nargs = 0;
    if(!lex_check(fs, ')'))
    {
        do
        {
            expr(fs);
            if(nargs == 255)
            {
                error(fs, TEA_ERR_XARGS);
            }
            nargs++;
        }
        while(lex_match(fs, ','));
    }
    lex_consume(fs, ')');
    return nargs;
}

static void expr_call(FuncState* fs, bool assign)
{
    uint8_t nargs = parse_args(fs);
    bcemit_argued(fs, BC_CALL, nargs);
}

static int bcassign(FuncState* fs)
{
    switch(fs->ls->curr.t)
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

static void expr_dot(FuncState* fs, bool assign)
{
    bool isnew = false;
    if(lex_match(fs, TK_NEW))
    {
        isnew = true;
    }
    else
    {
        lex_consume(fs, TK_NAME);
    }

    uint8_t name = const_str(fs, strV(&fs->ls->prev.tv));
    if(lex_match(fs, '('))
    {
        uint8_t nargs = parse_args(fs);
        if(isnew)
        {
            bcemit_argued(fs, BC_INVOKE_NEW, nargs);
        }
        else
        {
            bcemit_argued(fs, BC_INVOKE, name);
            bcemit_byte(fs, nargs);
        }
        return;
    }

    int bc;
    if(assign && lex_match(fs, '='))
    {
        expr(fs);
        bcemit_argued(fs, BC_SET_ATTR, name);
    }
    else if(assign && (bc = bcassign(fs)))
    {
        tea_lex_next(fs->ls);
        bcemit_argued(fs, BC_PUSH_ATTR, name);
        expr(fs);
        bcemit_op(fs, bc);
        bcemit_argued(fs, BC_SET_ATTR, name);
    }
    else
    {
        if(lex_match(fs, TK_PLUS_PLUS)) bc = BC_ADD;
        else if(lex_match(fs, TK_MINUS_MINUS)) bc = BC_SUBTRACT;
        else bc = 0;
        if(bc)
        {
            bcemit_argued(fs, BC_PUSH_ATTR, name);
            TValue _v;
            setnumV(&_v, 1);
            bcemit_num(fs, &_v);
            bcemit_op(fs, bc);
            bcemit_argued(fs, BC_SET_ATTR, name);
        }
        else
        {
            bcemit_argued(fs, BC_GET_ATTR, name);
        }
    }
}

static void expr_true(FuncState* fs, bool assign)
{
    bcemit_op(fs, BC_TRUE);
}

static void expr_false(FuncState* fs, bool assign)
{
    bcemit_op(fs, BC_FALSE);
}

static void expr_nil(FuncState* fs, bool assign)
{
    bcemit_op(fs, BC_NIL);
}

static void expr_list(FuncState* fs, bool assign)
{
    bcemit_op(fs, BC_LIST);
    if(!lex_check(fs, ']'))
    {
        do
        {
            /* Traling comma */
            if(lex_check(fs, ']'))
                break;

            if(lex_match(fs, TK_DOT_DOT_DOT))
            {
                expr(fs);
                bcemit_op(fs, BC_LIST_EXTEND);
                continue;
            }

            expr(fs);
            bcemit_op(fs, BC_LIST_ITEM);
        }
        while(lex_match(fs, ','));
    }
    lex_consume(fs, ']');
}

static void expr_map(FuncState* fs, bool assign)
{
    bcemit_op(fs, BC_MAP);
    if(!lex_check(fs, '}'))
    {
        do
        {
            /* Traling comma */
            if(lex_check(fs, '}'))
                break;

            if(lex_match(fs, '['))
            {
                expr(fs);
                lex_consume(fs, ']');
                lex_consume(fs, '=');
                expr(fs);
            }
            else
            {
                lex_consume(fs, TK_NAME);
                bcemit_str(fs, &fs->ls->prev.tv);
                lex_consume(fs, '=');
                expr(fs);
            }
            bcemit_op(fs, BC_MAP_FIELD);
        }
        while(lex_match(fs, ','));
    }
    lex_consume(fs, '}');
}

static bool parse_slice(FuncState* fs)
{
    expr(fs);
    /* It's a slice */
    if(lex_match(fs, ':'))
    {
        TValue tv1, tv2;
        /* [n:] */
        if(lex_check(fs, ']'))
        {
            setnumV(&tv1, INFINITY);
            setnumV(&tv2, 1);
            bcemit_num(fs, &tv1);
            bcemit_num(fs, &tv2);
        }
        else
        {
            /* [n::n] */
            if(lex_match(fs, ':'))
            {
                setnumV(&tv1, INFINITY);
                bcemit_num(fs, &tv1);
                expr(fs);
            }
            else
            {
                expr(fs);
                if(lex_match(fs, ':'))
                {
                    /* [n:n:n] */
                    expr(fs);
                }
                else
                {
                    setnumV(&tv1, 1);
                    bcemit_num(fs, &tv1);
                }
            }
        }
        return true;
    }
    return false;
}

static void expr_subscript(FuncState* fs, bool assign)
{
    if(parse_slice(fs))
    {
        bcemit_op(fs, BC_RANGE);
    }

    lex_consume(fs, ']');

    int bc;
    if(assign && lex_match(fs, '='))
    {
        expr(fs);
        bcemit_op(fs, BC_SET_INDEX);
    }
    else if(assign && (bc = bcassign(fs)))
    {
        tea_lex_next(fs->ls);
        bcemit_op(fs, BC_PUSH_INDEX);
        expr(fs);
        bcemit_op(fs, bc);
        bcemit_op(fs, BC_SET_INDEX);
    }
    else
    {
        if(lex_match(fs, TK_PLUS_PLUS)) bc = BC_ADD;
        else if(lex_match(fs, TK_MINUS_MINUS)) bc = BC_SUBTRACT;
        else bc = 0;
        if(bc)
        {
            bcemit_op(fs, BC_PUSH_INDEX);
            TValue _v;
            setnumV(&_v, 1);
            bcemit_num(fs, &_v);
            bcemit_op(fs, bc);
            bcemit_op(fs, BC_SET_INDEX);
        }
        else
        {
            bcemit_op(fs, BC_GET_INDEX);
        }
    }
}

static void expr_or(FuncState* fs, bool assign)
{
    BCPos else_jmp = bcemit_jump(fs, BC_JUMP_IF_FALSE);
    BCPos jmp = bcemit_jump(fs, BC_JUMP);
    bcpatch_jump(fs, else_jmp);
    bcemit_op(fs, BC_POP);
    expr_precedence(fs, PREC_OR);
    bcpatch_jump(fs, jmp);
}

static void expr_num(FuncState* fs, bool assign)
{
    bcemit_num(fs, &fs->ls->prev.tv);
}

static void expr_str(FuncState* fs, bool assign)
{
    tea_State* T = fs->T;
    TValue tv;
    copyTV(T, &tv, &fs->ls->prev.tv);
    if((fs->ls->curr.t == '+') &&
        (fs->ls->next.t == TK_STRING))
    {
        SBuf* sb = tea_buf_tmp_(T);
        tea_buf_putstr(T, sb, strV(&tv));
        while((fs->ls->curr.t == '+') && (fs->ls->next.t == TK_STRING)) 
        {
            TValue* o = &fs->ls->next.tv;
            GCstr* s2 = strV(o);
            tea_buf_putstr(T, sb, s2);
            tea_lex_next(fs->ls);
            tea_lex_next(fs->ls);
        }
        GCstr* str = tea_buf_str(T, sb);
        setstrV(T, &tv, str);
    }
    bcemit_str(fs, &tv);
}

static void expr_interpolation(FuncState* fs, bool assign)
{
    bcemit_op(fs, BC_LIST);
    do
    {
        bcemit_str(fs, &fs->ls->prev.tv);
        bcemit_op(fs, BC_LIST_ITEM);

        expr(fs);
        bcemit_op(fs, BC_LIST_ITEM);
    }
    while(lex_match(fs, TK_INTERPOLATION));

    lex_consume(fs, TK_STRING);
    bcemit_str(fs, &fs->ls->prev.tv);
    bcemit_op(fs, BC_LIST_ITEM);
    bcemit_invoke(fs, 0, "join");
}

static void check_const(FuncState* fs, uint8_t set_op, int arg, GCstr* str)
{
    switch(set_op)
    {
        case BC_SET_LOCAL:
        {
            if(fs->locals[arg].isconst)
            {
                error(fs, TEA_ERR_XVCONST);
            }
            break;
        }
        case BC_SET_UPVALUE:
        {
            if(fs->upvalues[arg].isconst)
            {
                error(fs, TEA_ERR_XVCONST);
            }
            break;
        }
        case BC_SET_MODULE:
        {
            if(tea_tab_get(&fs->ls->T->constants, str))
            {
                error(fs, TEA_ERR_XVCONST);
            }
            break;
        }
        default:
            break;
    }
}

static void named_variable(FuncState* fs, Token name, bool assign)
{
    GCstr* str = NULL;
    uint8_t get_op, set_op;
    int arg = var_lookup_local(fs, &name);
    if(arg != -1)
    {
        get_op = BC_GET_LOCAL;
        set_op = BC_SET_LOCAL;
    }
    else if((arg = var_lookup_uv(fs, &name)) != -1)
    {
        get_op = BC_GET_UPVALUE;
        set_op = BC_SET_UPVALUE;
    }
    else
    {
        str = strV(&name.tv);
        arg = const_str(fs, str);
        get_op = BC_GET_MODULE;
        set_op = BC_SET_MODULE;
    }

    int bc;
    if(assign && lex_match(fs, '='))
    {
        check_const(fs, set_op, arg, str);
        expr(fs);
        bcemit_argued(fs, set_op, (uint8_t)arg);
    }
    else if(assign && (bc = bcassign(fs)))
    {
        tea_lex_next(fs->ls);
        check_const(fs, set_op, arg, str);
        bcemit_argued(fs, get_op, (uint8_t)arg);
        expr(fs);
        bcemit_op(fs, bc);
        bcemit_argued(fs, set_op, (uint8_t)arg);
    }
    else
    {
        if(lex_match(fs, TK_PLUS_PLUS)) bc = BC_ADD;
        else if(lex_match(fs, TK_MINUS_MINUS)) bc = BC_SUBTRACT;
        else bc = 0;
        if(bc)
        {
            check_const(fs, set_op, arg, str);
            bcemit_argued(fs, get_op, (uint8_t)arg);
            TValue _v;
            setnumV(&_v, 1);
            bcemit_num(fs, &_v);
            bcemit_op(fs, bc);
            bcemit_argued(fs, set_op, (uint8_t)arg);
        }
        else
        {
            bcemit_argued(fs, get_op, (uint8_t)arg);
        }
    }
}

static void expr_name(FuncState* fs, bool assign)
{
    Token name = fs->ls->prev;
    named_variable(fs, name, assign);
}

static void expr_super(FuncState* fs, bool assign)
{
    if(fs->klass == NULL)
    {
        error(fs, TEA_ERR_XSUPERO);
    }
    else if(!fs->klass->has_superclass)
    {
        error(fs, TEA_ERR_XSUPERK);
    }

    /* super */
    if(!lex_check(fs, '(') && !lex_check(fs, '.'))
    {
        named_variable(fs, lex_synthetic(fs, "super"), false);
        return;
    }

    /* super() -> super.init() */
    if(lex_match(fs, '('))
    {
        Token tok = lex_synthetic(fs, "new");
        uint8_t name = const_str(fs, strV(&tok.tv));
        named_variable(fs, lex_synthetic(fs, "self"), false);
        uint8_t nargs = parse_args(fs);
        named_variable(fs, lex_synthetic(fs, "super"), false);
        bcemit_argued(fs, BC_SUPER, name);
        bcemit_byte(fs, nargs);
        return;
    }

    /* super.name */
    lex_consume(fs, '.');
    lex_consume(fs, TK_NAME);
    uint8_t name = const_str(fs, strV(&fs->ls->prev.tv));

    named_variable(fs, lex_synthetic(fs, "self"), false);

    if(lex_match(fs, '('))
    {
        /* super.name() */
        uint8_t nargs = parse_args(fs);
        named_variable(fs, lex_synthetic(fs, "super"), false);
        bcemit_argued(fs, BC_SUPER, name);
        bcemit_byte(fs, nargs);
    }
    else
    {
        /* super.name */
        named_variable(fs, lex_synthetic(fs, "super"), false);
        bcemit_argued(fs, BC_GET_SUPER, name);
    }
}

static void expr_self(FuncState* fs, bool assign)
{
    if(fs->klass == NULL)
    {
        error(fs, TEA_ERR_XSELFO);
    }
    expr_name(fs, false);
}

static void expr_unary(FuncState* fs, bool assign)
{
    int op = fs->ls->prev.t;
    expr_precedence(fs, PREC_UNARY);
    switch(op)
    {
        case TK_NOT:
        case '!':
            bcemit_op(fs, BC_NOT);
            break;
        case '-':
            bcemit_op(fs, BC_NEGATE);
            break;
        case '~':
            bcemit_op(fs, BC_BNOT);
            break;
        default:
            return; /* Unreachable */
    }
}

static void expr_range(FuncState* fs, bool assign)
{
    LexToken op_type = fs->ls->prev.t;
    ParseRule rule = expr_rule(op_type);
    expr_precedence(fs, (Precedence)(rule.prec + 1));
    if(lex_match(fs, TK_DOT_DOT))
    {
        expr(fs);
    }
    else
    {
        TValue tv;
        setnumV(&tv, 1);
        bcemit_num(fs, &tv);
    }
    bcemit_op(fs, BC_RANGE);
}

/* Forward declarations */
static void expr_anonymous(FuncState* fs, bool assign);
static void expr_grouping(FuncState* fs, bool assign);

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
            return PREFIX(expr_num);
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
        case TK_SELF:
            return PREFIX(expr_self);
        case TK_TRUE:
            return PREFIX(expr_true);
        case TK_FALSE:
            return PREFIX(expr_false);
        default:
            return NONE;
    }
}

#undef NONE
#undef RULE
#undef INFIX
#undef PREFIX
#undef OPERATOR

static void expr_precedence(FuncState* fs, Precedence prec)
{
    tea_lex_next(fs->ls);
    ParseFn prefix_rule = expr_rule(fs->ls->prev.t).prefix;
    if(prefix_rule == NULL)
    {
        error(fs, TEA_ERR_XEXPR);
    }

    bool assign = prec <= PREC_ASSIGNMENT;
    prefix_rule(fs, assign);

    while(prec <= expr_rule(fs->ls->curr.t).prec)
    {
        tea_lex_next(fs->ls);
        ParseFn infix_rule = expr_rule(fs->ls->prev.t).infix;
        infix_rule(fs, assign);
    }

    if(assign && lex_match(fs, '='))
    {
        error(fs, TEA_ERR_XASSIGN);
    }
}

static void expr(FuncState* fs)
{
    expr_precedence(fs, PREC_ASSIGNMENT);
}

/* Forward declaration */
static void parse_decl(FuncState* fs, bool export);

/* Parse a block */
static void parse_block(FuncState* fs)
{
    lex_consume(fs, '{');
    while(!lex_check(fs, '}') && !lex_check(fs, TK_EOF))
    {
        parse_decl(fs, false);
    }
    lex_consume(fs, '}');
}

/* Parse a code block */
static void parse_code(FuncState* fs)
{
    scope_begin(fs);
    parse_block(fs);
    scope_end(fs);
}

/* Parse function parameters */
static void parse_params(FuncState* fs)
{
    scope_begin(fs);
    if(!lex_check(fs, ')'))
    {
        bool isopt = false;
        bool spread = false;
        do
        {
            if(spread)
            {
                error(fs, TEA_ERR_XSPREADARGS);
            }

            spread = lex_match(fs, TK_DOT_DOT_DOT);
            lex_consume(fs, TK_NAME);

            Token name = fs->ls->prev;
            if(var_lookup_local(fs, &name) != -1)
                error(fs, TEA_ERR_XDUPARGS);

            if(spread)
            {
                fs->flags |= PROTO_VARARG;
            }

            if(lex_match(fs, '='))
            {
                if(spread)
                {
                    error(fs, TEA_ERR_XSPREADOPT);
                }
                fs->numopts++;
                isopt = true;
                expr(fs);
            }
            else
            {
                fs->numparams++;
                if(isopt)
                {
                    error(fs, TEA_ERR_XOPT);
                }
            }

            if(fs->numparams + fs->numopts > 255)
            {
                error(fs, TEA_ERR_XMAXARGS);
            }

            var_declare(fs, &name);
            var_define(fs, &name, false, false);
        }
        while(lex_match(fs, ','));

        if(fs->numopts > 0)
        {
            bcemit_op(fs, BC_DEFINE_OPTIONAL);
            bcemit_bytes(fs, fs->numparams, fs->numopts);
        }
    }
    lex_consume(fs, ')');
}

static void parse_arrow(FuncState* fs)
{
    parse_params(fs);
    lex_consume(fs, TK_ARROW);
    if(lex_check(fs, '{'))
    {
        /* Brace so expect a block */
        parse_block(fs);
        bcemit_return(fs);
    }
    else
    {
        /* No brace, so expect single expression */
        expr(fs);
        bcemit_op(fs, BC_RETURN);
    }
}

/* Parse body of a function */
static void parse_body(LexState* ls, FuncInfo info, BCLine line)
{
    FuncState fs, *pfs = ls->fs;
    GCproto* pt;
    ptrdiff_t oldbase = pfs->bcbase - ls->bcstack;
    fs_init(ls, &fs, info);
    fs.linedefined = line;
    fs.bcbase = pfs->bcbase + pfs->pc;
    fs.bclimit = pfs->bclimit - pfs->pc;
    if(info != FUNC_ARROW)
    {
        lex_consume(&fs, '(');
        parse_params(&fs);
        parse_block(&fs);
        bcemit_return(&fs);
    }
    else
    {
        parse_arrow(&fs);
    }
    pt = fs_finish(ls, line);
    pfs->bcbase = ls->bcstack + oldbase;    /* May have been reallocated */
    pfs->bclimit = (BCPos)(ls->sizebcstack - oldbase);
    /* Store new prototype in the constant array of the parent */
    bcemit_argued(pfs, BC_CLOSURE, const_gc(pfs, (GCobj*)pt, TEA_TPROTO));
    if(!(pfs->flags & PROTO_CHILD))
    {
        pfs->flags |= PROTO_CHILD;
    }
}

static void expr_anonymous(FuncState* fs, bool assign)
{
    parse_body(fs->ls, FUNC_ANONYMOUS, fs->ls->prev.line);
}

static void expr_grouping(FuncState* fs, bool assign)
{
    LexToken curr = fs->ls->curr.t;
    LexToken next = fs->ls->next.t;

    /* () => ...; (...v) => ... */
    /* (a) => ...; (a, ) => ... */
    if((curr == ')' && curr == TK_DOT_DOT_DOT) || 
        (curr == TK_NAME && (next == ',' || next == ')')) || 
        (curr == ')' && next == TK_ARROW))
    {
        parse_body(fs->ls, FUNC_ARROW, fs->ls->prev.line);
        return;
    }

    expr(fs);
    lex_consume(fs, ')');
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

static void parse_operator(FuncState* fs)
{
    int i = 0;
    while(ops[i] != TK_EOF)
    {
        if(lex_match(fs, ops[i]))
            break;
        i++;
    }

    if(i == SENTINEL)
    {
        error(fs, TEA_ERR_XMETHOD);
    }

    GCstr* name = NULL;

    if(fs->ls->prev.t == '[')
    {
        lex_consume(fs, ']');
        if(lex_match(fs, '='))
            name = mmname_str(fs->ls->T, MM_SETINDEX);
        else
            name = mmname_str(fs->ls->T, MM_GETINDEX);
    }
    else
    {
        name = mmname_str(fs->ls->T, i);
    }

    TValue tv;
    setstrV(fs->ls->T, &tv, name);
    uint8_t k = const_str(fs, strV(&tv));

    fs->name = name;
    parse_body(fs->ls, FUNC_OPERATOR, fs->ls->prev.line);
    bcemit_argued(fs, BC_METHOD, k);
}

static void parse_method(FuncState* fs, FuncInfo info)
{
    uint8_t k = const_str(fs, strV(&fs->ls->prev.tv));
    parse_body(fs->ls, info, fs->ls->prev.line);
    bcemit_argued(fs, BC_METHOD, k);
}

/* Initialize a new KlassState */
static void ks_init(FuncState* fs, KlassState* ks)
{
    ks->has_superclass = false;
    ks->prev = fs->klass;
    fs->klass = ks;
}

/* Parse class methods */
static void parse_class_body(FuncState* fs)
{
    while(!lex_check(fs, '}') && !lex_check(fs, TK_EOF))
    {
        LexToken t = fs->ls->curr.t;
        switch(t)
        {
            case TK_NEW:
                tea_lex_next(fs->ls);
                parse_method(fs, FUNC_INIT);
                break;
            case TK_FUNCTION:
                tea_lex_next(fs->ls);
                lex_consume(fs, TK_NAME);
                parse_method(fs, FUNC_METHOD);
                break;
            case TK_OPERATOR:
                tea_lex_next(fs->ls);
                parse_operator(fs);
                break;
            default:
                error(fs, TEA_ERR_XMETHOD);
        }
    }
}

/* Parse 'class' declaration */
static void parse_class(FuncState* fs, bool export)
{
    tea_lex_next(fs->ls);  /* Skip 'class' */
    lex_consume(fs, TK_NAME);
    Token class_name = fs->ls->prev;
    uint8_t k = const_str(fs, strV(&class_name.tv));
    var_declare(fs, &class_name);

    bcemit_argued(fs, BC_CLASS, k);
    var_define(fs, &class_name, false, export);

    KlassState ks;
    ks_init(fs, &ks);

    if(lex_match(fs, ':'))
    {
        expr(fs);

        scope_begin(fs);
        var_add_local(fs, lex_synthetic(fs, "super"));
        var_define(fs, 0, false, false);

        named_variable(fs, class_name, false);
        bcemit_op(fs, BC_INHERIT);
        ks.has_superclass = true;
    }

    named_variable(fs, class_name, false);

    lex_consume(fs, '{');
    parse_class_body(fs);
    lex_consume(fs, '}');

    bcemit_op(fs, BC_POP);

    if(ks.has_superclass)
    {
        scope_end(fs);
    }

    fs->klass = fs->klass->prev;
}

static void parse_function_assign(FuncState* fs, BCLine line)
{
    if(lex_match(fs, '.'))
    {
        lex_consume(fs, TK_NAME);
        uint8_t k = const_str(fs, strV(&fs->ls->prev.tv));
        if(!lex_check(fs, '('))
        {
            bcemit_argued(fs, BC_GET_ATTR, k);
            parse_function_assign(fs, line);
        }
        else
        {
            parse_body(fs->ls, FUNC_NORMAL, line);
            bcemit_argued(fs, BC_SET_ATTR, k);
            bcemit_op(fs, BC_POP);
            return;
        }
    }
    else if(lex_match(fs, ':'))
    {
        lex_consume(fs, TK_NAME);
        uint8_t k = const_str(fs, strV(&fs->ls->prev.tv));

        KlassState ks;
        ks_init(fs, &ks);

        parse_body(fs->ls, FUNC_METHOD, line);

        fs->klass = fs->klass->prev;

        bcemit_argued(fs, BC_EXTENSION_METHOD, k);
        bcemit_op(fs, BC_POP);
        return;
    }
}

/* Parse 'function' declaration */
static void parse_function(FuncState* fs, bool export)
{
    tea_lex_next(fs->ls);  /* Skip 'function' */
    BCLine line = fs->ls->prev.line;
    lex_consume(fs, TK_NAME);
    Token name = fs->ls->prev;

    if((lex_check(fs, '.') || lex_check(fs, ':')) && !export)
    {
        named_variable(fs, name, false);
        parse_function_assign(fs, line);
        return;
    }

    var_declare(fs, &name);
    var_mark(fs, false);
    parse_body(fs->ls, FUNC_NORMAL, line);
    var_define(fs, &name, false, export);
}

/* Parse 'var' or 'const' declaration */
static void parse_var(FuncState* fs, bool isconst, bool export)
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
            error(fs, TEA_ERR_XDOTS);
        }

        if(lex_match(fs, TK_DOT_DOT_DOT))
        {
            rest = true;
            rest_count++;
        }

        lex_consume(fs, TK_NAME);
        vars[var_count] = fs->ls->prev;
        var_count++;

        if(rest)
        {
            rest_pos = var_count;
            rest = false;
        }

        if(var_count == 1 && lex_match(fs, '='))
        {
            if(rest_count)
            {
                error(fs, TEA_ERR_XSINGLEREST);
            }

            var_declare(fs, &vars[0]);
            expr(fs);
            var_define(fs, &vars[0], isconst, export);

            if(lex_match(fs, ','))
            {
                do
                {
                    lex_consume(fs, TK_NAME);
                    Token tok = fs->ls->prev;
                    var_declare(fs, &tok);
                    lex_consume(fs, '=');
                    expr(fs);
                    var_define(fs, &tok, isconst, export);
                }
                while(lex_match(fs, ','));
            }
            return;
        }
    }
    while(lex_match(fs, ','));

    if(rest_count)
    {
        lex_consume(fs, '=');
        expr(fs);
        bcemit_op(fs, BC_UNPACK_REST);
        bcemit_bytes(fs, var_count, rest_pos - 1);
        goto finish;
    }

    if(lex_match(fs, '='))
    {
        do
        {
            expr(fs);
            expr_count++;
            if(expr_count == 1 && (!lex_check(fs, ',')))
            {
                bcemit_argued(fs, BC_UNPACK, var_count);
                goto finish;
            }

        }
        while(lex_match(fs, ','));

        if(expr_count != var_count)
        {
            error(fs, TEA_ERR_XVALASSIGN);
        }
    }
    else
    {
        for(int i = 0; i < var_count; i++)
        {
            bcemit_op(fs, BC_NIL);
        }
    }

finish:
    if(fs->scope_depth == 0)
    {
        for(int i = var_count - 1; i >= 0; i--)
        {
            var_declare(fs, &vars[i]);
            var_define(fs, &vars[i], isconst, export);
        }
    }
    else
    {
        for(int i = 0; i < var_count; i++)
        {
            var_declare(fs, &vars[i]);
            var_define(fs, &vars[i], isconst, export);
        }
    }
}

/* Parse an expression statement */
static void parse_expr_stmt(FuncState* fs)
{
    expr(fs);
    bcemit_op(fs, BC_POP);
}

/* Parse iterable 'for' */
static void parse_for_in(FuncState* fs, Token var, bool isconst)
{
    Token vars[255];
    int var_count = 1;
    vars[0] = var;

    if(lex_match(fs, ','))
    {
        do
        {
            lex_consume(fs, TK_NAME);
            vars[var_count] = fs->ls->prev;
            var_count++;
        }
        while(lex_match(fs, ','));
    }

    lex_consume(fs, TK_IN);

    expr(fs);
    int seq_slot = var_add_local(fs, lex_synthetic(fs, "seq "));
    var_mark(fs, false);

    expr_nil(fs, false);
    int iter_slot = var_add_local(fs, lex_synthetic(fs, "iter "));
    var_mark(fs, false);

    Loop loop;
    loop_begin(fs, &loop);

    /* Get the iterator index. If it's nil, it means the loop is over */
    bcemit_op(fs, BC_GET_ITER);
    bcemit_bytes(fs, seq_slot, iter_slot);
    fs->loop->end = bcemit_jump(fs, BC_JUMP_IF_NIL);
    bcemit_op(fs, BC_POP);

    /* Get the iterator value */
    bcemit_op(fs, BC_FOR_ITER);
    bcemit_bytes(fs, seq_slot, iter_slot);

    scope_begin(fs);

    if(var_count > 1)
        bcemit_argued(fs, BC_UNPACK, var_count);

    for(int i = 0; i < var_count; i++)
    {
        var_declare(fs, &vars[i]);
        var_define(fs, &vars[i], isconst, false);
    }

    fs->loop->body = fs->pc;
    parse_code(fs);

    /* Loop variable */
    scope_end(fs);

    bcemit_loop(fs, fs->loop->start);
    loop_end(fs);

    /* Hidden variables */
    scope_end(fs);
}

/* Parse generic 'for' */
static void parse_for(FuncState* fs)
{
    scope_begin(fs);

    tea_lex_next(fs->ls);  /* Skip 'for' */

    int loop_var = -1;
    Token var_name = fs->ls->curr;

    bool isconst = false;
    if(lex_match(fs, TK_VAR) || (isconst = lex_match(fs, TK_CONST)))
    {
        lex_consume(fs, TK_NAME);
        Token var = fs->ls->prev;

        if(lex_check(fs, TK_IN) || lex_check(fs, ','))
        {
            /* It's a for in statement */
            parse_for_in(fs, var, isconst);
            return;
        }

        /* Grab the name of the loop variable */
        var_name = var;

        var_declare(fs, &var);

        if(lex_match(fs, '='))
        {
            expr(fs);
        }
        else
        {
            bcemit_op(fs, BC_NIL);
        }

        var_define(fs, &var, isconst, false);
        lex_consume(fs, ';');

        /* Get loop variable slot */
        loop_var = fs->local_count - 1;
    }
    else
    {
        parse_expr_stmt(fs);
        lex_consume(fs, ';');
    }

    Loop loop;
    loop_begin(fs, &loop);

    fs->loop->end = -1;

    expr(fs);
    lex_consume(fs, ';');

    fs->loop->end = bcemit_jump(fs, BC_JUMP_IF_FALSE);
    bcemit_op(fs, BC_POP); /* Condition */

    BCPos body_jmp = bcemit_jump(fs, BC_JUMP);

    BCPos increment_start = fs->pc;
    expr(fs);
    bcemit_op(fs, BC_POP);

    bcemit_loop(fs, fs->loop->start);
    fs->loop->start = increment_start;

    bcpatch_jump(fs, body_jmp);

    fs->loop->body = fs->pc;

    int inner_var = -1;
    if(loop_var != -1)
    {
        scope_begin(fs);
        bcemit_argued(fs, BC_GET_LOCAL, (uint8_t)loop_var);
        var_add_local(fs, var_name);
        var_mark(fs, false);
        inner_var = fs->local_count - 1;
    }

    parse_code(fs);

    if(inner_var != -1)
    {
        bcemit_argued(fs, BC_GET_LOCAL, (uint8_t)inner_var);
        bcemit_argued(fs, BC_SET_LOCAL, (uint8_t)loop_var);
        bcemit_op(fs, BC_POP);
        scope_end(fs);
    }

    bcemit_loop(fs, fs->loop->start);

    loop_end(fs);
    scope_end(fs);
}

/* Parse 'break' statement */
static void parse_break(FuncState* fs)
{
    tea_lex_next(fs->ls);  /* Skip 'break' */
    if(fs->loop == NULL)
    {
        error(fs, TEA_ERR_XBREAK);
    }

    /* Discard any locals created inside the loop */
    scope_discard(fs, fs->loop->scope_depth + 1);
    bcemit_jump(fs, BC_END);
}

/* Parse 'continue' statement */
static void parse_continue(FuncState* fs)
{
    tea_lex_next(fs->ls);  /* Skip 'continue' */
    if(fs->loop == NULL)
    {
        error(fs, TEA_ERR_XCONTINUE);
    }

    /* Discard any locals created inside the loop */
    scope_discard(fs, fs->loop->scope_depth + 1);

    /* Jump to the top of the innermost loop */
    bcemit_loop(fs, fs->loop->start);
}

/* Parse 'if' statement */
static void parse_if(FuncState* fs)
{
    tea_lex_next(fs->ls);  /* Skip 'if' */
    expr(fs);

    BCPos else_jmp = bcemit_jump(fs, BC_JUMP_IF_FALSE);
    bcemit_op(fs, BC_POP);
    
    parse_code(fs);

    BCPos end_jmp = bcemit_jump(fs, BC_JUMP);

    bcpatch_jump(fs, else_jmp);
    bcemit_op(fs, BC_POP);

    if(lex_match(fs, TK_ELSE))
    {
        if(lex_check(fs, TK_IF))
            parse_if(fs);
        else
            parse_code(fs);
    }

    bcpatch_jump(fs, end_jmp);
}

/* Parse 'switch' statement */
static void parse_switch(FuncState* fs)
{
    int case_ends[256];
    int case_count = 0;

    tea_lex_next(fs->ls);  /* Skip 'switch' */
    expr(fs);
    lex_consume(fs, '{');

    if(lex_match(fs, TK_CASE))
    {
        do
        {
            expr(fs);
            int multiple_cases = 0;
            if(lex_match(fs, ','))
            {
                do
                {
                    multiple_cases++;
                    expr(fs);
                }
                while(lex_match(fs, ','));
                bcemit_argued(fs, BC_MULTI_CASE, multiple_cases);
            }
            BCPos jmp = bcemit_jump(fs, BC_COMPARE_JUMP);
            parse_code(fs);
            case_ends[case_count++] = bcemit_jump(fs, BC_JUMP);
            bcpatch_jump(fs, jmp);
            if(case_count > 255)
            {
                error(fs, TEA_ERR_XSWITCH);
            }

        }
        while(lex_match(fs, TK_CASE));
    }

    bcemit_op(fs, BC_POP); /* Expression */
    if(lex_match(fs, TK_DEFAULT))
    {
        parse_code(fs);
    }

    if(lex_match(fs, TK_CASE))
    {
        error(fs, TEA_ERR_XCASE);
    }

    lex_consume(fs, '}');

    for(int i = 0; i < case_count; i++)
    {
    	bcpatch_jump(fs, case_ends[i]);
    }
}

/* Parse 'return' statement */
static void parse_return(FuncState* fs)
{
    tea_lex_next(fs->ls);  /* Skip 'return' */

    if(fs->info == FUNC_SCRIPT)
    {
        error(fs, TEA_ERR_XRET);
    }

    if(lex_check(fs, '}') || lex_match(fs, ';'))
    {
        bcemit_return(fs);
    }
    else
    {
        if(fs->info == FUNC_INIT)
        {
            error(fs, TEA_ERR_XINIT);
        }

        expr(fs);
        bcemit_op(fs, BC_RETURN);
    }
}

/* Parse 'import name' statement */
static void parse_import_name(FuncState* fs)
{
    lex_consume(fs, TK_NAME);
    Token name = fs->ls->prev;
    uint8_t k = const_str(fs, strV(&name.tv));
    bcemit_argued(fs, BC_IMPORT_NAME, k);
    bcemit_op(fs, BC_POP);

    if(lex_match(fs, TK_AS))
    {
        lex_consume(fs, TK_NAME);
        name = fs->ls->prev;
    }
    
    var_declare(fs, &name);
    bcemit_op(fs, BC_IMPORT_ALIAS);
    var_define(fs, &name, false, false);

    bcemit_op(fs, BC_IMPORT_END);

    if(lex_match(fs, ','))
    {
        parse_import_name(fs);
    }
}

/* Parse 'import <string>' statement */
static void parse_import_string(FuncState* fs)
{
    if(lex_match(fs, TK_STRING))
    {
        uint8_t k = const_str(fs, strV(&fs->ls->prev.tv));
        bcemit_argued(fs, BC_IMPORT_STRING, k);
    }
    else
    {
        lex_consume(fs, TK_INTERPOLATION);
        expr_interpolation(fs, false);
        bcemit_op(fs, BC_IMPORT_FMT);
    }
    bcemit_op(fs, BC_POP);

    if(lex_match(fs, TK_AS))
    {
        lex_consume(fs, TK_NAME);
        Token name = fs->ls->prev;
        var_declare(fs, &name);
        bcemit_op(fs, BC_IMPORT_ALIAS);
        var_define(fs, &name, false, false);
    }

    bcemit_op(fs, BC_IMPORT_END);

    if(lex_match(fs, ','))
    {
        parse_import_string(fs);
    }
}

/* Parse 'from' statement */
static void parse_from(FuncState* fs)
{
    if(lex_match(fs, TK_STRING))
    {
        uint8_t k = const_str(fs, strV(&fs->ls->prev.tv));
        bcemit_argued(fs, BC_IMPORT_STRING, k);
    }
    else if(lex_match(fs, TK_INTERPOLATION))
    {
        expr_interpolation(fs, false);
        bcemit_op(fs, BC_IMPORT_FMT);
    }
    else
    {
        lex_consume(fs, TK_NAME);
        uint8_t k = const_str(fs, strV(&fs->ls->prev.tv));
        bcemit_argued(fs, BC_IMPORT_NAME, k);
    }
    lex_consume(fs, TK_IMPORT);
    bcemit_op(fs, BC_POP);

    do
    {
        lex_consume(fs, TK_NAME);
        Token name = fs->ls->prev;
        uint8_t k = const_str(fs, strV(&name.tv));

        if(lex_match(fs, TK_AS))
        {
            lex_consume(fs, TK_NAME);
            name = fs->ls->prev;
        }
        var_declare(fs, &name);
        bcemit_argued(fs, BC_IMPORT_VARIABLE, k);
        var_define(fs, &name, false, false);
    }
    while(lex_match(fs, ','));

    bcemit_op(fs, BC_IMPORT_END);
}

/* Parse 'while' statement */
static void parse_while(FuncState* fs)
{
    Loop loop;
    loop_begin(fs, &loop);

    tea_lex_next(fs->ls);  /* Skip 'while' */
    if(lex_check(fs, '{'))
    {
        bcemit_byte(fs, BC_TRUE);
    }
    else
    {
        expr(fs);
    }

    /* Jump ot of the loop if the condition is false */
    fs->loop->end = bcemit_jump(fs, BC_JUMP_IF_FALSE);
    bcemit_op(fs, BC_POP);

    /* Compile the body */
    fs->loop->body = fs->pc;
    parse_code(fs);

    /* Loop back to the start */
    bcemit_loop(fs, fs->loop->start);
    loop_end(fs);
}

/* Parse 'do' statement */
static void parse_do(FuncState* fs)
{
    Loop loop;
    loop_begin(fs, &loop);

    tea_lex_next(fs->ls);  /* Skip 'do' */

    fs->loop->body = fs->pc;
    parse_code(fs);

    lex_consume(fs, TK_WHILE);
    expr(fs);

    fs->loop->end = bcemit_jump(fs, BC_JUMP_IF_FALSE);
    bcemit_op(fs, BC_POP);

    bcemit_loop(fs, fs->loop->start);
    loop_end(fs);
}

static void parse_multiple_assign(FuncState* fs)
{
    int expr_count = 0;
    int var_count = 0;
    Token vars[255];

    do
    {
        lex_consume(fs, TK_NAME);
        vars[var_count] = fs->ls->prev;
        var_count++;
    }
    while(lex_match(fs, ','));

    lex_consume(fs, '=');

    do
    {
        expr(fs);
        expr_count++;
        if(expr_count == 1 && (!lex_check(fs, ',')))
        {
            bcemit_argued(fs, BC_UNPACK, var_count);
            goto finish;
        }
    }
    while(lex_match(fs, ','));

    if(expr_count != var_count)
    {
        error(fs, TEA_ERR_XVASSIGN);
    }

finish:
    for(int i = var_count - 1; i >= 0; i--)
    {
        GCstr* str = NULL;
        Token tok = vars[i];
        uint8_t set_op;
        int arg = var_lookup_local(fs, &tok);
        if(arg != -1)
        {
            set_op = BC_SET_LOCAL;
        }
        else if((arg = var_lookup_uv(fs, &tok)) != -1)
        {
            set_op = BC_SET_UPVALUE;
        }
        else
        {
            str = strV(&tok.tv);
            arg = const_str(fs, str);
            set_op = BC_SET_MODULE;
        }
        check_const(fs, set_op, arg, str);
        bcemit_argued(fs, set_op, (uint8_t)arg);
        bcemit_op(fs, BC_POP);
    }
}

static void parse_export(FuncState* fs)
{
    tea_lex_next(fs->ls);
    if(fs->info != FUNC_SCRIPT)
    {
        error(fs, TEA_ERR_XRET);
    }
    LexToken t = fs->ls->curr.t;
    if(t == TK_CLASS || t == TK_FUNCTION || t == TK_CONST || t == TK_VAR)
    {
        parse_decl(fs, true);
        return;
    }
    lex_consume(fs, '{');
    while(!lex_check(fs, '}') && !lex_check(fs, TK_EOF))
    {
        do
        {
            Token name;
            lex_consume(fs, TK_NAME);
            name = fs->ls->prev;
            uint8_t k = const_str(fs, strV(&name.tv));
            bcemit_argued(fs, BC_GET_MODULE, k);
            bcemit_argued(fs, BC_DEFINE_MODULE, k);
            bcemit_byte(fs, 2);
        }
        while(lex_match(fs, ','));
    }
    lex_consume(fs, '}');
}

/* -- Parse statements and declarations ---------------------------------------------------- */

/* Parse a statement */
static void parse_stmt(FuncState* fs)
{
    switch(fs->ls->curr.t)
    {
        case ';':
            tea_lex_next(fs->ls);
            break;
        case TK_FOR:
            parse_for(fs);
            break;
        case TK_IF:
            parse_if(fs);
            break;
        case TK_SWITCH:
            parse_switch(fs);
            break;
        case TK_EXPORT:
            parse_export(fs);
            break;
        case TK_RETURN:
            parse_return(fs);
            break;
        case TK_WHILE:
            parse_while(fs);
            break;
        case TK_DO:
            parse_do(fs);
            break;
        case TK_IMPORT:
            tea_lex_next(fs->ls);  /* Skip 'import' */
            if(lex_check(fs, TK_STRING) ||
                lex_check(fs, TK_INTERPOLATION))
                parse_import_string(fs);
            else
                parse_import_name(fs);
            break;
        case TK_FROM:
            tea_lex_next(fs->ls);  /* Skip 'from' */
            parse_from(fs);
            break;
        case TK_BREAK:
            parse_break(fs);
            break;
        case TK_CONTINUE:
            parse_continue(fs);
            break;
        case TK_NAME:
        {
            if(fs->ls->next.t == ',')
                parse_multiple_assign(fs);
            else
                parse_expr_stmt(fs);
            break;
        }
        case '{':
        {
            parse_code(fs);
            break;
        }
        default:
            parse_expr_stmt(fs);
            break;
    }
}

/* Parse a declaration */
static void parse_decl(FuncState* fs, bool export)
{
    LexToken t = fs->ls->curr.t;
    switch(t)
    {
        case TK_CLASS:
            parse_class(fs, export);
            break;
        case TK_FUNCTION:
            parse_function(fs, export);
            break;
        case TK_CONST:
        case TK_VAR:
            tea_lex_next(fs->ls);  /* Skip 'const' or 'var' */
            parse_var(fs, t == TK_CONST, export);
            break;
        default:
            parse_stmt(fs);
            break;
    }
}

/* Entry point of bytecode parser */
GCproto* tea_parse(LexState* ls, bool isexpr)
{
    FuncState fs;
    GCproto* pt;
    tea_State* T = ls->T;
    fs_init(ls, &fs, FUNC_SCRIPT);
    fs.linedefined = 0;
    fs.bcbase = NULL;
    fs.bclimit = 0;

    tea_lex_next(fs.ls);   /* Read the first token into "next" */
    tea_lex_next(fs.ls);   /* Copy "next" -> "curr" */

    if(isexpr)
    {
        expr(&fs);
        bcemit_op(&fs, BC_RETURN);
        lex_consume(&fs, TK_EOF);
    }
    else
    {
        while(!lex_match(&fs, TK_EOF))
        {
            parse_decl(&fs, false);
        }
        bcemit_return(&fs);
    }
    if(strncmp(str_data(ls->module->name), "=<stdin>", 8) != 0)
    {
        tea_tab_free(T, &T->constants);
    }
    pt = fs_finish(ls, ls->linenumber);
    tea_assertT(fs.prev == NULL && ls->fs == NULL, "mismatched frame nesting");
    tea_assertT(pt->sizeuv == 0, "top level proto has upvalues");
    return pt;
}