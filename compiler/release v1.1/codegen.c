/* ast_to_arm64.c  ──  AST (JSON) → ARM64 Apple Silicon Assembly
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
 * Build:   cc -O2 -o ast_to_arm64 ast_to_arm64.c
 * Usage:   ./ast_to_arm64 input.json [output.s]
 * Assemble/link: clang -arch arm64 output.s -o program
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
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
            JVal *val = jget(s, "value");
            if (val)
            {
                char *p = proc_esc(jS(val, "value"));
                strIntern(p);
                free(p);
            }
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
        else if (!strcmp(tp, "AugAssign"))
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
            "\tmsub x4, x7, x5, x0           ; x4 = x0 % 10\n"
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
    return 0;
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
        if (jB(e, "scoped"))
        {
            int off = lGet(nm);
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
    if (!strcmp(t, "BinaryOp"))
    {
        const char *op = jS(e, "op");
        emit_expr(jget(e, "left"));
        fprintf(OUT, "\tmov  x10, x8\n");
        emit_expr(jget(e, "right"));
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
            fprintf(OUT, "\tmov  x0, x8\n"); /* x0 = left string ptr  */
            emit_expr(right);
            fprintf(OUT, "\tmov  x1, x8\n");  /* x1 = right string ptr */
            fprintf(OUT, "\tbl   _strcmp\n"); /* x0 = strcmp result    */
            fprintf(OUT, "\tcmp  x0, #0\n");
        }
        else
        {
            /* Numeric comparison: left → x10, right → x8, cmp. */
            emit_expr(left);
            fprintf(OUT, "\tmov  x10, x8\n");
            emit_expr(right);
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
        emit_expr(jget(e, "min"));            /* x8 = min              */
        fprintf(OUT, "\tmov  x10, x8\n");     /* x10 = min             */
        emit_expr(jget(e, "max"));            /* x8 = max              */
        fprintf(OUT, "\tsub  x8, x8, x10\n"); /* x8 = max - min        */
        fprintf(OUT, "\tadd  x8, x8, #1\n");  /* x8 = range            */
        fprintf(OUT, "\tmov  x0, x8\n");      /* x0 = arg for syscall  */
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
            /* txt variables already hold a char* — skip int→string conversion. */
            const char *nm = jS(val, "name");
            if (!isTxtVar(nm))
                fprintf(OUT, "\tbl   _int64_to_str\n");
            fprintf(OUT, "\tbl   _puts\n");
        }
        return;
    }
    if (!strcmp(t, "TxtDecl"))
    {
        JVal *val = jget(s, "value");
        char *proc = proc_esc(jS(val, "value"));
        int id = strIntern(proc);
        free(proc);
        fprintf(OUT, "\tadrp x8, _str%d@PAGE\n", id);
        fprintf(OUT, "\tadd  x8, x8, _str%d@PAGEOFF\n", id);
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
    if (!strcmp(t, "AugAssign"))
    {
        const char *nm = jS(s, "name");
        const char *op = jS(s, "op");
        int is_scoped = jB(s, "scoped");
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
        fprintf(OUT, "\tldp  x29, x30, [sp], #%d\n", gFsz);
        fprintf(OUT, "\tb    %s\n", jS(s, "target"));
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
    Scan sc = {{0}, 0, 0};
    scan_stmts(body, &sc);
    int raw = 16 + 8 * (sc.ns + sc.nl);
    int fsz = (raw + 15) & ~15;
    gFsz = fsz;
    gLctrOff = 16 + 8 * sc.ns;
    lReset();
    fprintf(OUT, "%s:\n", name);
    fprintf(OUT, "\tstp  x29, x30, [sp, #-%d]!\n", fsz);
    fprintf(OUT, "\tmov  x29, sp\n");
    emit_stmts(body);
    int last_is_jump = 0;
    if (body && body->arr.len > 0)
    {
        const char *lt = jS(body->arr.a[body->arr.len - 1], "type");
        if (lt && !strcmp(lt, "Jump"))
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
/* ════════════════════════════════════════════════════════════
   §6  MAIN
   ════════════════════════════════════════════════════════════ */
static char *read_file(const char *path)
{
    FILE *f = path ? fopen(path, "r") : stdin;
    if (!f)
    {
        perror(path ? path : "stdin");
        exit(1);
    }
    size_t cap = 4096, len = 0;
    char *b = malloc(cap);
    int c;
    while ((c = fgetc(f)) != EOF)
    {
        if (len + 2 > cap)
            b = realloc(b, cap *= 2);
        b[len++] = (char)c;
    }
    b[len] = '\0';
    if (path)
        fclose(f);
    return b;
}
int main(int argc, char **argv)
{
    char *json = read_file(argc > 1 ? argv[1] : NULL);
    P = json;
    JVal *ast = parse_val();
    OUT = (argc > 2) ? fopen(argv[2], "w") : stdout;
    if (!OUT)
    {
        perror(argv[2]);
        exit(1);
    }
    JVal *fns = jA(ast, "functions");
    if (fns)
    {
        for (int i = 0; i < fns->arr.len; i++)
        {
            Scan sc = {{0}, 0, 0};
            scan_stmts(jA(fns->arr.a[i], "body"), &sc);
        }
    }
    /* ── data section ─────────────────────────────────────── */
    fprintf(OUT, "; ARM64 Apple Silicon Assembly\n");
    fprintf(OUT, "; Generated by ast_to_arm64.c\n");
    fprintf(OUT, ";\n");
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
    if (OUT != stdout)
        fclose(OUT);
    free(json);
    return 0;
}