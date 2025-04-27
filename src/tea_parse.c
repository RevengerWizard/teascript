/*
** tea_parse.c
** Teascript parser (source code -> bytecode)
*/

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

/* Precedence levels for operators */
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
} Prec;

typedef struct
{
    GCstr* name;
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

/* Per-class state */
typedef struct KlassState
{
    struct KlassState* prev; /* Enclosing class state */
    bool isinherit;
    bool isstatic;
} KlassState;

typedef struct Loop
{
    struct Loop* prev; /* Enclosing loop */
    BCPos start;    /* Start of loop */
    BCPos body; /* Start of loop body */
    int end;    /* End of loop or -1 */
    int scope_depth;    /* Scope depth of loop */
} Loop;

typedef enum
{
    FUNC_NORMAL,
    FUNC_ANONYMOUS,
    FUNC_ARROW,
    FUNC_INIT,
    FUNC_METHOD,
    FUNC_STATIC,
    FUNC_GETTER,
    FUNC_SETTER,
    FUNC_OPERATOR,
    FUNC_SCRIPT
} FuncInfo;

/* Per-function state */
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
    uint16_t varnum;  /* Number of variables */
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
    Prec prec;
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

/* Synthetic token */
static Token lex_synthetic(FuncState* fs, const char* text)
{
    Token tok;
    GCstr* s = tea_str_newlen(fs->ls->T, text);
    setstrV(fs->ls->T, &tok.tv, s);
    return tok;
}

/* Check for matching token and consume it */
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

/* Emit a byte, which can be used for opcodes or arguments */
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

/* Emit 2 bytes */
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

/* Emit a bytecode instruction with a byte argument */
static void bcemit_arg(FuncState* fs, BCOp op, uint8_t byte)
{
    bcemit_bytes(fs, op, byte);
    fs->max_slots += bc_effects[op];
}

/* Emit a loop instruction */
static void bcemit_loop(FuncState* fs, BCPos start)
{
    bcemit_op(fs, BC_LOOP);
    BCPos ofs = fs->pc - start + 2;
    if(ofs > UINT16_MAX)
        error(fs, TEA_ERR_XLOOP);
    bcemit_byte(fs, (ofs >> 8) & 0xff);
    bcemit_byte(fs, ofs & 0xff);
}

/* Emit a jump instruction */
static BCPos bcemit_jump(FuncState* fs, BCOp op)
{
    bcemit_op(fs, op);
    bcemit_bytes(fs, 0xff, 0xff);
    return fs->pc - 2;
}

/* Emit a return instruction */
static void bcemit_return(FuncState* fs)
{
    if(fs->info == FUNC_INIT)
    {
        bcemit_arg(fs, BC_GETLOCAL, 0);
    }
    else
    {
        bcemit_op(fs, BC_KNIL);
    }
    bcemit_op(fs, BC_RETURN);
}

/* Emit an invoke instruction */
static void bcemit_invoke(FuncState* fs, int args, const char* name)
{
    GCstr* str = tea_str_new(fs->T, name, strlen(name));
    bcemit_arg(fs, BC_INVOKE, const_str(fs, str));
    bcemit_byte(fs, args);
}

static void bcemit_num(FuncState* fs, double n)
{
    TValue tv; setnumV(&tv, n);
    bcemit_arg(fs, BC_CONSTANT, const_num(fs, &tv));
}

static void bcemit_str(FuncState* fs, TValue* o)
{
    bcemit_arg(fs, BC_CONSTANT, const_str(fs, strV(o)));
}

/* Patch jump instruction */
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
    for(uint32_t i = 0; i < kt->size; i++)
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
    tea_assertT(ls->fs != NULL || ls->curr.t == TK_eof, "bad parser state");
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
        fs->klass = fs->prev->klass;
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
        case FUNC_STATIC:
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
    local->init = true;
    local->isconst = false;
    local->isupval = false;

    switch(info)
    {
        case FUNC_SCRIPT:
        case FUNC_NORMAL:
        case FUNC_ANONYMOUS:
        case FUNC_ARROW:
        case FUNC_STATIC:
            local->name = &T->strempty;
            break;
        case FUNC_INIT:
        case FUNC_METHOD:
        case FUNC_OPERATOR:
            local->name = tea_str_newlit(T, "self");
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

/* Close locals with open upvalues in a scope */
static int scope_close(FuncState* fs, int depth)
{
    int local;
    for(local = fs->local_count - 1; local >= 0 && fs->locals[local].depth >= depth; local--)
    {
        if(fs->locals[local].isupval)
        {
            bcemit_op(fs, BC_CLOSEUPVAL);
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
    int effect = scope_close(fs, fs->scope_depth);
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

/* Begin a loop */
static void loop_begin(FuncState* fs, Loop* loop)
{
    loop->start = fs->pc;
    loop->scope_depth = fs->scope_depth;
    loop->prev = fs->loop;
    fs->loop = loop;
}

/* End a loop, patch jumps and fixup bytecode */
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
            fs->bcbase[i].ins = BC_JMP;
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

/* Lookup a local variable by name */
static int var_lookup_local(FuncState* fs, Token* tok)
{
    for(int i = fs->local_count - 1; i >= 0; i--)
    {
        Local* local = &fs->locals[i];
        if(strV(&tok->tv) == local->name)
        {
            if(!local->init)
                break;
            return i;
        }
    }
    return -1;
}

/* Map an upvalue to the FuncState */
static int var_add_uv(FuncState* fs, uint8_t idx, bool islocal, bool isconst)
{
    uint32_t i, n = fs->nuv;
    for(i = 0; i < n; i++)
    {
        Upvalue* uv = &fs->upvalues[i];
        if(uv->index == idx && uv->islocal == islocal)
            return i;
    }
    checklimit(fs, n, TEA_MAX_UPVAL, "closure variables");
    fs->upvalues[n].islocal = islocal;
    fs->upvalues[n].isconst = isconst;
    fs->upvalues[n].index = idx;
    fs->uvmap[n] = (uint16_t)(((islocal) << 8) | (idx));
    return fs->nuv++;
}

/* Lookup an upvalue by name */
static int var_lookup_uv(FuncState* fs, Token* tok)
{
    if(fs->prev == NULL)
        return -1;

    bool isconst;
    int idx = var_lookup_local(fs->prev, tok);
    if(idx != -1)
    {
        isconst = fs->prev->locals[idx].isconst;
        fs->prev->locals[idx].isupval = true;
        return var_add_uv(fs, (uint8_t)idx, true, isconst);
    }

    idx = var_lookup_uv(fs->prev, tok);
    if(idx != -1)
    {
        isconst = fs->prev->upvalues[idx].isconst;
        return var_add_uv(fs, (uint8_t)idx, false, isconst);
    }

    return -1;
}

/* Mark a variable as constant */
static void var_mark(FuncState* fs, bool isconst)
{
    if(fs->scope_depth == 0)
    {
        fs->ls->vstack[fs->ls->vtop - 1].isconst = isconst;
        fs->ls->vstack[fs->ls->vtop - 1].init = true;
    }
    else
    {
        fs->locals[fs->local_count - 1].isconst = isconst;
        fs->locals[fs->local_count - 1].init = true;
    }
}

/* Add a local variable */
static int var_add_local(FuncState* fs, Token tok)
{
    checklimit(fs, fs->local_count, TEA_MAX_LOCAL, "local variables");
    int idx = var_lookup_local(fs, &tok);
    if(idx != -1 && fs->locals[idx].depth == fs->scope_depth)
    {
        GCstr* name = fs->locals[idx].name;
        tea_lex_error(fs->ls, fs->ls->prev.t, fs->ls->prev.line, TEA_ERR_XDECL, str_data(name));
    }
    Local* local = &fs->locals[fs->local_count++];
    local->name = strV(&tok.tv);
    local->depth = fs->scope_depth;
    local->isupval = false;
    local->isconst = false;
    local->init = false;
    return fs->local_count - 1;
}

/* Lookup a top-level variable by name */
static int var_lookup_var(FuncState* fs, Token* tok)
{
    for(int i = 0; i < fs->ls->vtop; i++)
    {
        VarInfo* v = &fs->ls->vstack[i];
        if(strV(&tok->tv) == v->name)
        {
            if(!v->init && fs->scope_depth == 0)
                break;
            return i;
        }
    }
    return -1;
}

/* Add a top-level variable */
static void var_add_var(FuncState* fs, Token* tok, bool init)
{
    LexState* ls = fs->ls;
    uint32_t vtop = ls->vtop;
    checklimit(fs, fs->varnum, TEA_MAX_VAR, "variables");
    int idx = var_lookup_var(fs, tok);
    if(idx != -1)
    {
        tea_lex_error(ls, ls->prev.t, ls->prev.line, TEA_ERR_XDECL, str_data(ls->vstack[idx].name));
    }
    if(TEA_UNLIKELY(vtop >= ls->sizevstack))
    {
        ls->vstack = tea_mem_growvec(ls->T, VarInfo, ls->vstack, ls->sizevstack, TEA_MAX_VAR);
    }
    fs->varnum++;
    /* Add new var info to the vtop of vstack */
    ls->vtop = vtop + 1;
    VarInfo* v = &ls->vstack[vtop]; 
    v->name = strV(&tok->tv);
    v->isconst = false;
    v->init = init;
}

/* Declare a variable */
static void var_declare(FuncState* fs, Token* tok)
{
    if(fs->scope_depth == 0)
        var_add_var(fs, tok, false);
    else
        var_add_local(fs, *tok);
}

/* Define a variable */
static void var_define(FuncState* fs, Token* tok, bool isconst, bool export)
{
    var_mark(fs, isconst);
    if(fs->scope_depth == 0)
    {
        uint8_t idx = fs->ls->vtop - 1;
        bcemit_arg(fs, BC_SETMODULE, idx);
        if(export)
        {
            GCstr* str = strV(&tok->tv);
            uint8_t k = const_str(fs, str);
            bcemit_arg(fs, BC_DEFMODULE, k);
        }
        bcemit_byte(fs, BC_POP);
    }
}

/* Find get/set bytecodes for variable */
static int var_find(FuncState* fs, Token* tok, bool assign, uint8_t* getbc, uint8_t* setbc)
{
    GCstr* name = strV(&tok->tv);
    int arg = var_lookup_local(fs, tok);
    if(arg != -1)
    {
        *getbc = BC_GETLOCAL;
        *setbc = BC_SETLOCAL;
    }
    else if((arg = var_lookup_uv(fs, tok)) != -1)
    {
        *getbc = BC_GETUPVAL;
        *setbc = BC_SETUPVAL;
    }
    else if((arg = var_lookup_var(fs, tok)) != -1)
    {
        *getbc = BC_GETMODULE;
        *setbc = BC_SETMODULE;
    }
    else if(!assign && tea_tab_get(&fs->T->globals, name) != NULL)
    {
        arg = const_str(fs, name);
        *getbc = BC_GETGLOBAL;
    }
    else
    {
        tea_lex_error(fs->ls, fs->ls->prev.t, fs->ls->prev.line, TEA_ERR_XVAR, str_data(name));
    }
    return arg;
}

/* -- Variable fixup ------------------------------------------------------ */

/* Fixup residue variables from module */ 
static void var_fixup_start(FuncState* fs)
{
    GCmodule* mod = fs->ls->module;
    /* Add remaining module variables to the vstack */
    if(mod->vars != NULL)
    {
        for(int i = 0; i < mod->size; i++)
        {
            GCstr* name = mod->varnames[i];
            Token tok;
            setstrV(fs->ls->T, &tok.tv, name);
            var_add_var(fs, &tok, true);
        }
    }
}

/* Fixup module arrays of variable names and slots */
static void var_fixup_end(FuncState* fs)
{
    tea_State* T = fs->T;
    GCmodule* mod = fs->ls->module;
    uint16_t oldsize = mod->size;
    uint16_t newsize = fs->varnum;
    /* Allocate new arrays for module variables */
    mod->size = newsize;
    mod->vars = tea_mem_reallocvec(T, TValue, mod->vars, oldsize, newsize);
    mod->varnames = tea_mem_reallocvec(T, GCstr*, mod->varnames, oldsize, newsize);
    for(uint16_t i = oldsize; i < newsize; i++)
    {
        GCstr* name = fs->ls->vstack[i].name;
        tea_assertT(name != NULL, "bad variable name"); 
        mod->varnames[i] = name;
        setnilV(&mod->vars[i]);
    }
}

/* -- Expressions --------------------------------------------------------- */

/* Forward declarations */
static ParseRule expr_rule(int type);
static void expr_prec(FuncState* fs, Prec prec);

/* Parse logical and expression */
static void expr_and(FuncState* fs, bool assign)
{
    BCPos jmp = bcemit_jump(fs, BC_JMPFALSE);
    bcemit_op(fs, BC_POP);
    expr_prec(fs, PREC_AND);
    bcpatch_jump(fs, jmp);
}

/* Parse binary expression */
static void expr_binary(FuncState* fs, bool assign)
{
    LexToken tok = fs->ls->prev.t;
    if(tok == TK_not)
    {
        lex_consume(fs, TK_in);
        ParseRule rule = expr_rule(tok);
        expr_prec(fs, (Prec)(rule.prec + 1));
        bcemit_ops(fs, BC_IN, BC_NOT);
        return;
    }

    if(tok == TK_is && lex_match(fs, TK_not))
    {
        ParseRule rule = expr_rule(tok);
        expr_prec(fs, (Prec)(rule.prec + 1));
        bcemit_ops(fs, BC_IS, BC_NOT);
        return;
    }

    ParseRule rule = expr_rule(tok);
    expr_prec(fs, (Prec)(rule.prec + 1));
    switch(tok)
    {
        case TK_noteq:
            bcemit_ops(fs, BC_ISEQ, BC_NOT);
            break;
        case TK_eq:
            bcemit_op(fs, BC_ISEQ);
            break;
        case TK_is:
            bcemit_op(fs, BC_IS);
            break;
        case '>':
            bcemit_op(fs, BC_ISGT);
            break;
        case TK_ge:
            bcemit_op(fs, BC_ISGE);
            break;
        case '<':
            bcemit_op(fs, BC_ISLT);
            break;
        case TK_le:
            bcemit_op(fs, BC_ISLE);
            break;
        case '+':
            bcemit_op(fs, BC_ADD);
            break;
        case '-':
            bcemit_op(fs, BC_SUB);
            break;
        case '*':
            bcemit_op(fs, BC_MUL);
            break;
        case '/':
            bcemit_op(fs, BC_DIV);
            break;
        case '%':
            bcemit_op(fs, BC_MOD);
            break;
        case TK_pow:
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
        case TK_rshift:
            bcemit_op(fs, BC_RSHIFT);
            break;
        case TK_lshift:
            bcemit_op(fs, BC_LSHIFT);
            break;
        case TK_in:
            bcemit_op(fs, BC_IN);
            break;
        default:
            tea_assertFS(0, "unknown binary op");
            return; /* Unreachable */
    }
}

/* Forward declaration */
static void expr(FuncState* fs);

/* Parse ternary expression ? : */
static void expr_ternary(FuncState* fs, bool assign)
{
    /* Jump to else branch if the condition is false */
    BCPos else_jmp = bcemit_jump(fs, BC_JMPFALSE);

    /* Pop the condition */
    bcemit_op(fs, BC_POP);
    expr(fs);

    BCPos end_jmp = bcemit_jump(fs, BC_JMP);

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

/* Parse expression call */
static void expr_call(FuncState* fs, bool assign)
{
    uint8_t nargs = parse_args(fs);
    bcemit_arg(fs, BC_CALL, nargs);
}

/* Convert token to bytecode assignment */
static BCOp tok2bcassign(FuncState* fs)
{
    switch(fs->ls->curr.t)
    {
        case TK_pluseq:
            return BC_ADD;
        case TK_mineq:
            return BC_SUB;
        case TK_muleq:
            return BC_MUL;
        case TK_diveq:
            return BC_DIV;
        case TK_modeq:
            return BC_MOD;
        case TK_poweq:
            return BC_POW;
        case TK_bandeq:
            return BC_BAND;
        case TK_boreq:
            return BC_BOR;
        case TK_bxoreq:
            return BC_BXOR;
        case TK_lshifteq:
            return BC_LSHIFT;
        case TK_rshifteq:
            return BC_RSHIFT;
        default:
            return 0;
    }
}

/* Parse attribute expression with named field */
static void expr_dot(FuncState* fs, bool assign)
{
    bool isnew = false;
    if(lex_match(fs, TK_new))
    {
        isnew = true;
    }
    else
    {
        lex_consume(fs, TK_name);
    }

    uint8_t name = const_str(fs, strV(&fs->ls->prev.tv));
    if(lex_match(fs, '('))
    {
        uint8_t nargs = parse_args(fs);
        if(isnew)
        {
            bcemit_arg(fs, BC_NEW, nargs);
        }
        else
        {
            bcemit_arg(fs, BC_INVOKE, name);
            bcemit_byte(fs, nargs);
        }
        return;
    }

    BCOp bc;
    if(assign && lex_match(fs, '='))
    {
        expr(fs);
        bcemit_arg(fs, BC_SETATTR, name);
    }
    else if(assign && (bc = tok2bcassign(fs)))
    {
        tea_lex_next(fs->ls);
        bcemit_arg(fs, BC_PUSHATTR, name);
        expr(fs);
        bcemit_op(fs, bc);
        bcemit_arg(fs, BC_SETATTR, name);
    }
    else
    {
        bcemit_arg(fs, BC_GETATTR, name);
    }
}

/* Return true */
static void expr_true(FuncState* fs, bool assign)
{
    bcemit_op(fs, BC_KTRUE);
}

/* Return false */
static void expr_false(FuncState* fs, bool assign)
{
    bcemit_op(fs, BC_KFALSE);
}

/* Return nil */
static void expr_nil(FuncState* fs, bool assign)
{
    bcemit_op(fs, BC_KNIL);
}

/* Parse list expression */
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

            if(lex_match(fs, TK_dotdotdot))
            {
                expr(fs);
                bcemit_op(fs, BC_LISTEXTEND);
                continue;
            }

            expr(fs);
            bcemit_op(fs, BC_LISTITEM);
        }
        while(lex_match(fs, ','));
    }
    lex_consume(fs, ']');
}

/* Parse map expression */
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
                lex_consume(fs, TK_name);
                bcemit_str(fs, &fs->ls->prev.tv);
                lex_consume(fs, '=');
                expr(fs);
            }
            bcemit_op(fs, BC_MAPFIELD);
        }
        while(lex_match(fs, ','));
    }
    lex_consume(fs, '}');
}

/* Parse a slice into a range, use infinity to signal no limit */
static bool parse_slice(FuncState* fs)
{
    expr(fs);
    /* It's a slice */
    if(lex_match(fs, ':'))
    {
        /* [n:] */
        if(lex_check(fs, ']'))
        {
            bcemit_num(fs, INFINITY);
            bcemit_num(fs, 1);
        }
        else
        {
            /* [n::n] */
            if(lex_match(fs, ':'))
            {
                bcemit_num(fs, INFINITY);
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
                    bcemit_num(fs, 1);
                }
            }
        }
        return true;
    }
    return false;
}

/* Parse index expression with brackets */
static void expr_subscript(FuncState* fs, bool assign)
{
    if(parse_slice(fs))
    {
        bcemit_op(fs, BC_RANGE);
    }

    lex_consume(fs, ']');

    BCOp bc;
    if(assign && lex_match(fs, '='))
    {
        expr(fs);
        bcemit_op(fs, BC_SETIDX);
    }
    else if(assign && (bc = tok2bcassign(fs)))
    {
        tea_lex_next(fs->ls);
        bcemit_op(fs, BC_PUSHIDX);
        expr(fs);
        bcemit_op(fs, bc);
        bcemit_op(fs, BC_SETIDX);
    }
    else
    {
        bcemit_op(fs, BC_GETIDX);
    }
}

/* Parse logical or expression */
static void expr_or(FuncState* fs, bool assign)
{
    BCPos else_jmp = bcemit_jump(fs, BC_JMPFALSE);
    BCPos jmp = bcemit_jump(fs, BC_JMP);
    bcpatch_jump(fs, else_jmp);
    bcemit_op(fs, BC_POP);
    expr_prec(fs, PREC_OR);
    bcpatch_jump(fs, jmp);
}

/* Parse number expression */
static void expr_num(FuncState* fs, bool assign)
{
    bcemit_arg(fs, BC_CONSTANT, const_num(fs, &fs->ls->prev.tv));
}

/* Parse string expression, apply concatenation optimization */
static void expr_str(FuncState* fs, bool assign)
{
    tea_State* T = fs->T;
    TValue tv;
    copyTV(T, &tv, &fs->ls->prev.tv);
    if((fs->ls->curr.t == '+') &&
        (fs->ls->next.t == TK_string))
    {
        SBuf* sb = tea_buf_tmp_(T);
        tea_buf_putstr(T, sb, strV(&tv));
        while((fs->ls->curr.t == '+') && (fs->ls->next.t == TK_string)) 
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

/* Parse interpolated string expression */
static void expr_interpolation(FuncState* fs, bool assign)
{
    bcemit_op(fs, BC_LIST);
    do
    {
        bcemit_str(fs, &fs->ls->prev.tv);
        bcemit_op(fs, BC_LISTITEM);
        expr(fs);
        bcemit_op(fs, BC_LISTITEM);
    }
    while(lex_match(fs, TK_interpolation));
    lex_consume(fs, TK_string);
    bcemit_str(fs, &fs->ls->prev.tv);
    bcemit_op(fs, BC_LISTITEM);
    bcemit_invoke(fs, 0, "join");
}

static void check_const(FuncState* fs, uint8_t set_op, int arg)
{
    switch(set_op)
    {
        case BC_SETLOCAL:
            if(fs->locals[arg].isconst)
            {
                error(fs, TEA_ERR_XVCONST);
            }
            break;
        case BC_SETUPVAL:
            if(fs->upvalues[arg].isconst)
            {
                error(fs, TEA_ERR_XVCONST);
            }
            break;
        case BC_SETMODULE:
            if(fs->ls->vstack[arg].isconst)
            {
                error(fs, TEA_ERR_XVCONST);
            }
            break;
        default:
            break;
    }
}

/* Parse named expression */
static void parse_named(FuncState* fs, Token name, bool assign)
{
    uint8_t getbc, setbc;
    int arg = var_find(fs, &name, lex_check(fs, '='), &getbc, &setbc);

    BCOp bc;
    if(assign && lex_match(fs, '='))
    {
        check_const(fs, setbc, arg);
        expr(fs);
        bcemit_arg(fs, setbc, (uint8_t)arg);
    }
    else if(assign && (bc = tok2bcassign(fs)))
    {
        tea_lex_next(fs->ls);
        check_const(fs, setbc, arg);
        bcemit_arg(fs, getbc, (uint8_t)arg);
        expr(fs);
        bcemit_op(fs, bc);
        bcemit_arg(fs, setbc, (uint8_t)arg);
    }
    else
    {
        bcemit_arg(fs, getbc, (uint8_t)arg);
    }
}

/* Parse name expression */
static void expr_name(FuncState* fs, bool assign)
{
    Token name = fs->ls->prev;
    parse_named(fs, name, assign);
}

/* Parse super */
static void expr_super(FuncState* fs, bool assign)
{
    if(fs->klass == NULL)
    {
        error(fs, TEA_ERR_XSUPERO);
    }
    else if(!fs->klass->isinherit)
    {
        error(fs, TEA_ERR_XSUPERK);
    }

    /* super */
    if(!lex_check(fs, '(') && !lex_check(fs, '.'))
    {
        parse_named(fs, lex_synthetic(fs, "super"), false);
        return;
    }

    /* super() -> super.new() */
    if(lex_match(fs, '('))
    {
        Token tok = lex_synthetic(fs, "new");
        uint8_t name = const_str(fs, strV(&tok.tv));
        parse_named(fs, lex_synthetic(fs, "self"), false);
        uint8_t nargs = parse_args(fs);
        parse_named(fs, lex_synthetic(fs, "super"), false);
        bcemit_arg(fs, BC_SUPER, name);
        bcemit_byte(fs, nargs);
        return;
    }

    /* super.name */
    lex_consume(fs, '.');
    lex_consume(fs, TK_name);
    uint8_t name = const_str(fs, strV(&fs->ls->prev.tv));

    parse_named(fs, lex_synthetic(fs, "self"), false);

    if(lex_match(fs, '('))
    {
        /* super.name() */
        uint8_t nargs = parse_args(fs);
        parse_named(fs, lex_synthetic(fs, "super"), false);
        bcemit_arg(fs, BC_SUPER, name);
        bcemit_byte(fs, nargs);
    }
    else
    {
        /* super.name */
        parse_named(fs, lex_synthetic(fs, "super"), false);
        bcemit_arg(fs, BC_GETSUPER, name);
    }
}

/* Parse self */
static void expr_self(FuncState* fs, bool assign)
{
    if(fs->klass == NULL)
    {
        error(fs, TEA_ERR_XSELFO);
    }
    else if(fs->klass->isstatic)
    {
        error(fs, TEA_ERR_XSELFS);
    }
    expr_name(fs, false);
}

/* Manage syntactic levels to avoid blowing up the stack */
static void synlevel_begin(FuncState* fs)
{
    if(++fs->ls->level >= TEA_MAX_XLEVEL)
        tea_lex_error(fs->ls, 0, fs->ls->prev.line, TEA_ERR_XLEVELS);
}

#define synlevel_end(fs) (((fs)->ls)->level--)

/* Parse unary expression */
static void expr_unary(FuncState* fs, bool assign)
{
    LexToken tok = fs->ls->prev.t;
    expr_prec(fs, PREC_UNARY);
    switch(tok)
    {
        case TK_not:
        case '!':
            bcemit_op(fs, BC_NOT);
            break;
        case '-':
            bcemit_op(fs, BC_NEG);
            break;
        case '~':
            bcemit_op(fs, BC_BNOT);
            break;
        default:
            tea_assertFS(0, "unknown unary op");
            return; /* Unreachable */
    }
}

/* Parse range expression with optional step */
static void expr_range(FuncState* fs, bool assign)
{
    LexToken tok = fs->ls->prev.t;
    ParseRule rule = expr_rule(tok);
    expr_prec(fs, (Prec)(rule.prec + 1));
    if(lex_match(fs, TK_dotdot))
    {
        expr(fs);
    }
    else
    {
        bcemit_num(fs, 1);
    }
    bcemit_op(fs, BC_RANGE);
}

/* Forward declarations */
static void expr_anonymous(FuncState* fs, bool assign);
static void expr_group(FuncState* fs, bool assign);

#define NONE                    (ParseRule){ NULL, NULL, PREC_NONE }
#define RULE(pr, in, prec)      (ParseRule){ pr, in, prec }
#define INFIX(in)               (ParseRule){ NULL, in, PREC_NONE }
#define PREFIX(pr)              (ParseRule){ pr, NULL, PREC_NONE }
#define OPERATOR(in, prec)      (ParseRule){ NULL, in, prec }

/* Find the rule for a given token */
static ParseRule expr_rule(LexToken t)
{
    switch(t)
    {
        case '(':
            return RULE(expr_group, expr_call, PREC_CALL);
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
        case TK_noteq:
        case TK_eq:
            return OPERATOR(expr_binary, PREC_EQUALITY);
        case '<':
        case '>':
        case TK_ge:
        case TK_le:
        case TK_in:
            return OPERATOR(expr_binary, PREC_COMPARISON);
        case '%':
            return OPERATOR(expr_binary, PREC_FACTOR);
        case TK_pow:
            return OPERATOR(expr_binary, PREC_INDICES);
        case TK_not:
            return RULE(expr_unary, expr_binary, PREC_IS);
        case TK_dotdot:
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
        case TK_lshift:
        case TK_rshift:
            return OPERATOR(expr_binary, PREC_SHIFT);
        case TK_name:
            return PREFIX(expr_name);
        case TK_number:
            return PREFIX(expr_num);
        case TK_string:
            return PREFIX(expr_str);
        case TK_interpolation:
            return PREFIX(expr_interpolation);
        case TK_and:
            return OPERATOR(expr_and, PREC_AND);
        case TK_function:
            return PREFIX(expr_anonymous);
        case TK_nil:
            return PREFIX(expr_nil);
        case TK_or:
            return OPERATOR(expr_or, PREC_OR);
        case TK_is:
            return OPERATOR(expr_binary, PREC_IS);
        case TK_super:
            return PREFIX(expr_super);
        case TK_self:
            return PREFIX(expr_self);
        case TK_true:
            return PREFIX(expr_true);
        case TK_false:
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

/* Parse expression with precedence */
static void expr_prec(FuncState* fs, Prec prec)
{
    synlevel_begin(fs);
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
    synlevel_end(fs);

    if(assign && lex_match(fs, '='))
    {
        error(fs, TEA_ERR_XASSIGN);
    }
}

/* Parse expression */
static void expr(FuncState* fs)
{
    expr_prec(fs, PREC_ASSIGNMENT);
}

/* Forward declaration */
static void parse_decl(FuncState* fs, bool export);

/* Parse a block */
static void parse_block(FuncState* fs)
{
    lex_consume(fs, '{');
    while(!lex_check(fs, '}') && !lex_check(fs, TK_eof))
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
        bool isspread = false;
        do
        {
            if(isspread)
            {
                error(fs, TEA_ERR_XSPREADARGS);
            }

            isspread = lex_match(fs, TK_dotdotdot);
            lex_consume(fs, TK_name);

            Token name = fs->ls->prev;
            if(var_lookup_local(fs, &name) != -1)
                error(fs, TEA_ERR_XDUPARGS);

            if(isspread)
            {
                fs->flags |= PROTO_VARARG;
            }

            if(lex_match(fs, '='))
            {
                if(isspread)
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
            bcemit_op(fs, BC_DEFOPT);
            bcemit_bytes(fs, fs->numparams, fs->numopts);
        }
    }
    lex_consume(fs, ')');
}

/* Parse arrow function */
static void parse_arrow(FuncState* fs)
{
    parse_params(fs);
    lex_consume(fs, TK_arrow);
    if(lex_check(fs, '{'))
    {
        /* Expect a block */
        parse_block(fs);
        bcemit_return(fs);
    }
    else
    {
        /* Expect single expression */
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
    if(info == FUNC_GETTER)
    {
        parse_block(&fs);
    }
    else if(info == FUNC_ARROW)
    {
        parse_arrow(&fs);
    }
    else
    {
        lex_consume(&fs, '(');
        parse_params(&fs);
        parse_block(&fs);
        bcemit_return(&fs);
    }
    pt = fs_finish(ls, line);
    pfs->bcbase = ls->bcstack + oldbase;    /* May have been reallocated */
    pfs->bclimit = (BCPos)(ls->sizebcstack - oldbase);
    /* Store new prototype in the constant array of the parent */
    bcemit_arg(pfs, BC_CLOSURE, const_gc(pfs, (GCobj*)pt, TEA_TPROTO));
    if(!(pfs->flags & PROTO_CHILD))
    {
        pfs->flags |= PROTO_CHILD;
    }
}

/* Parse anonymous function */
static void expr_anonymous(FuncState* fs, bool assign)
{
    parse_body(fs->ls, FUNC_ANONYMOUS, fs->ls->prev.line);
}

/* Parse grouping expression */
static void expr_group(FuncState* fs, bool assign)
{
    LexToken curr = fs->ls->curr.t;
    LexToken next = fs->ls->next.t;
    /* () => ...; (...v) => ... */
    /* (a) => ...; (a, ) => ... */
    if((curr == ')' && curr == TK_dotdotdot) || 
        (curr == TK_name && (next == ',' || next == ')')) || 
        (curr == ')' && next == TK_arrow))
    {
        parse_body(fs->ls, FUNC_ARROW, fs->ls->prev.line);
        return;
    }
    expr(fs);
    lex_consume(fs, ')');
}

static const LexToken ops[] = {
    '+', '-', '*', '/', '%',
    TK_pow,        /* ** */
    '&', '|', '~', '^',
    TK_lshift,        /* << */
    TK_rshift,  /* >> */
    '<',
    TK_le,       /* <= */
    '>',
    TK_ge,    /* >= */
    TK_eq,      /* == */
    '[',
    TK_eof
};

#define SENTINEL 18

/* Parse operator overload method */
static void parse_operator(FuncState* fs)
{
    int i = 0;
    while(ops[i] != TK_eof)
    {
        if(lex_match(fs, ops[i]))
            break;
        i++;
    }
    if(i == SENTINEL) error(fs, TEA_ERR_XMETHOD);

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
        fs->klass->isstatic = true;
    }

    uint8_t k = const_str(fs, name);
    fs->name = name;
    parse_body(fs->ls, FUNC_OPERATOR, fs->ls->prev.line);
    bcemit_arg(fs, BC_METHOD, k);
    bcemit_byte(fs, 0);
}

/* Parse method */
static void parse_method(FuncState* fs, FuncInfo info, uint8_t flags)
{
    uint8_t k = const_str(fs, strV(&fs->ls->prev.tv));
    parse_body(fs->ls, info, fs->ls->prev.line);
    bcemit_arg(fs, BC_METHOD, k);
    bcemit_byte(fs, flags);
}

/* Parse get/set accessor methods */
static void parse_accessor(FuncState* fs)
{
    lex_consume(fs, TK_name);
    uint8_t k = const_str(fs, strV(&fs->ls->prev.tv));
    uint8_t flags = 0;
    FuncInfo info = 0;
    if(lex_match(fs, '='))
    {
        info = FUNC_SETTER;
        flags = ACC_SET;
    }
    else
    {
        info = FUNC_GETTER;
        flags = ACC_GET;
    }
    parse_body(fs->ls, info, fs->ls->prev.line);
    bcemit_arg(fs, BC_METHOD, k);
    bcemit_byte(fs, flags);
}

/* Initialize a new KlassState */
static void ks_init(FuncState* fs, KlassState* ks)
{
    ks->isinherit = false;
    ks->isstatic = false;
    ks->prev = fs->klass;
    fs->klass = ks;
}

/* Parse class methods */
static void parse_class_body(FuncState* fs)
{
    while(!lex_check(fs, '}') && !lex_check(fs, TK_eof))
    {
        fs->klass->isstatic = false;
        LexToken t = fs->ls->curr.t;
        switch(t)
        {
            case TK_new:
                tea_lex_next(fs->ls);
                parse_method(fs, FUNC_INIT, 0);
                break;
            case TK_function:
                tea_lex_next(fs->ls);
                lex_consume(fs, TK_name);
                parse_method(fs, FUNC_METHOD, 0);
                break;
            case TK_static:
                tea_lex_next(fs->ls);
                lex_consume(fs, TK_name);
                fs->klass->isstatic = true;
                parse_method(fs, FUNC_STATIC, ACC_STATIC);
                break;
            case TK_operator:
                tea_lex_next(fs->ls);
                parse_operator(fs);
                break;
            case TK_name:
                parse_accessor(fs);
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
    lex_consume(fs, TK_name);
    Token class_name = fs->ls->prev;
    uint8_t k = const_str(fs, strV(&class_name.tv));
    var_declare(fs, &class_name);

    bcemit_arg(fs, BC_CLASS, k);
    var_define(fs, &class_name, false, export);

    KlassState ks;
    ks_init(fs, &ks);

    if(lex_match(fs, ':'))
    {
        expr(fs);

        scope_begin(fs);
        var_add_local(fs, lex_synthetic(fs, "super"));
        var_define(fs, 0, false, false);

        parse_named(fs, class_name, false);
        bcemit_op(fs, BC_INHERIT);
        ks.isinherit = true;
    }

    parse_named(fs, class_name, false);

    lex_consume(fs, '{');
    parse_class_body(fs);
    lex_consume(fs, '}');

    bcemit_op(fs, BC_POP);

    if(ks.isinherit)
    {
        scope_end(fs);
    }

    fs->klass = fs->klass->prev;
}

/* Parse function assignment obj.name or obj:name */
static void parse_function_assign(FuncState* fs, BCLine line)
{
    if(lex_match(fs, '.'))
    {
        lex_consume(fs, TK_name);
        uint8_t k = const_str(fs, strV(&fs->ls->prev.tv));
        if(!lex_check(fs, '('))
        {
            bcemit_arg(fs, BC_GETATTR, k);
            parse_function_assign(fs, line);
        }
        else
        {
            parse_body(fs->ls, FUNC_NORMAL, line);
            bcemit_arg(fs, BC_SETATTR, k);
            bcemit_op(fs, BC_POP);
            return;
        }
    }
    else if(lex_match(fs, ':'))
    {
        lex_consume(fs, TK_name);
        uint8_t k = const_str(fs, strV(&fs->ls->prev.tv));
        KlassState ks;
        ks_init(fs, &ks);
        parse_body(fs->ls, FUNC_METHOD, line);
        fs->klass = fs->klass->prev;
        bcemit_op(fs, BC_ISTYPE);
        bcemit_arg(fs, BC_METHOD, k);
        bcemit_byte(fs, 0);
        bcemit_op(fs, BC_POP);
        return;
    }
}

/* Parse 'function' declaration */
static void parse_function(FuncState* fs, bool export)
{
    tea_lex_next(fs->ls);  /* Skip 'function' */
    BCLine line = fs->ls->prev.line;
    lex_consume(fs, TK_name);
    Token name = fs->ls->prev;
    if((lex_check(fs, '.') || lex_check(fs, ':')) && !export)
    {
        parse_named(fs, name, false);
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
    int varnum = 0;
    int exprnum = 0;
    bool isrest = false;
    int resnum = 0;
    int restpos = 0;

    do
    {
        if(resnum > 1)
        {
            error(fs, TEA_ERR_XDOTS);
        }

        if(lex_match(fs, TK_dotdotdot))
        {
            isrest = true;
            resnum++;
        }

        lex_consume(fs, TK_name);
        vars[varnum] = fs->ls->prev;
        varnum++;

        if(isrest)
        {
            restpos = varnum;
            isrest = false;
        }

        if(varnum == 1 && lex_match(fs, '='))
        {
            if(resnum)
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
                    lex_consume(fs, TK_name);
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

    if(resnum)
    {
        lex_consume(fs, '=');
        expr(fs);
        bcemit_op(fs, BC_UNPACKREST);
        bcemit_bytes(fs, varnum, restpos - 1);
        goto finish;
    }

    if(lex_match(fs, '='))
    {
        do
        {
            expr(fs);
            exprnum++;
            if(exprnum == 1 && (!lex_check(fs, ',')))
            {
                bcemit_arg(fs, BC_UNPACK, varnum);
                goto finish;
            }
        }
        while(lex_match(fs, ','));

        if(exprnum != varnum)
        {
            error(fs, TEA_ERR_XVALASSIGN);
        }
    }
    else
    {
        for(int i = 0; i < varnum; i++)
        {
            bcemit_op(fs, BC_KNIL);
        }
    }

finish:
    if(fs->scope_depth == 0)
    {
        for(int i = varnum - 1; i >= 0; i--)
        {
            var_declare(fs, &vars[i]);
            var_define(fs, &vars[i], isconst, export);
        }
    }
    else
    {
        for(int i = 0; i < varnum; i++)
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
    int varnum = 1;
    vars[0] = var;

    if(lex_match(fs, ','))
    {
        do
        {
            lex_consume(fs, TK_name);
            vars[varnum] = fs->ls->prev;
            varnum++;
        }
        while(lex_match(fs, ','));
    }

    lex_consume(fs, TK_in);

    expr(fs);
    int seq_slot = var_add_local(fs, lex_synthetic(fs, "seq "));
    var_mark(fs, false);

    /* Get the iterator */
    bcemit_arg(fs, BC_GETITER, seq_slot);
    int iter_slot = var_add_local(fs, lex_synthetic(fs, "iter "));
    var_mark(fs, false);

    Loop loop;
    loop_begin(fs, &loop);

    loop.end = -1;

    /* Call the iterator, if nil it means the loop is over */
    bcemit_arg(fs, BC_FORITER, iter_slot);
    BCPos end_jpm = bcemit_jump(fs, BC_JMPNIL);

    scope_begin(fs);

    if(varnum > 1)
        bcemit_arg(fs, BC_UNPACK, varnum);

    for(int i = 0; i < varnum; i++)
    {
        var_declare(fs, &vars[i]);
        var_define(fs, &vars[i], isconst, false);
    }

    fs->loop->body = fs->pc;
    parse_code(fs);

    /* Loop variable */
    scope_end(fs);

    bcemit_loop(fs, fs->loop->start);
    bcpatch_jump(fs, end_jpm);
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
    if(lex_match(fs, TK_var) || (isconst = lex_match(fs, TK_const)))
    {
        lex_consume(fs, TK_name);
        Token var = fs->ls->prev;

        if(lex_check(fs, TK_in) || lex_check(fs, ','))
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
            bcemit_op(fs, BC_KNIL);
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

    fs->loop->end = bcemit_jump(fs, BC_JMPFALSE);
    bcemit_op(fs, BC_POP); /* Condition */

    BCPos body_jmp = bcemit_jump(fs, BC_JMP);

    BCPos inc_start = fs->pc;
    expr(fs);
    bcemit_op(fs, BC_POP);

    bcemit_loop(fs, fs->loop->start);
    fs->loop->start = inc_start;

    bcpatch_jump(fs, body_jmp);

    fs->loop->body = fs->pc;

    int inner_var = -1;
    if(loop_var != -1)
    {
        scope_begin(fs);
        bcemit_arg(fs, BC_GETLOCAL, (uint8_t)loop_var);
        var_add_local(fs, var_name);
        var_mark(fs, false);
        inner_var = fs->local_count - 1;
    }

    parse_code(fs);

    if(inner_var != -1)
    {
        bcemit_arg(fs, BC_GETLOCAL, (uint8_t)inner_var);
        bcemit_arg(fs, BC_SETLOCAL, (uint8_t)loop_var);
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
    scope_close(fs, fs->loop->scope_depth + 1);
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
    scope_close(fs, fs->loop->scope_depth + 1);
    /* Jump to the top of the innermost loop */
    bcemit_loop(fs, fs->loop->start);
}

/* Parse 'if' statement */
static void parse_if(FuncState* fs)
{
    tea_lex_next(fs->ls);  /* Skip 'if' */
    expr(fs);

    BCPos else_jmp = bcemit_jump(fs, BC_JMPFALSE);
    bcemit_op(fs, BC_POP);
    
    parse_code(fs);

    BCPos end_jmp = bcemit_jump(fs, BC_JMP);

    bcpatch_jump(fs, else_jmp);
    bcemit_op(fs, BC_POP);

    if(lex_match(fs, TK_else))
    {
        if(lex_check(fs, TK_if))
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
    int casenum = 0;

    tea_lex_next(fs->ls);  /* Skip 'switch' */
    expr(fs);
    lex_consume(fs, '{');

    if(lex_match(fs, TK_case))
    {
        do
        {
            expr(fs);
            int multi = 0;  /* Keep track of multi-cases */
            if(lex_match(fs, ','))
            {
                do
                {
                    multi++;
                    expr(fs);
                }
                while(lex_match(fs, ','));
                bcemit_arg(fs, BC_MULTICASE, multi);
            }
            BCPos jmp = bcemit_jump(fs, BC_JMPCMP);
            parse_code(fs);
            case_ends[casenum++] = bcemit_jump(fs, BC_JMP);
            bcpatch_jump(fs, jmp);
            if(casenum > 255)
            {
                error(fs, TEA_ERR_XSWITCH);
            }
        }
        while(lex_match(fs, TK_case));
    }

    bcemit_op(fs, BC_POP); /* Expression */
    if(lex_match(fs, TK_default))
    {
        parse_code(fs);
    }

    if(lex_match(fs, TK_case))
    {
        error(fs, TEA_ERR_XCASE);
    }

    lex_consume(fs, '}');

    for(int i = 0; i < casenum; i++)
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
    lex_consume(fs, TK_name);
    Token name = fs->ls->prev;
    uint8_t k = const_str(fs, strV(&name.tv));
    bcemit_arg(fs, BC_IMPORTNAME, k);
    bcemit_op(fs, BC_POP);

    if(lex_match(fs, TK_as))
    {
        lex_consume(fs, TK_name);
        name = fs->ls->prev;
    }
    
    var_declare(fs, &name);
    bcemit_op(fs, BC_IMPORTALIAS);
    var_define(fs, &name, false, false);
    bcemit_op(fs, BC_IMPORTEND);

    if(lex_match(fs, ','))
    {
        parse_import_name(fs);
    }
}

/* Parse 'import <string>' statement */
static void parse_import_string(FuncState* fs)
{
    if(lex_match(fs, TK_string))
    {
        uint8_t k = const_str(fs, strV(&fs->ls->prev.tv));
        bcemit_arg(fs, BC_IMPORTSTR, k);
    }
    else
    {
        lex_consume(fs, TK_interpolation);
        expr_interpolation(fs, false);
        bcemit_op(fs, BC_IMPORTFMT);
    }
    bcemit_op(fs, BC_POP);

    if(lex_match(fs, TK_as))
    {
        lex_consume(fs, TK_name);
        Token name = fs->ls->prev;
        var_declare(fs, &name);
        bcemit_op(fs, BC_IMPORTALIAS);
        var_define(fs, &name, false, false);
    }

    bcemit_op(fs, BC_IMPORTEND);

    if(lex_match(fs, ','))
    {
        parse_import_string(fs);
    }
}

/* Parse 'from' statement */
static void parse_from(FuncState* fs)
{
    if(lex_match(fs, TK_string))
    {
        uint8_t k = const_str(fs, strV(&fs->ls->prev.tv));
        bcemit_arg(fs, BC_IMPORTSTR, k);
    }
    else if(lex_match(fs, TK_interpolation))
    {
        expr_interpolation(fs, false);
        bcemit_op(fs, BC_IMPORTFMT);
    }
    else
    {
        lex_consume(fs, TK_name);
        uint8_t k = const_str(fs, strV(&fs->ls->prev.tv));
        bcemit_arg(fs, BC_IMPORTNAME, k);
    }
    lex_consume(fs, TK_import);
    bcemit_op(fs, BC_POP);

    do
    {
        lex_consume(fs, TK_name);
        Token name = fs->ls->prev;
        uint8_t k = const_str(fs, strV(&name.tv));
        if(lex_match(fs, TK_as))
        {
            lex_consume(fs, TK_name);
            name = fs->ls->prev;
        }
        var_declare(fs, &name);
        bcemit_arg(fs, BC_IMPORTVAR, k);
        var_define(fs, &name, false, false);
    }
    while(lex_match(fs, ','));
    bcemit_op(fs, BC_IMPORTEND);
}

/* Parse 'while' statement */
static void parse_while(FuncState* fs)
{
    Loop loop;
    loop_begin(fs, &loop);

    tea_lex_next(fs->ls);  /* Skip 'while' */
    if(lex_check(fs, '{'))
    {
        bcemit_byte(fs, BC_KTRUE);
    }
    else
    {
        expr(fs);
    }

    /* Jump ot of the loop if the condition is false */
    fs->loop->end = bcemit_jump(fs, BC_JMPFALSE);
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

    lex_consume(fs, TK_while);
    expr(fs);

    fs->loop->end = bcemit_jump(fs, BC_JMPFALSE);
    bcemit_op(fs, BC_POP);

    bcemit_loop(fs, fs->loop->start);
    loop_end(fs);
}

/* Parse multiple assignment of named variables */
static void parse_multiple_assign(FuncState* fs)
{
    int exprnum = 0;
    int varnum = 0;
    Token vars[255];

    do
    {
        lex_consume(fs, TK_name);
        vars[varnum] = fs->ls->prev;
        varnum++;
    }
    while(lex_match(fs, ','));

    lex_consume(fs, '=');

    do
    {
        expr(fs);
        exprnum++;
        if(exprnum == 1 && (!lex_check(fs, ',')))
        {
            bcemit_arg(fs, BC_UNPACK, varnum);
            goto finish;
        }
    }
    while(lex_match(fs, ','));

    if(exprnum != varnum)
    {
        error(fs, TEA_ERR_XVASSIGN);
    }

finish:
    for(int i = varnum - 1; i >= 0; i--)
    {
        Token tok = vars[i];
        uint8_t getbc, setbc;
        int arg = var_find(fs, &tok, true, &getbc, &setbc);
        check_const(fs, setbc, arg);
        bcemit_arg(fs, setbc, (uint8_t)arg);
        bcemit_op(fs, BC_POP);
    }
}

/* Parse 'export' statement */
static void parse_export(FuncState* fs)
{
    tea_lex_next(fs->ls);
    if(fs->info != FUNC_SCRIPT)
    {
        error(fs, TEA_ERR_XRET);
    }
    LexToken t = fs->ls->curr.t;
    if(t == TK_class || t == TK_function || t == TK_const || t == TK_var)
    {
        parse_decl(fs, true);
        return;
    }
    lex_consume(fs, '{');
    while(!lex_check(fs, '}') && !lex_check(fs, TK_eof))
    {
        do
        {
            Token name;
            lex_consume(fs, TK_name);
            name = fs->ls->prev;
            int idx = var_lookup_var(fs, &name);
            if(idx == -1)
            {
                tea_lex_error(fs->ls, fs->ls->prev.t, fs->ls->prev.line, TEA_ERR_XVAR, str_data(strV(&name.tv)));
            }
            bcemit_arg(fs, BC_GETMODULE, idx);
            uint8_t k = const_str(fs, strV(&name.tv));
            bcemit_arg(fs, BC_DEFMODULE, k);
            bcemit_byte(fs, BC_POP);
        }
        while(lex_match(fs, ','));
    }
    lex_consume(fs, '}');
}

/* -- Parse statements and declarations ------------------------------------ */

/* Parse a statement */
static void parse_stmt(FuncState* fs)
{
    switch(fs->ls->curr.t)
    {
        case ';':
            tea_lex_next(fs->ls);
            break;
        case TK_for:
            parse_for(fs);
            break;
        case TK_if:
            parse_if(fs);
            break;
        case TK_switch:
            parse_switch(fs);
            break;
        case TK_export:
            parse_export(fs);
            break;
        case TK_return:
            parse_return(fs);
            break;
        case TK_while:
            parse_while(fs);
            break;
        case TK_do:
            parse_do(fs);
            break;
        case TK_import:
            tea_lex_next(fs->ls);  /* Skip 'import' */
            if(lex_check(fs, TK_string) ||
                lex_check(fs, TK_interpolation))
                parse_import_string(fs);
            else
                parse_import_name(fs);
            break;
        case TK_from:
            tea_lex_next(fs->ls);  /* Skip 'from' */
            parse_from(fs);
            break;
        case TK_break:
            parse_break(fs);
            break;
        case TK_continue:
            parse_continue(fs);
            break;
        case TK_name:
            if(fs->ls->next.t == ',')
                parse_multiple_assign(fs);
            else
                parse_expr_stmt(fs);
            break;
        case '{':
            parse_code(fs);
            break;
        default:
            parse_expr_stmt(fs);
            break;
    }
}

/* Parse a declaration */
static void parse_decl(FuncState* fs, bool export)
{
    synlevel_begin(fs);
    LexToken t = fs->ls->curr.t;
    switch(t)
    {
        case TK_class:
            parse_class(fs, export);
            break;
        case TK_function:
            parse_function(fs, export);
            break;
        case TK_const:
        case TK_var:
            tea_lex_next(fs->ls);  /* Skip 'const' or 'var' */
            parse_var(fs, t == TK_const, export);
            break;
        default:
            parse_stmt(fs);
            break;
    }
    synlevel_end(fs);
}

/* Parse a single expression and return its result */
static void parse_eval(FuncState* fs)
{
    expr(fs);
    bcemit_op(fs, BC_RETURN);
    lex_consume(fs, TK_eof);
}

/* A chunk is a sequence of statements */
static void parse_chunk(FuncState* fs)
{
    while(!lex_match(fs, TK_eof))
    {
        parse_decl(fs, false);
    }
    bcemit_return(fs);
}

/* Entry point of bytecode parser */
GCproto* tea_parse(LexState* ls, bool eval)
{
    FuncState fs;
    GCproto* pt;
    tea_State* T = ls->T;
    ls->level = 0;
    fs_init(ls, &fs, FUNC_SCRIPT);
    fs.linedefined = 0;
    fs.bcbase = NULL;
    fs.bclimit = 0;
    fs.varnum = 0;
    tea_lex_next(fs.ls);   /* Read the first token into "next" */
    tea_lex_next(fs.ls);   /* Copy "next" -> "curr" */
    var_fixup_start(&fs);
    if(eval)
        parse_eval(&fs);
    else
        parse_chunk(&fs);
    pt = fs_finish(ls, ls->linenumber);
    var_fixup_end(&fs);
    tea_assertT(fs.prev == NULL && ls->fs == NULL, "mismatched frame nesting");
    tea_assertT(pt->sizeuv == 0, "top level proto has upvalues");
    return pt;
}