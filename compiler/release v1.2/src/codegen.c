/* Nevo v1.2 AST code generator.
 *
 * The default backend emits ARM64 Apple Silicon assembly. Defining
 * NEVO_TARGET_WINDOWS_X64 selects the Windows x86-64 NASM backend while
 * retaining the exact same AST and language frontend.
 *
 * Language features:
 *   num VAR = EXPR          global variable declaration / assignment
 *   scoped num VAR = EXPR   function-local (stack) variable
 *   print(EXPR)             print integer or string literal
 *   jump FUNC()             tail call (no return)
 *   loop COUNT { ... }      repeat body COUNT times
 *   if EXPR { ... }         conditional block
 *   if EXPR { ... } else { ... }  conditional with else branch
 *   txt VAR = "str"         string variable (stores pointer)
 *   file VAR = loadf(PATH)   read-only file value (stores text pointer)
 *   VAR.filter(TEXT)         lines containing TEXT
 *   VAR.line(N)              one-based line lookup
 *   VAR.count(...)           line/filter/word count
 *   num VAR += EXPR         augmented add-assign
 *   num VAR -= EXPR         augmented sub-assign
 *   if (VAR == "str") { }   string comparison via strcmp
 *   random(MIN, MAX)        inclusive random integer [MIN, MAX]
 *   EXPR: NUMBER | IDENTIFIER | EXPR OP EXPR   (OP: + - * / == != < > <= >=)
 *
 * Frame layout per function
 *   [x29 + 0]               saved x29 (frame pointer)
 *   [x29 + 8]               saved x30 (link register)
 *   [x29 + 16 .. +8*ns)     scoped variables  (8 bytes each)
 *   [x29 + 16+8*ns ..]      loop counters     (8 bytes each)
 *   frame_size = align16(16 + 8*(ns + nl))
 *
 * macOS assemble/link: clang -arch arm64 output.asm -o program
 * Windows assemble:    nasm -f win64 output.asm -o output.obj
 * Windows link:        gcc output.obj file_runtime.c -o program.exe
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "compiler_api.h"
#include "source_io.h"
/* ════════════════════════════════════════════════════════════
   §1  MINIMAL  JSON  PARSER
   ════════════════════════════════════════════════════════════ */
typedef enum
{
    JT_NULL,
    JT_BOOL,
    JT_NUM,
    JT_STR,
    JT_ARR,
    JT_OBJ
} JType;
typedef struct JVal JVal;
typedef struct JPair JPair;
struct JPair
{
    char *k;
    JVal *v;
    JPair *next;
};
struct JVal
{
    JType t;
    union
    {
        int b;    /* JT_BOOL */
        double n; /* JT_NUM  */
        char *s;  /* JT_STR  */
        struct
        {
            JVal **a;
            int len;
        } arr;      /* JT_ARR  */
        JPair *obj; /* JT_OBJ  */
    };
};
static const char *P; /* parse cursor */
static void skip_ws(void)
{
    while (isspace((unsigned char)*P))
        P++;
}
static JVal *jnew(JType t)
{
    JVal *v = calloc(1, sizeof *v);
    v->t = t;
    return v;
}
static char *parse_str(void)
{
    P++; /* skip " */
    size_t cap = 128, len = 0;
    char *b = malloc(cap);
    while (*P && *P != '"')
    {
        char c = *P++;
        if (c == '\\')
        {
            switch (*P++)
            {
            case 'n':
                c = '\n';
                break;
            case 't':
                c = '\t';
                break;
            case 'r':
                c = '\r';
                break;
            case '\\':
                c = '\\';
                break;
            case '"':
                c = '"';
                break;
            case '/':
                c = '/';
                break;
            default:
                c = P[-1];
            }
        }
        if (len + 2 > cap)
            b = realloc(b, cap *= 2);
        b[len++] = c;
    }
    P++; /* skip " */
    b[len] = '\0';
    return b;
}
static JVal *parse_val(void);
static JVal *parse_arr(void)
{
    JVal *v = jnew(JT_ARR);
    P++; /* skip [ */
    int cap = 8;
    v->arr.a = malloc(cap * sizeof *v->arr.a);
    skip_ws();
    if (*P == ']')
    {
        P++;
        return v;
    }
    for (;;)
    {
        if (v->arr.len >= cap)
            v->arr.a = realloc(v->arr.a, (cap *= 2) * sizeof *v->arr.a);
        v->arr.a[v->arr.len++] = parse_val();
        skip_ws();
        if (*P == ',')
        {
            P++;
            skip_ws();
        }
        else
            break;
    }
    P++; /* skip ] */
    return v;
}
static JVal *parse_obj(void)
{
    JVal *v = jnew(JT_OBJ);
    P++; /* skip { */
    JPair *tail = NULL;
    skip_ws();
    if (*P == '}')
    {
        P++;
        return v;
    }
    for (;;)
    {
        skip_ws();
        JPair *pr = calloc(1, sizeof *pr);
        pr->k = parse_str();
        skip_ws();
        P++;
        skip_ws(); /* skip : */
        pr->v = parse_val();
        if (!v->obj)
            v->obj = pr;
        else
            tail->next = pr;
        tail = pr;
        skip_ws();
        if (*P == ',')
            P++;
        else
            break;
    }
    skip_ws();
    P++; /* skip } */
    return v;
}
static JVal *parse_val(void)
{
    skip_ws();
    switch (*P)
    {
    case '"':
    {
        JVal *v = jnew(JT_STR);
        v->s = parse_str();
        return v;
    }
    case '{':
        return parse_obj();
    case '[':
        return parse_arr();
    case 't':
        P += 4;
        {
            JVal *v = jnew(JT_BOOL);
            v->b = 1;
            return v;
        }
    case 'f':
        P += 5;
        return jnew(JT_BOOL);
    case 'n':
        P += 4;
        return jnew(JT_NULL);
    default:
    {
        JVal *v = jnew(JT_NUM);
        char *e;
        v->n = strtod(P, &e);
        P = e;
        return v;
    }
    }
}
/* ── JSON field helpers ── */
static JVal *jget(JVal *o, const char *k)
{
    if (!o || o->t != JT_OBJ)
        return NULL;
    for (JPair *p = o->obj; p; p = p->next)
        if (!strcmp(p->k, k))
            return p->v;
    return NULL;
}
static const char *jS(JVal *o, const char *k)
{
    JVal *f = jget(o, k);
    return (f && f->t == JT_STR) ? f->s : NULL;
}
static long long jN(JVal *o, const char *k)
{
    JVal *f = jget(o, k);
    return (f && f->t == JT_NUM) ? (long long)f->n : 0;
}
static int jB(JVal *o, const char *k)
{
    JVal *f = jget(o, k);
    return (f && f->t == JT_BOOL) ? f->b : 0;
}
static JVal *jA(JVal *o, const char *k)
{
    JVal *f = jget(o, k);
    return (f && f->t == JT_ARR) ? f : NULL;
}
/* ════════════════════════════════════════════════════════════
   §2  GLOBAL  STATE
   ════════════════════════════════════════════════════════════ */
/* ── String literal pool ─────────────────────────────────── */
#define MAX_STRS 512
static struct
{
    char *s;
    int id;
} gStrs[MAX_STRS];
static int gNStr = 0;
static int strIntern(const char *s)
{
    for (int i = 0; i < gNStr; i++)
        if (!strcmp(gStrs[i].s, s))
            return gStrs[i].id;
    gStrs[gNStr].s = strdup(s);
    gStrs[gNStr].id = gNStr;
    return gNStr++;
}
/* ── Global (unscoped) variable registry ─────────────────── */
#define MAX_GVARS 256
static char *gVars[MAX_GVARS];
static int gNVar = 0;
static int gvHas(const char *n)
{
    for (int i = 0; i < gNVar; i++)
        if (!strcmp(gVars[i], n))
            return 1;
    return 0;
}
static void gvAdd(const char *n)
{
    if (!gvHas(n))
        gVars[gNVar++] = strdup(n);
}
/* ── Txt variable registries ─────────────────────────────── */
/* Track which names were declared via TxtDecl so Print and
   string-comparison code can skip the int64_to_str path.    */
#define MAX_LVARS 128
static char *gTxtVars[MAX_GVARS]; /* global txt var names  */
static int gNTxtVar = 0;
static char *lTxtVars[MAX_LVARS]; /* local  txt var names  */
static int lNTxtVar = 0;
static char *gFileVars[MAX_GVARS]; /* global file var names */
static int gNFileVar = 0;
static char *gFuncNames[256];
static char *gFuncReturnTypes[256];
static int gNFunc = 0;
static int isTxtVar(const char *nm)
{
    if (!nm)
        return 0;
    for (int i = 0; i < gNTxtVar; i++)
        if (!strcmp(gTxtVars[i], nm))
            return 1;
    for (int i = 0; i < lNTxtVar; i++)
        if (!strcmp(lTxtVars[i], nm))
            return 1;
    return 0;
}
static int isFileVar(const char *nm)
{
    if (!nm)
        return 0;
    for (int i = 0; i < gNFileVar; i++)
        if (!strcmp(gFileVars[i], nm))
            return 1;
    return 0;
}
static void fileVarAdd(const char *nm)
{
    if (!isFileVar(nm))
        gFileVars[gNFileVar++] = strdup(nm);
}
static const char *funcReturnType(const char *nm)
{
    for (int i = 0; i < gNFunc; i++)
        if (!strcmp(gFuncNames[i], nm))
            return gFuncReturnTypes[i];
    return NULL;
}
/* ── Scoped (stack) variable table — reset per function ───── */
static struct
{
    char *name;
    int off;
} lVars[MAX_LVARS];
static int lNVar = 0;
static int lNext = 16;
static void lReset(void)
{
    lNVar = 0;
    lNext = 16;
    lNTxtVar = 0;
}
static int lGet(const char *n)
{
    for (int i = 0; i < lNVar; i++)
        if (!strcmp(lVars[i].name, n))
            return lVars[i].off;
    return -1;
}
static int lAlloc(const char *n)
{
    int off = lGet(n);
    if (off >= 0)
        return off;
    lVars[lNVar].name = strdup(n);
    lVars[lNVar].off = lNext;
    lNext += 8;
    return lVars[lNVar++].off;
}
/* ── Code-gen per-function globals ───────────────────────── */
static FILE *OUT;
static int gLoopUID = 0;
static int gIfUID = 0;
static int gFsz = 0;
static int gLctrOff = 0;
/* ════════════════════════════════════════════════════════════
   §3  PRE-SCAN
   ════════════════════════════════════════════════════════════ */
typedef struct
{
    char *sn[MAX_LVARS];
    int ns;
    int nl;
} Scan;
static int scanHasLocal(const Scan *sc, const char *name)
{
    for (int i = 0; i < sc->ns; i++)
        if (!strcmp(sc->sn[i], name))
            return 1;
    return 0;
}
static void scanAddLocal(Scan *sc, const char *name)
{
    if (!scanHasLocal(sc, name))
        sc->sn[sc->ns++] = (char *)name;
}
static void scanParams(JVal *fn, Scan *sc)
{
    JVal *params = jA(fn, "params");
    if (!params)
        return;
    if (params->arr.len > 8)
    {
        fprintf(stderr, "Error: functions support at most 8 parameters\n");
        exit(1);
    }
    for (int i = 0; i < params->arr.len; i++)
        scanAddLocal(sc, params->arr.a[i]->s);
}
static char *proc_esc(const char *s)
{
    size_t cap = 128, len = 0;
    char *b = malloc(cap);
    while (*s)
    {
        char c = *s++;
        if (c == '\\' && *s)
        {
            switch (*s++)
            {
            case 'n':
                c = '\n';
                break;
            case 't':
                c = '\t';
                break;
            case 'r':
                c = '\r';
                break;
            case '\\':
                c = '\\';
                break;
            case '"':
                c = '"';
                break;
            default:
                if (len + 3 > cap)
                    b = realloc(b, cap *= 2);
                b[len++] = '\\';
                c = s[-1];
            }
        }
        if (len + 2 > cap)
            b = realloc(b, cap *= 2);
        b[len++] = c;
    }
    b[len] = '\0';
    return b;
}
/* Recursively intern any String literals found inside an expression.
   Called during the pre-scan pass so all strings appear in the data
   section before any code is emitted.                               */
static void scan_expr(JVal *e)
{
    if (!e)
        return;
    const char *t = jS(e, "type");
    if (!t)
        return;
    if (!strcmp(t, "String"))
    {
        char *p = proc_esc(jS(e, "value"));
        strIntern(p);
        free(p);
        return;
    }
    if (!strcmp(t, "BinaryOp") || !strcmp(t, "Cmp"))
    {
        scan_expr(jget(e, "left"));
        scan_expr(jget(e, "right"));
        return;
    }
    if (!strcmp(t, "UnaryNeg"))
    {
        scan_expr(jget(e, "operand"));
        return;
    }
    if (!strcmp(t, "Random"))
    {
        scan_expr(jget(e, "min"));
        scan_expr(jget(e, "max"));
        return;
    }
    if (!strcmp(t, "LoadFile") || !strcmp(t, "CreateFile") ||
        !strcmp(t, "Word"))
    {
        scan_expr(jget(e, "argument"));
        return;
    }
    if (!strcmp(t, "FileFilter") || !strcmp(t, "FileLine") ||
        !strcmp(t, "FileCountLines") || !strcmp(t, "FileCountFilter") ||
        !strcmp(t, "FileCountWord"))
    {
        scan_expr(jget(e, "file"));
        scan_expr(jget(e, "argument"));
        return;
    }
    if (!strcmp(t, "Call"))
    {
        JVal *args = jA(e, "args");
        if (args)
            for (int i = 0; i < args->arr.len; i++)
                scan_expr(args->arr.a[i]);
        return;
    }
}
static void scan_stmts(JVal *stmts, Scan *sc)
{
    if (!stmts || stmts->t != JT_ARR)
        return;
    for (int i = 0; i < stmts->arr.len; i++)
    {
        JVal *s = stmts->arr.a[i];
        const char *tp = jS(s, "type");
        if (!tp)
            continue;
        if (!strcmp(tp, "VarDecl"))
        {
            const char *nm = jS(s, "name");
            scan_expr(jget(s, "value"));
            if (jB(s, "scoped"))
            {
                int dup = 0;
                for (int j = 0; j < sc->ns; j++)
                    if (!strcmp(sc->sn[j], nm))
                    {
                        dup = 1;
                        break;
                    }
                if (!dup)
                    sc->sn[sc->ns++] = (char *)nm;
            }
            else
            {
                gvAdd(nm);
            }
        }
        else if (!strcmp(tp, "Print"))
        {
            scan_expr(jget(s, "value"));
        }
        else if (!strcmp(tp, "TxtDecl"))
        {
            const char *nm = jS(s, "name");
            scan_expr(jget(s, "value"));
            if (jB(s, "scoped"))
            {
                int dup = 0;
                for (int j = 0; j < sc->ns; j++)
                    if (!strcmp(sc->sn[j], nm))
                    {
                        dup = 1;
                        break;
                    }
                if (!dup)
                    sc->sn[sc->ns++] = (char *)nm;
            }
            else
            {
                gvAdd(nm);
            }
        }
        else if (!strcmp(tp, "FileDecl"))
        {
            const char *nm = jS(s, "name");
            scan_expr(jget(s, "value"));
            fileVarAdd(nm);
            gvAdd(nm);
        }
        else if (!strcmp(tp, "WriteFile"))
        {
            scan_expr(jget(s, "target"));
            scan_expr(jget(s, "value"));
        }
        else if (!strcmp(tp, "AugAssign") || !strcmp(tp, "Assign"))
        {
            scan_expr(jget(s, "value"));
            const char *nm = jS(s, "name");
            if (jB(s, "scoped"))
                scanAddLocal(sc, nm);
            else if (!scanHasLocal(sc, nm))
                gvAdd(nm);
        }
        else if (!strcmp(tp, "Return"))
        {
            scan_expr(jget(s, "value"));
        }
        else if (!strcmp(tp, "CallStmt"))
        {
            scan_expr(jget(s, "value"));
        }
        else if (!strcmp(tp, "Loop"))
        {
            scan_expr(jget(s, "count"));
            sc->nl++;
            scan_stmts(jA(s, "body"), sc);
        }
        else if (!strcmp(tp, "If"))
        {
            /* Scan condition — it may contain a String literal (e.g. x == "hi") */
            scan_expr(jget(s, "condition"));
            scan_stmts(jA(s, "body"), sc);
            JVal *eb = jget(s, "else_body");
            if (eb && eb->t == JT_ARR)
                scan_stmts(eb, sc);
        }
    }
}
/* ════════════════════════════════════════════════════════════
   §4  EMIT  HELPERS
   ════════════════════════════════════════════════════════════ */
static void asm_esc(const char *s)
{
    for (; *s; s++)
    {
        switch (*s)
        {
        case '\n':
            fputs("\\n", OUT);
            break;
        case '\t':
            fputs("\\t", OUT);
            break;
        case '\r':
            fputs("\\r", OUT);
            break;
        case '\\':
            fputs("\\\\", OUT);
            break;
        case '"':
            fputs("\\\"", OUT);
            break;
        default:
            fputc(*s, OUT);
            break;
        }
    }
}
static void emit_imm(long long v)
{
    fprintf(OUT, "\tmov  x8, #%lld\n", v & 0xFFFF);
    if ((v >> 16) & 0xFFFF)
        fprintf(OUT, "\tmovk x8, #0x%llx, lsl #16\n", (v >> 16) & 0xFFFF);
    if ((v >> 32) & 0xFFFF)
        fprintf(OUT, "\tmovk x8, #0x%llx, lsl #32\n", (v >> 32) & 0xFFFF);
    if ((v >> 48) & 0xFFFF)
        fprintf(OUT, "\tmovk x8, #0x%llx, lsl #48\n", (v >> 48) & 0xFFFF);
}
/* ════════════════════════════════════════════════════════════
   §4b  RUNTIME HELPER EMITTER
   ════════════════════════════════════════════════════════════ */
static void emit_runtime_helpers(void)
{
    fprintf(OUT,
            "; ── runtime helper: int64 → decimal string ──────────────\n"
            "; Input:  x0 = int64 value\n"
            "; Output: x0 = pointer to null-terminated string in _rt_buf\n"
            "; Clobbers: x1-x7\n"
            "_int64_to_str:\n"
            "\tstp  x29, x30, [sp, #-16]!\n"
            "\tmov  x29, sp\n"
            "\tadrp x1, _rt_buf@PAGE\n"
            "\tadd  x1, x1, _rt_buf@PAGEOFF   ; x1 = buf base\n"
            "\n"
            "\t; handle zero\n"
            "\tcbnz x0, _i2s_nonzero\n"
            "\tmov  w2, #48\n"
            "\tstrb w2, [x1]\n"
            "\tmov  w2, #10\n"
            "\tstrb w2, [x1, #1]              ; newline\n"
            "\tmov  w2, #0\n"
            "\tstrb w2, [x1, #2]              ; null terminator\n"
            "\tmov  x0, x1\n"
            "\tldp  x29, x30, [sp], #16\n"
            "\tret\n"
            "\n"
            "_i2s_nonzero:\n"
            "\tmov  x6, #0                    ; x6 = negative flag\n"
            "\ttbz  x0, #63, _i2s_positive\n"
            "\tmov  x6, #1\n"
            "\tneg  x0, x0\n"
            "_i2s_positive:\n"
            "\tadd  x3, x1, #30              ; x3 = write pointer (end)\n"
            "\tstrb w4, [x1, #1]             ; newline at buf+31\n"
            "\tmov  w4, #0\n"
            "\tstrb w4, [x3, #2]             ; null terminator at buf+32\n"
            "\n"
            "_i2s_digit_loop:\n"
            "\tcbz  x0, _i2s_done_digits\n"
            "\tmov  x5, #10\n"
            "\tudiv x7, x0, x5               ; x7 = x0 / 10\n"
            "\tmsub x4, x7, x5, x0           ; x4 = x0 %% 10\n"
            "\tadd  w4, w4, #48              ; to ASCII\n"
            "\tstrb w4, [x3]\n"
            "\tsub  x3, x3, #1\n"
            "\tmov  x0, x7\n"
            "\tb    _i2s_digit_loop\n"
            "\n"
            "_i2s_done_digits:\n"
            "\tcbz  x6, _i2s_no_neg\n"
            "\tmov  w4, #45                  ; '-'\n"
            "\tstrb w4, [x3]\n"
            "\tsub  x3, x3, #1\n"
            "_i2s_no_neg:\n"
            "\tadd  x0, x3, #1               ; x0 = pointer to first digit\n"
            "\tldp  x29, x30, [sp], #16\n"
            "\tret\n"
            "\n");
}
/* ════════════════════════════════════════════════════════════
   §5  CODE  EMITTER
   ════════════════════════════════════════════════════════════ */
static void emit_stmts(JVal *stmts); /* forward decl */
static void emit_expr(JVal *e);      /* forward decl */
/* Returns 1 if the expression node evaluates to a string pointer
   (either a String literal or a txt variable identifier).        */
static int is_str_expr(JVal *e)
{
    if (!e)
        return 0;
    const char *t = jS(e, "type");
    if (!t)
        return 0;
    if (!strcmp(t, "String"))
        return 1;
    if (!strcmp(t, "Identifier"))
        return isTxtVar(jS(e, "name"));
    if (!strcmp(t, "FileFilter") || !strcmp(t, "FileLine"))
        return 1;
    if (!strcmp(t, "Call"))
    {
        const char *rt = funcReturnType(jS(e, "name"));
        return rt && !strcmp(rt, "txt");
    }
    return 0;
}
static int is_file_object_expr(JVal *e)
{
    if (!e)
        return 0;
    const char *t = jS(e, "type");
    if (!t)
        return 0;
    if (!strcmp(t, "Identifier"))
        return isFileVar(jS(e, "name"));
    if (!strcmp(t, "Call"))
    {
        const char *rt = funcReturnType(jS(e, "name"));
        return rt && !strcmp(rt, "file");
    }
    return !strcmp(t, "LoadFile") || !strcmp(t, "CreateFile");
}

/* Evaluate a two-argument runtime call. The first result is kept on the
   stack because expression evaluation and the call may clobber x8-x17. */
static void emit_file_call(const char *symbol, JVal *file, JVal *arg)
{
    emit_expr(file);
    fprintf(OUT, "\tstr  x8, [sp, #-16]!\n");
    emit_expr(arg);
    fprintf(OUT, "\tmov  x1, x8\n");
    fprintf(OUT, "\tldr  x0, [sp], #16\n");
    fprintf(OUT, "\tbl   _%s\n", symbol);
    fprintf(OUT, "\tmov  x8, x0\n");
}
static void emit_call_args(JVal *args)
{
    if (!args || args->t != JT_ARR)
        return;
    if (args->arr.len > 8)
    {
        fprintf(stderr, "Error: functions support at most 8 arguments\n");
        exit(1);
    }
    for (int i = 0; i < args->arr.len; i++)
    {
        emit_expr(args->arr.a[i]);
        fprintf(OUT, "\tstr  x8, [sp, #-16]!\n");
    }
    for (int i = args->arr.len - 1; i >= 0; i--)
        fprintf(OUT, "\tldr  x%d, [sp], #16\n", i);
}
static void emit_expr(JVal *e)
{
    const char *t = jS(e, "type");
    if (!t)
        return;
    if (!strcmp(t, "Number"))
    {
        emit_imm(jN(e, "value"));
        return;
    }
    /* String literal: load its interned pointer into x8. */
    if (!strcmp(t, "String"))
    {
        char *proc = proc_esc(jS(e, "value"));
        int id = strIntern(proc);
        free(proc);
        fprintf(OUT, "\tadrp x8, _str%d@PAGE\n", id);
        fprintf(OUT, "\tadd  x8, x8, _str%d@PAGEOFF\n", id);
        return;
    }
    if (!strcmp(t, "Identifier"))
    {
        const char *nm = jS(e, "name");
        int local_off = lGet(nm);
        if (jB(e, "scoped") || local_off >= 0)
        {
            int off = local_off;
            if (off < 0)
            {
                fprintf(stderr, "Error: scoped var '%s' not declared\n", nm);
                exit(1);
            }
            fprintf(OUT, "\tldr  x8, [x29, #%d]\n", off);
        }
        else
        {
            fprintf(OUT, "\tadrp x9, _gv_%s@PAGE\n", nm);
            fprintf(OUT, "\tldr  x8, [x9, _gv_%s@PAGEOFF]\n", nm);
        }
        return;
    }
    if (!strcmp(t, "Call"))
    {
        emit_call_args(jA(e, "args"));
        fprintf(OUT, "\tbl   %s\n", jS(e, "name"));
        fprintf(OUT, "\tmov  x8, x0\n");
        return;
    }
    if (!strcmp(t, "LoadFile"))
    {
        emit_expr(jget(e, "argument"));
        fprintf(OUT, "\tmov  x0, x8\n");
        fprintf(OUT, "\tbl   _nevo_loadf\n");
        fprintf(OUT, "\tmov  x8, x0\n");
        return;
    }
    if (!strcmp(t, "CreateFile"))
    {
        emit_expr(jget(e, "argument"));
        fprintf(OUT, "\tmov  x0, x8\n");
        fprintf(OUT, "\tbl   _nevo_createf\n");
        fprintf(OUT, "\tmov  x8, x0\n");
        return;
    }
    if (!strcmp(t, "FileFilter"))
    {
        emit_file_call("nevo_file_filter", jget(e, "file"), jget(e, "argument"));
        return;
    }
    if (!strcmp(t, "FileLine"))
    {
        emit_file_call("nevo_file_line", jget(e, "file"), jget(e, "argument"));
        return;
    }
    if (!strcmp(t, "FileCountLines"))
    {
        emit_expr(jget(e, "file"));
        fprintf(OUT, "\tmov  x0, x8\n");
        fprintf(OUT, "\tbl   _nevo_file_count_lines\n");
        fprintf(OUT, "\tmov  x8, x0\n");
        return;
    }
    if (!strcmp(t, "FileCountFilter"))
    {
        JVal *filter = jget(e, "argument");
        emit_file_call("nevo_file_count_filter", jget(e, "file"),
                       jget(filter, "argument"));
        return;
    }
    if (!strcmp(t, "FileCountWord"))
    {
        JVal *word = jget(e, "argument");
        emit_file_call("nevo_file_count_word", jget(e, "file"),
                       jget(word, "argument"));
        return;
    }
    if (!strcmp(t, "BinaryOp"))
    {
        const char *op = jS(e, "op");
        emit_expr(jget(e, "left"));
        fprintf(OUT, "\tstr  x8, [sp, #-16]!\n");
        emit_expr(jget(e, "right"));
        fprintf(OUT, "\tldr  x10, [sp], #16\n");
        if (!strcmp(op, "+"))
            fprintf(OUT, "\tadd  x8, x10, x8\n");
        else if (!strcmp(op, "-"))
            fprintf(OUT, "\tsub  x8, x10, x8\n");
        else if (!strcmp(op, "*"))
            fprintf(OUT, "\tmul  x8, x10, x8\n");
        else if (!strcmp(op, "/"))
            fprintf(OUT, "\tsdiv x8, x10, x8\n");
        return;
    }
    if (!strcmp(t, "Cmp"))
    {
        const char *op = jS(e, "op");
        JVal *left = jget(e, "left");
        JVal *right = jget(e, "right");
        if (is_str_expr(left) || is_str_expr(right))
        {
            /* String comparison: call strcmp(left, right).
             * strcmp returns 0 for equal, <0 for less, >0 for greater. */
            emit_expr(left);
            fprintf(OUT, "\tstr  x8, [sp, #-16]!\n");
            emit_expr(right);
            fprintf(OUT, "\tmov  x1, x8\n");        /* x1 = right string ptr */
            fprintf(OUT, "\tldr  x0, [sp], #16\n"); /* x0 = left string ptr */
            fprintf(OUT, "\tbl   _strcmp\n");       /* x0 = strcmp result    */
            fprintf(OUT, "\tcmp  x0, #0\n");
        }
        else
        {
            /* Numeric comparison: left → x10, right → x8, cmp. */
            emit_expr(left);
            fprintf(OUT, "\tstr  x8, [sp, #-16]!\n");
            emit_expr(right);
            fprintf(OUT, "\tldr  x10, [sp], #16\n");
            fprintf(OUT, "\tcmp  x10, x8\n");
        }
        /* cset is the same for both paths — condition flags already set. */
        if (!strcmp(op, "=="))
            fprintf(OUT, "\tcset x8, eq\n");
        else if (!strcmp(op, "!="))
            fprintf(OUT, "\tcset x8, ne\n");
        else if (!strcmp(op, "<"))
            fprintf(OUT, "\tcset x8, lt\n");
        else if (!strcmp(op, ">"))
            fprintf(OUT, "\tcset x8, gt\n");
        else if (!strcmp(op, "<="))
            fprintf(OUT, "\tcset x8, le\n");
        else if (!strcmp(op, ">="))
            fprintf(OUT, "\tcset x8, ge\n");
        return;
    }
    /* ── random(min, max) ────────────────────────────────────────
     *  Calls arc4random_uniform(range) which returns a value in
     *  [0, range-1], then adds min to shift into [min, max].
     *
     *  x10 = min  (saved across the call on a 16-byte stack slot)
     *  x0  = max - min + 1  (range argument)
     *  After call: x8 = x0 (return value) + x10 (min)
     * ──────────────────────────────────────────────────────────── */
    if (!strcmp(t, "Random"))
    {
        emit_expr(jget(e, "min"));                /* x8 = min              */
        fprintf(OUT, "\tstr  x8, [sp, #-16]!\n"); /* save min             */
        emit_expr(jget(e, "max"));                /* x8 = max              */
        fprintf(OUT, "\tldr  x10, [sp], #16\n");  /* x10 = min            */
        fprintf(OUT, "\tsub  x8, x8, x10\n");     /* x8 = max - min        */
        fprintf(OUT, "\tadd  x8, x8, #1\n");      /* x8 = range            */
        fprintf(OUT, "\tmov  x0, x8\n");          /* x0 = arg for syscall  */
        /* Push min onto the stack (16-byte aligned) so the bl
           doesn't clobber it — x10 is caller-saved.              */
        fprintf(OUT, "\tstr  x10, [sp, #-16]!\n");
        fprintf(OUT, "\tbl   _arc4random_uniform\n");
        fprintf(OUT, "\tldr  x10, [sp], #16\n"); /* pop min            */
        fprintf(OUT, "\tadd  x8, x0, x10\n");    /* x8 = result + min  */
        return;
    }
}
static void emit_stmt(JVal *s)
{
    const char *t = jS(s, "type");
    if (!t)
        return;
    if (!strcmp(t, "VarDecl"))
    {
        const char *nm = jS(s, "name");
        emit_expr(jget(s, "value"));
        if (jB(s, "scoped"))
        {
            int off = lAlloc(nm);
            fprintf(OUT, "\tstr  x8, [x29, #%d]\n", off);
        }
        else
        {
            gvAdd(nm);
            fprintf(OUT, "\tadrp x9, _gv_%s@PAGE\n", nm);
            fprintf(OUT, "\tstr  x8, [x9, _gv_%s@PAGEOFF]\n", nm);
        }
        return;
    }
    if (!strcmp(t, "Print"))
    {
        JVal *val = jget(s, "value");
        const char *vt = jS(val, "type");
        if (vt && !strcmp(vt, "String"))
        {
            /* Inline string literal: intern and pass directly to puts. */
            char *proc = proc_esc(jS(val, "value"));
            int id = strIntern(proc);
            free(proc);
            fprintf(OUT, "\tadrp x0, _str%d@PAGE\n", id);
            fprintf(OUT, "\tadd  x0, x0, _str%d@PAGEOFF\n", id);
            fprintf(OUT, "\tbl   _puts\n");
        }
        else
        {
            emit_expr(val);
            fprintf(OUT, "\tmov  x0, x8\n");
            if (is_file_object_expr(val))
            {
                fprintf(OUT, "\tbl   _nevo_print_file\n");
                return;
            }
            if (vt && (!strcmp(vt, "FileFilter") || !strcmp(vt, "FileLine")))
            {
                /* Keep query output faithful while terminating a final line. */
                fprintf(OUT, "\tbl   _nevo_print_text\n");
                return;
            }
            /* txt variables already hold a char* — skip int→string conversion. */
            if (!is_str_expr(val))
                fprintf(OUT, "\tbl   _int64_to_str\n");
            fprintf(OUT, "\tbl   _puts\n");
        }
        return;
    }
    if (!strcmp(t, "TxtDecl"))
    {
        JVal *val = jget(s, "value");
        emit_expr(val);
        const char *nm = jS(s, "name");
        if (jB(s, "scoped"))
        {
            lTxtVars[lNTxtVar++] = strdup(nm);
            int off = lAlloc(nm);
            fprintf(OUT, "\tstr  x8, [x29, #%d]\n", off);
        }
        else
        {
            gTxtVars[gNTxtVar++] = strdup(nm);
            gvAdd(nm);
            fprintf(OUT, "\tadrp x9, _gv_%s@PAGE\n", nm);
            fprintf(OUT, "\tstr  x8, [x9, _gv_%s@PAGEOFF]\n", nm);
        }
        return;
    }
    if (!strcmp(t, "Assign"))
    {
        const char *nm = jS(s, "name");
        emit_expr(jget(s, "value"));
        if (jB(s, "scoped") || lGet(nm) >= 0)
        {
            int off = lGet(nm);
            if (off < 0)
            {
                fprintf(stderr, "Error: scoped var '%s' not declared\n", nm);
                exit(1);
            }
            fprintf(OUT, "\tstr  x8, [x29, #%d]\n", off);
        }
        else
        {
            fprintf(OUT, "\tadrp x9, _gv_%s@PAGE\n", nm);
            fprintf(OUT, "\tstr  x8, [x9, _gv_%s@PAGEOFF]\n", nm);
        }
        return;
    }
    if (!strcmp(t, "FileDecl"))
    {
        const char *nm = jS(s, "name");
        fileVarAdd(nm);
        gvAdd(nm);
        emit_expr(jget(s, "value"));
        fprintf(OUT, "\tadrp x9, _gv_%s@PAGE\n", nm);
        fprintf(OUT, "\tstr  x8, [x9, _gv_%s@PAGEOFF]\n", nm);
        return;
    }
    if (!strcmp(t, "WriteFile"))
    {
        JVal *target = jget(s, "target");
        JVal *value = jget(s, "value");
        const char *target_type = jS(target, "type");
        int value_is_file = is_file_object_expr(value);

        if (target_type && !strcmp(target_type, "FileLine"))
        {
            emit_expr(jget(target, "file"));
            fprintf(OUT, "\tstr  x8, [sp, #-16]!\n");
            emit_expr(jget(target, "argument"));
            fprintf(OUT, "\tstr  x8, [sp, #-16]!\n");
            emit_expr(value);
            fprintf(OUT, "\tmov  x2, x8\n");
            fprintf(OUT, "\tldr  x1, [sp], #16\n");
            fprintf(OUT, "\tldr  x0, [sp], #16\n");
            fprintf(OUT, "\tbl   _%s\n", value_is_file ? "nevo_write_line_file" : "nevo_write_line_text");
        }
        else
        {
            emit_expr(target);
            fprintf(OUT, "\tstr  x8, [sp, #-16]!\n");
            emit_expr(value);
            fprintf(OUT, "\tmov  x1, x8\n");
            fprintf(OUT, "\tldr  x0, [sp], #16\n");
            fprintf(OUT, "\tbl   _%s\n", value_is_file ? "nevo_write_file" : "nevo_write_text");
        }
        return;
    }
    if (!strcmp(t, "AugAssign"))
    {
        const char *nm = jS(s, "name");
        const char *op = jS(s, "op");
        int is_scoped = jB(s, "scoped") || lGet(nm) >= 0;
        if (is_scoped)
        {
            int off = lGet(nm);
            if (off < 0)
            {
                fprintf(stderr, "Error: scoped var '%s' not declared\n", nm);
                exit(1);
            }
            fprintf(OUT, "\tldr  x10, [x29, #%d]\n", off);
        }
        else
        {
            fprintf(OUT, "\tadrp x9, _gv_%s@PAGE\n", nm);
            fprintf(OUT, "\tldr  x10, [x9, _gv_%s@PAGEOFF]\n", nm);
        }
        emit_expr(jget(s, "value"));
        if (op && op[0] == '+')
            fprintf(OUT, "\tadd  x8, x10, x8\n");
        else
            fprintf(OUT, "\tsub  x8, x10, x8\n");
        if (is_scoped)
        {
            int off = lGet(nm);
            fprintf(OUT, "\tstr  x8, [x29, #%d]\n", off);
        }
        else
        {
            fprintf(OUT, "\tadrp x9, _gv_%s@PAGE\n", nm);
            fprintf(OUT, "\tstr  x8, [x9, _gv_%s@PAGEOFF]\n", nm);
        }
        return;
    }
    if (!strcmp(t, "Jump"))
    {
        emit_call_args(jA(s, "args"));
        fprintf(OUT, "\tldp  x29, x30, [sp], #%d\n", gFsz);
        fprintf(OUT, "\tb    %s\n", jS(s, "target"));
        return;
    }
    if (!strcmp(t, "CallStmt"))
    {
        emit_expr(jget(s, "value"));
        return;
    }
    if (!strcmp(t, "Return"))
    {
        JVal *value = jget(s, "value");
        if (value && value->t != JT_NULL)
        {
            emit_expr(value);
            fprintf(OUT, "\tmov  x0, x8\n");
        }
        else
        {
            fprintf(OUT, "\tmov  x0, #0\n");
        }
        fprintf(OUT, "\tldp  x29, x30, [sp], #%d\n", gFsz);
        fprintf(OUT, "\tret\n");
        return;
    }
    if (!strcmp(t, "Loop"))
    {
        int uid = gLoopUID++;
        int coff = gLctrOff;
        gLctrOff += 8;
        emit_expr(jget(s, "count"));
        fprintf(OUT, "\tstr  x8, [x29, #%d]         ; loop counter\n", coff);
        fprintf(OUT, "_Lstart%d:\n", uid);
        fprintf(OUT, "\tldr  x8, [x29, #%d]\n", coff);
        fprintf(OUT, "\tcbz  x8, _Lend%d\n", uid);
        emit_stmts(jA(s, "body"));
        fprintf(OUT, "\tldr  x8, [x29, #%d]\n", coff);
        fprintf(OUT, "\tsub  x8, x8, #1\n");
        fprintf(OUT, "\tstr  x8, [x29, #%d]\n", coff);
        fprintf(OUT, "\tb    _Lstart%d\n", uid);
        fprintf(OUT, "_Lend%d:\n", uid);
        gLctrOff -= 8;
        return;
    }
    if (!strcmp(t, "If"))
    {
        int uid = gIfUID++;
        emit_expr(jget(s, "condition"));
        fprintf(OUT, "\tcbz  x8, _Ifalse%d\n", uid);
        emit_stmts(jA(s, "body"));
        JVal *eb = jget(s, "else_body");
        int has_else = eb && eb->t == JT_ARR;
        if (has_else)
            fprintf(OUT, "\tb    _Iend%d\n", uid);
        fprintf(OUT, "_Ifalse%d:\n", uid);
        if (has_else)
        {
            emit_stmts(eb);
            fprintf(OUT, "_Iend%d:\n", uid);
        }
        return;
    }
}
static void emit_stmts(JVal *stmts)
{
    if (!stmts || stmts->t != JT_ARR)
        return;
    for (int i = 0; i < stmts->arr.len; i++)
        emit_stmt(stmts->arr.a[i]);
}
static void emit_fn(JVal *fn)
{
    const char *name = jS(fn, "name");
    JVal *body = jA(fn, "body");
    JVal *params = jA(fn, "params");
    Scan sc = {{0}, 0, 0};
    scanParams(fn, &sc);
    scan_stmts(body, &sc);
    int raw = 16 + 8 * (sc.ns + sc.nl);
    int fsz = (raw + 15) & ~15;
    gFsz = fsz;
    gLctrOff = 16 + 8 * sc.ns;
    lReset();
    fprintf(OUT, "%s:\n", name);
    fprintf(OUT, "\tstp  x29, x30, [sp, #-%d]!\n", fsz);
    fprintf(OUT, "\tmov  x29, sp\n");
    if (params)
        for (int i = 0; i < params->arr.len; i++)
        {
            int off = lAlloc(params->arr.a[i]->s);
            fprintf(OUT, "\tstr  x%d, [x29, #%d]       ; parameter %s\n",
                    i, off, params->arr.a[i]->s);
        }
    emit_stmts(body);
    int last_is_jump = 0;
    if (body && body->arr.len > 0)
    {
        const char *lt = jS(body->arr.a[body->arr.len - 1], "type");
        if (lt && (!strcmp(lt, "Jump") || !strcmp(lt, "Return")))
            last_is_jump = 1;
    }
    if (!last_is_jump)
    {
        fprintf(OUT, "\tmov  x0, #0\n");
        fprintf(OUT, "\tldp  x29, x30, [sp], #%d\n", fsz);
        fprintf(OUT, "\tret\n");
    }
    fprintf(OUT, "\n");
}

#ifdef NEVO_TARGET_WINDOWS_X64
/* ════════════════════════════════════════════════════════════
   §6  WINDOWS x86-64 / NASM BACKEND

   Expressions return their value in RAX. Calls follow the Microsoft x64
   ABI: RCX, RDX, R8 and R9 hold the first four arguments, later arguments
   are placed after the mandatory 32-byte shadow space.
   ════════════════════════════════════════════════════════════ */
static void win_emit_expr(JVal *e);
static void win_emit_stmts(JVal *stmts);

static const char *win_symbol(const char *name)
{
    return !strcmp(name, "_main") ? "main" : name;
}

static void win_push_rax(void)
{
    fprintf(OUT, "\tsub rsp, 16\n\tmov [rsp], rax\n");
}

static void win_pop(const char *reg)
{
    fprintf(OUT, "\tmov %s, [rsp]\n\tadd rsp, 16\n", reg);
}

static void win_call0(const char *symbol)
{
    fprintf(OUT, "\tsub rsp, 32\n\tcall %s\n\tadd rsp, 32\n", symbol);
}

static void win_call1(const char *symbol, JVal *first)
{
    win_emit_expr(first);
    fprintf(OUT, "\tmov rcx, rax\n");
    win_call0(symbol);
}

static void win_call2(const char *symbol, JVal *first, JVal *second)
{
    win_emit_expr(first);
    win_push_rax();
    win_emit_expr(second);
    win_push_rax();
    fprintf(OUT,
            "\tsub rsp, 32\n"
            "\tmov rdx, [rsp + 32]\n"
            "\tmov rcx, [rsp + 48]\n"
            "\tcall %s\n"
            "\tadd rsp, 64\n", symbol);
}

static void win_call3(const char *symbol, JVal *first, JVal *second, JVal *third)
{
    win_emit_expr(first);
    win_push_rax();
    win_emit_expr(second);
    win_push_rax();
    win_emit_expr(third);
    win_push_rax();
    fprintf(OUT,
            "\tsub rsp, 32\n"
            "\tmov r8,  [rsp + 32]\n"
            "\tmov rdx, [rsp + 48]\n"
            "\tmov rcx, [rsp + 64]\n"
            "\tcall %s\n"
            "\tadd rsp, 80\n", symbol);
}

static void win_emit_user_call(const char *symbol, JVal *args)
{
    int count = args && args->t == JT_ARR ? args->arr.len : 0;
    if (count > 8)
    {
        fprintf(stderr, "Error: functions support at most 8 arguments\n");
        exit(1);
    }
    for (int i = 0; i < count; i++)
    {
        win_emit_expr(args->arr.a[i]);
        win_push_rax();
    }

    int stack_count = count > 4 ? count - 4 : 0;
    int call_area = 32 + stack_count * 8;
    call_area = (call_area + 15) & ~15;
    fprintf(OUT, "\tsub rsp, %d\n", call_area);

    static const char *registers[] = {"rcx", "rdx", "r8", "r9"};
    for (int i = 0; i < count && i < 4; i++)
        fprintf(OUT, "\tmov %s, [rsp + %d]\n", registers[i],
                call_area + 16 * (count - 1 - i));
    for (int i = 4; i < count; i++)
    {
        fprintf(OUT, "\tmov r11, [rsp + %d]\n",
                call_area + 16 * (count - 1 - i));
        fprintf(OUT, "\tmov [rsp + %d], r11\n", 32 + 8 * (i - 4));
    }
    fprintf(OUT, "\tcall %s\n", win_symbol(symbol));
    fprintf(OUT, "\tadd rsp, %d\n", call_area + count * 16);
}

static void win_emit_expr(JVal *e)
{
    const char *t = jS(e, "type");
    if (!t)
        return;
    if (!strcmp(t, "Number"))
    {
        fprintf(OUT, "\tmov rax, %lld\n", jN(e, "value"));
        return;
    }
    if (!strcmp(t, "String"))
    {
        char *processed = proc_esc(jS(e, "value"));
        int id = strIntern(processed);
        free(processed);
        fprintf(OUT, "\tlea rax, [rel str%d]\n", id);
        return;
    }
    if (!strcmp(t, "Identifier"))
    {
        const char *name = jS(e, "name");
        int offset = lGet(name);
        if (jB(e, "scoped") || offset >= 0)
        {
            if (offset < 0)
            {
                fprintf(stderr, "Error: scoped var '%s' not declared\n", name);
                exit(1);
            }
            fprintf(OUT, "\tmov rax, [rbp - %d]\n", offset);
        }
        else
            fprintf(OUT, "\tmov rax, [rel gv_%s]\n", name);
        return;
    }
    if (!strcmp(t, "Call"))
    {
        win_emit_user_call(jS(e, "name"), jA(e, "args"));
        return;
    }
    if (!strcmp(t, "LoadFile"))
    {
        win_call1("nevo_loadf", jget(e, "argument"));
        return;
    }
    if (!strcmp(t, "CreateFile"))
    {
        win_call1("nevo_createf", jget(e, "argument"));
        return;
    }
    if (!strcmp(t, "FileFilter"))
    {
        win_call2("nevo_file_filter", jget(e, "file"), jget(e, "argument"));
        return;
    }
    if (!strcmp(t, "FileLine"))
    {
        win_call2("nevo_file_line", jget(e, "file"), jget(e, "argument"));
        return;
    }
    if (!strcmp(t, "FileCountLines"))
    {
        win_call1("nevo_file_count_lines", jget(e, "file"));
        return;
    }
    if (!strcmp(t, "FileCountFilter"))
    {
        JVal *filter = jget(e, "argument");
        win_call2("nevo_file_count_filter", jget(e, "file"),
                  jget(filter, "argument"));
        return;
    }
    if (!strcmp(t, "FileCountWord"))
    {
        JVal *word = jget(e, "argument");
        win_call2("nevo_file_count_word", jget(e, "file"),
                  jget(word, "argument"));
        return;
    }
    if (!strcmp(t, "UnaryNeg"))
    {
        win_emit_expr(jget(e, "operand"));
        fprintf(OUT, "\tneg rax\n");
        return;
    }
    if (!strcmp(t, "BinaryOp"))
    {
        const char *op = jS(e, "op");
        win_emit_expr(jget(e, "left"));
        win_push_rax();
        win_emit_expr(jget(e, "right"));
        win_pop("r10");
        if (!strcmp(op, "+"))
            fprintf(OUT, "\tadd rax, r10\n");
        else if (!strcmp(op, "-"))
            fprintf(OUT, "\tmov r11, rax\n\tmov rax, r10\n\tsub rax, r11\n");
        else if (!strcmp(op, "*"))
            fprintf(OUT, "\timul rax, r10\n");
        else if (!strcmp(op, "/"))
            fprintf(OUT, "\tmov r11, rax\n\tmov rax, r10\n\tcqo\n\tidiv r11\n");
        return;
    }
    if (!strcmp(t, "Cmp"))
    {
        const char *op = jS(e, "op");
        JVal *left = jget(e, "left");
        JVal *right = jget(e, "right");
        if (is_str_expr(left) || is_str_expr(right))
        {
            win_call2("strcmp", left, right);
            fprintf(OUT, "\ttest eax, eax\n");
        }
        else
        {
            win_emit_expr(left);
            win_push_rax();
            win_emit_expr(right);
            win_pop("r10");
            fprintf(OUT, "\tcmp r10, rax\n");
        }
        const char *condition = "e";
        if (!strcmp(op, "!=")) condition = "ne";
        else if (!strcmp(op, "<")) condition = "l";
        else if (!strcmp(op, ">")) condition = "g";
        else if (!strcmp(op, "<=")) condition = "le";
        else if (!strcmp(op, ">=")) condition = "ge";
        fprintf(OUT, "\tset%s al\n\tmovzx rax, al\n", condition);
        return;
    }
    if (!strcmp(t, "Random"))
    {
        win_call2("nevo_random_range", jget(e, "min"), jget(e, "max"));
        return;
    }
}

static void win_store_name(const char *name, int scoped)
{
    int offset = lGet(name);
    if (scoped || offset >= 0)
    {
        if (offset < 0)
            offset = lAlloc(name);
        fprintf(OUT, "\tmov [rbp - %d], rax\n", offset);
    }
    else
    {
        gvAdd(name);
        fprintf(OUT, "\tmov [rel gv_%s], rax\n", name);
    }
}

static void win_emit_stmt(JVal *s)
{
    const char *t = jS(s, "type");
    if (!t)
        return;
    if (!strcmp(t, "VarDecl"))
    {
        win_emit_expr(jget(s, "value"));
        win_store_name(jS(s, "name"), jB(s, "scoped"));
        return;
    }
    if (!strcmp(t, "TxtDecl"))
    {
        const char *name = jS(s, "name");
        win_emit_expr(jget(s, "value"));
        if (jB(s, "scoped"))
            lTxtVars[lNTxtVar++] = strdup(name);
        else
            gTxtVars[gNTxtVar++] = strdup(name);
        win_store_name(name, jB(s, "scoped"));
        return;
    }
    if (!strcmp(t, "FileDecl"))
    {
        const char *name = jS(s, "name");
        fileVarAdd(name);
        win_emit_expr(jget(s, "value"));
        win_store_name(name, 0);
        return;
    }
    if (!strcmp(t, "Assign"))
    {
        win_emit_expr(jget(s, "value"));
        win_store_name(jS(s, "name"), jB(s, "scoped"));
        return;
    }
    if (!strcmp(t, "AugAssign"))
    {
        const char *name = jS(s, "name");
        const char *op = jS(s, "op");
        int offset = lGet(name);
        int scoped = jB(s, "scoped") || offset >= 0;
        if (scoped)
            fprintf(OUT, "\tmov rax, [rbp - %d]\n", offset);
        else
            fprintf(OUT, "\tmov rax, [rel gv_%s]\n", name);
        win_push_rax();
        win_emit_expr(jget(s, "value"));
        win_pop("r10");
        if (op && op[0] == '+')
            fprintf(OUT, "\tadd rax, r10\n");
        else
            fprintf(OUT, "\tmov r11, rax\n\tmov rax, r10\n\tsub rax, r11\n");
        if (scoped)
            fprintf(OUT, "\tmov [rbp - %d], rax\n", offset);
        else
            fprintf(OUT, "\tmov [rel gv_%s], rax\n", name);
        return;
    }
    if (!strcmp(t, "Print"))
    {
        JVal *value = jget(s, "value");
        const char *value_type = jS(value, "type");
        win_emit_expr(value);
        fprintf(OUT, "\tmov rcx, rax\n");
        if (is_file_object_expr(value))
            win_call0("nevo_print_file");
        else if ((value_type && (!strcmp(value_type, "FileFilter") ||
                                !strcmp(value_type, "FileLine"))))
            win_call0("nevo_print_text");
        else if (is_str_expr(value))
            win_call0("puts");
        else
            win_call0("nevo_print_num");
        return;
    }
    if (!strcmp(t, "WriteFile"))
    {
        JVal *target = jget(s, "target");
        JVal *value = jget(s, "value");
        int file_value = is_file_object_expr(value);
        if (!strcmp(jS(target, "type"), "FileLine"))
            win_call3(file_value ? "nevo_write_line_file" : "nevo_write_line_text",
                      jget(target, "file"), jget(target, "argument"), value);
        else
            win_call2(file_value ? "nevo_write_file" : "nevo_write_text",
                      target, value);
        return;
    }
    if (!strcmp(t, "CallStmt"))
    {
        win_emit_expr(jget(s, "value"));
        return;
    }
    if (!strcmp(t, "Return"))
    {
        JVal *value = jget(s, "value");
        if (value && value->t != JT_NULL)
            win_emit_expr(value);
        else
            fprintf(OUT, "\txor eax, eax\n");
        fprintf(OUT, "\tleave\n\tret\n");
        return;
    }
    if (!strcmp(t, "Jump"))
    {
        win_emit_user_call(jS(s, "target"), jA(s, "args"));
        fprintf(OUT, "\tleave\n\tret\n");
        return;
    }
    if (!strcmp(t, "Loop"))
    {
        int id = gLoopUID++;
        int offset = gLctrOff;
        gLctrOff += 8;
        win_emit_expr(jget(s, "count"));
        fprintf(OUT, "\tmov [rbp - %d], rax\nLw_loop_%d:\n", offset, id);
        fprintf(OUT, "\tcmp qword [rbp - %d], 0\n\tje Lw_loop_end_%d\n", offset, id);
        win_emit_stmts(jA(s, "body"));
        fprintf(OUT, "\tdec qword [rbp - %d]\n\tjmp Lw_loop_%d\nLw_loop_end_%d:\n",
                offset, id, id);
        gLctrOff -= 8;
        return;
    }
    if (!strcmp(t, "If"))
    {
        int id = gIfUID++;
        JVal *else_body = jget(s, "else_body");
        int has_else = else_body && else_body->t == JT_ARR;
        win_emit_expr(jget(s, "condition"));
        fprintf(OUT, "\ttest rax, rax\n\tjz Lw_if_false_%d\n", id);
        win_emit_stmts(jA(s, "body"));
        if (has_else)
            fprintf(OUT, "\tjmp Lw_if_end_%d\n", id);
        fprintf(OUT, "Lw_if_false_%d:\n", id);
        if (has_else)
        {
            win_emit_stmts(else_body);
            fprintf(OUT, "Lw_if_end_%d:\n", id);
        }
        return;
    }
}

static void win_emit_stmts(JVal *stmts)
{
    if (!stmts || stmts->t != JT_ARR)
        return;
    for (int i = 0; i < stmts->arr.len; i++)
        win_emit_stmt(stmts->arr.a[i]);
}

static void win_emit_fn(JVal *fn)
{
    const char *source_name = jS(fn, "name");
    const char *name = win_symbol(source_name);
    JVal *body = jA(fn, "body");
    JVal *params = jA(fn, "params");
    Scan scan = {{0}, 0, 0};
    scanParams(fn, &scan);
    scan_stmts(body, &scan);
    gFsz = (16 + 8 * (scan.ns + scan.nl) + 15) & ~15;
    gLctrOff = 16 + 8 * scan.ns;
    lReset();

    fprintf(OUT, "%s:\n\tpush rbp\n\tmov rbp, rsp\n\tsub rsp, %d\n", name, gFsz);
    if (params)
    {
        static const char *registers[] = {"rcx", "rdx", "r8", "r9"};
        for (int i = 0; i < params->arr.len; i++)
        {
            int offset = lAlloc(params->arr.a[i]->s);
            if (i < 4)
                fprintf(OUT, "\tmov [rbp - %d], %s ; parameter %s\n",
                        offset, registers[i], params->arr.a[i]->s);
            else
            {
                fprintf(OUT, "\tmov rax, [rbp + %d]\n", 48 + 8 * (i - 4));
                fprintf(OUT, "\tmov [rbp - %d], rax ; parameter %s\n",
                        offset, params->arr.a[i]->s);
            }
        }
    }
    win_emit_stmts(body);
    int terminated = 0;
    if (body && body->arr.len)
    {
        const char *last = jS(body->arr.a[body->arr.len - 1], "type");
        terminated = last && (!strcmp(last, "Jump") || !strcmp(last, "Return"));
    }
    if (!terminated)
        fprintf(OUT, "\txor eax, eax\n\tleave\n\tret\n");
    fprintf(OUT, "\n");
}

static void win_emit_program(JVal *fns)
{
    fprintf(OUT,
            "; Windows x86-64 NASM assembly generated by Nevo v1.2\n"
            "bits 64\n"
            "default rel\n\n"
            "section .rdata\n");
    for (int i = 0; i < gNStr; i++)
    {
        fprintf(OUT, "str%d: db ", i);
        const unsigned char *text = (const unsigned char *)gStrs[i].s;
        for (; *text; text++)
            fprintf(OUT, "%u,", (unsigned)*text);
        fprintf(OUT, "0\n");
    }
    fprintf(OUT, "\nsection .bss\nalign 8\n");
    for (int i = 0; i < gNVar; i++)
        fprintf(OUT, "gv_%s: resq 1\n", gVars[i]);
    fprintf(OUT,
            "\nsection .text\n"
            "global main\n"
            "extern puts\n"
            "extern strcmp\n"
            "extern nevo_loadf\n"
            "extern nevo_createf\n"
            "extern nevo_print_text\n"
            "extern nevo_print_file\n"
            "extern nevo_print_num\n"
            "extern nevo_random_range\n"
            "extern nevo_file_filter\n"
            "extern nevo_file_line\n"
            "extern nevo_file_count_lines\n"
            "extern nevo_file_count_filter\n"
            "extern nevo_file_count_word\n"
            "extern nevo_write_text\n"
            "extern nevo_write_file\n"
            "extern nevo_write_line_text\n"
            "extern nevo_write_line_file\n\n");
    for (int i = 0; fns && i < fns->arr.len; i++)
        win_emit_fn(fns->arr.a[i]);
}
#endif

int nevo_codegen_file(const char *json_path, const char *assembly_path)
{
    char *json = nevo_read_text_file(json_path);
    if (!json)
        return 1;
    P = json;
    JVal *ast = parse_val();
    OUT = assembly_path ? fopen(assembly_path, "w") : stdout;
    if (!OUT)
    {
        perror(assembly_path);
        exit(1);
    }
    JVal *fns = jA(ast, "functions");
    if (fns)
    {
        for (int i = 0; i < fns->arr.len; i++)
        {
            gFuncNames[gNFunc] = strdup(jS(fns->arr.a[i], "name"));
            const char *rt = jS(fns->arr.a[i], "return_type");
            gFuncReturnTypes[gNFunc] = strdup(rt ? rt : "void");
            gNFunc++;
        }
        for (int i = 0; i < fns->arr.len; i++)
        {
            Scan sc = {{0}, 0, 0};
            scanParams(fns->arr.a[i], &sc);
            scan_stmts(jA(fns->arr.a[i], "body"), &sc);
        }
    }
    /* ── data section ─────────────────────────────────────── */
#ifdef NEVO_TARGET_WINDOWS_X64
    win_emit_program(fns);
#else
    fprintf(OUT, ".section __TEXT,__cstring,cstring_literals\n");
    for (int i = 0; i < gNStr; i++)
    {
        fprintf(OUT, "_str%d:\n\t.asciz \"", i);
        asm_esc(gStrs[i].s);
        fprintf(OUT, "\"\n");
    }
    fprintf(OUT, "\n");
    fprintf(OUT, ".section __DATA,__bss\n");
    fprintf(OUT, "_rt_buf:\n\t.space 33\n\n");
    for (int i = 0; i < gNVar; i++)
        fprintf(OUT, ".comm _gv_%s, 8, 3\n", gVars[i]);
    if (gNVar)
        fprintf(OUT, "\n");
    /* ── code section ─────────────────────────────────────── */
    fprintf(OUT, ".section __TEXT,__text\n");
    fprintf(OUT, ".globl _main\n\n");
    emit_runtime_helpers();
    if (fns)
        for (int i = 0; i < fns->arr.len; i++)
            emit_fn(fns->arr.a[i]);
#endif
    if (OUT != stdout)
        fclose(OUT);
    free(json);
    return 0;
}

#ifndef NEVO_LIBRARY_BUILD
int main(int argc, char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <ast.json> <output.asm>\n", argv[0]);
        return 1;
    }
    return nevo_codegen_file(argv[1], argv[2]);
}
#endif
