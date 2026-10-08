/*
 * formatter.c
 * Parses source and emits an AST as JSON.
 *
 * Usage:  ./formatter <source_file>
 *         ./formatter <source_file> <out.json>
 *
 * Language quick-ref:
 *   Functions (top-level only):  _name(p1, p2) { ... }
 *   Entry point:                 _main() { ... }
 *   Jump:                        jump _name(arg1, arg2)
 *   Variable:                    num x = <expr>
 *   String variable:             txt x = "hello"
 *   Scoped variable:             scoped num x = <expr>
 *   Scoped string variable:      scoped txt x = "hello"
 *   Scoped access:               $x
 *   Augmented assign:            x += <expr> | x -= <expr>
 *   Print:                       print("text") | print(x) | print($x)
 *   Loop:                        loop <expr> { ... }
 *   If:                          if <expr> { ... }
 *   String comparison:           if (x == "hello") { ... }
 *   Comparison ops:              == != < > <= >=
 *   Comments:                    # line comment
 *   Random:                      random(min, max)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* =========================================================
 *  LEXER
 * ========================================================= */

typedef enum
{
    TOK_EOF,        /* end of file                            */
    TOK_IDENT,      /* regular name or _funcname              */
    TOK_SCOPED_VAR, /* $varname                               */
    TOK_NUMBER,     /* numeric literal                        */
    TOK_STRING,     /* "text"                                 */
    TOK_KW_NUM,     /* num                                    */
    TOK_KW_TXT,     /* txt                                    */
    TOK_KW_SCOPED,  /* scoped                                 */
    TOK_KW_PRINT,   /* print                                  */
    TOK_KW_LOOP,    /* loop                                   */
    TOK_KW_JUMP,    /* jump                                   */
    TOK_KW_IF,      /* if                                     */
    TOK_KW_ELSE,    /* else                                   */
    TOK_KW_RANDOM,  /* random                                 */
    TOK_EQUALS,     /* =                                      */
    TOK_EQEQ,       /* ==                                     */
    TOK_NEQ,        /* !=                                     */
    TOK_LT,         /* <                                      */
    TOK_GT,         /* >                                      */
    TOK_LTE,        /* <=                                     */
    TOK_GTE,        /* >=                                     */
    TOK_BANG,       /* !                                      */
    TOK_PLUS,       /* +                                      */
    TOK_MINUS,      /* -                                      */
    TOK_PLUS_EQ,    /* +=                                     */
    TOK_MINUS_EQ,   /* -=                                     */
    TOK_STAR,       /* *                                      */
    TOK_SLASH,      /* /                                      */
    TOK_LPAREN,     /* (                                      */
    TOK_RPAREN,     /* )                                      */
    TOK_LBRACE,     /* {                                      */
    TOK_RBRACE,     /* }                                      */
    TOK_COMMA,      /* ,                                      */
} TokenKind;

typedef struct
{
    TokenKind kind; /* kind of token                     */
    char text[512]; /* raw text of the token             */
    double num_val; /* parsed value for TOK_NUMBER       */
    int line;       /* 1-based source line               */
} Token;

static const char *g_src;
static int g_pos;
static int g_line;
static Token g_cur;

static void skip_ws_comments(void)
{
    for (;;)
    {
        while (g_src[g_pos] && isspace((unsigned char)g_src[g_pos]))
        {
            if (g_src[g_pos] == '\n')
                g_line++;
            g_pos++;
        }
        if (g_src[g_pos] == '#')
        {
            while (g_src[g_pos] && g_src[g_pos] != '\n')
                g_pos++;
        }
        else
        {
            break;
        }
    }
}

static Token lex_next(void)
{
    skip_ws_comments();
    Token t = {0};
    t.line = g_line;

    char c = g_src[g_pos];
    if (!c)
    {
        t.kind = TOK_EOF;
        return t;
    }

    /* String literal */
    if (c == '"')
    {
        g_pos++;
        int i = 0;
        while (g_src[g_pos] && g_src[g_pos] != '"')
        {
            if (g_src[g_pos] == '\\' && g_src[g_pos + 1])
            {
                t.text[i++] = '\\';
                t.text[i++] = g_src[++g_pos];
            }
            else
            {
                t.text[i++] = g_src[g_pos];
            }
            g_pos++;
        }
        t.text[i] = '\0';
        if (g_src[g_pos] == '"')
            g_pos++;
        t.kind = TOK_STRING;
        return t;
    }

    /* Scoped variable  $name */
    if (c == '$')
    {
        g_pos++;
        int i = 0;
        while (isalnum((unsigned char)g_src[g_pos]) || g_src[g_pos] == '_')
            t.text[i++] = g_src[g_pos++];
        t.text[i] = '\0';
        t.kind = TOK_SCOPED_VAR;
        return t;
    }

    /* Numeric literal */
    if (isdigit((unsigned char)c))
    {
        int i = 0;
        while (isdigit((unsigned char)g_src[g_pos]) || g_src[g_pos] == '.')
            t.text[i++] = g_src[g_pos++];
        t.text[i] = '\0';
        t.num_val = atof(t.text);
        t.kind = TOK_NUMBER;
        return t;
    }

    /* Identifiers and keywords */
    if (isalpha((unsigned char)c) || c == '_')
    {
        int i = 0;
        while (isalnum((unsigned char)g_src[g_pos]) || g_src[g_pos] == '_')
            t.text[i++] = g_src[g_pos++];
        t.text[i] = '\0';

        if (!strcmp(t.text, "num"))
            t.kind = TOK_KW_NUM;
        else if (!strcmp(t.text, "txt"))
            t.kind = TOK_KW_TXT;
        else if (!strcmp(t.text, "scoped"))
            t.kind = TOK_KW_SCOPED;
        else if (!strcmp(t.text, "print"))
            t.kind = TOK_KW_PRINT;
        else if (!strcmp(t.text, "loop"))
            t.kind = TOK_KW_LOOP;
        else if (!strcmp(t.text, "jump"))
            t.kind = TOK_KW_JUMP;
        else if (!strcmp(t.text, "if"))
            t.kind = TOK_KW_IF;
        else if (!strcmp(t.text, "else"))
            t.kind = TOK_KW_ELSE;
        else if (!strcmp(t.text, "random"))
            t.kind = TOK_KW_RANDOM;
        else
            t.kind = TOK_IDENT;
        return t;
    }

    /* Single and double-character tokens */
    g_pos++;
    t.text[0] = c;
    t.text[1] = '\0';

    char next = g_src[g_pos];

    if (c == '=' && next == '=')
    {
        g_pos++;
        strcpy(t.text, "==");
        t.kind = TOK_EQEQ;
        return t;
    }
    if (c == '!' && next == '=')
    {
        g_pos++;
        strcpy(t.text, "!=");
        t.kind = TOK_NEQ;
        return t;
    }
    if (c == '<' && next == '=')
    {
        g_pos++;
        strcpy(t.text, "<=");
        t.kind = TOK_LTE;
        return t;
    }
    if (c == '>' && next == '=')
    {
        g_pos++;
        strcpy(t.text, ">=");
        t.kind = TOK_GTE;
        return t;
    }
    if (c == '+' && next == '=')
    {
        g_pos++;
        strcpy(t.text, "+=");
        t.kind = TOK_PLUS_EQ;
        return t;
    }
    if (c == '-' && next == '=')
    {
        g_pos++;
        strcpy(t.text, "-=");
        t.kind = TOK_MINUS_EQ;
        return t;
    }

    switch (c)
    {
    case '=':
        t.kind = TOK_EQUALS;
        break;
    case '!':
        t.kind = TOK_BANG;
        break;
    case '<':
        t.kind = TOK_LT;
        break;
    case '>':
        t.kind = TOK_GT;
        break;
    case '+':
        t.kind = TOK_PLUS;
        break;
    case '-':
        t.kind = TOK_MINUS;
        break;
    case '*':
        t.kind = TOK_STAR;
        break;
    case '/':
        t.kind = TOK_SLASH;
        break;
    case '(':
        t.kind = TOK_LPAREN;
        break;
    case ')':
        t.kind = TOK_RPAREN;
        break;
    case '{':
        t.kind = TOK_LBRACE;
        break;
    case '}':
        t.kind = TOK_RBRACE;
        break;
    case ',':
        t.kind = TOK_COMMA;
        break;
    default:
        fprintf(stderr, "Line %d: unknown character '%c'\n", g_line, c);
        exit(1);
    }
    return t;
}

static void advance(void) { g_cur = lex_next(); }
static int check(TokenKind k) { return g_cur.kind == k; }

static void expect(TokenKind k, const char *what)
{
    if (!check(k))
    {
        fprintf(stderr, "Line %d: expected %s but got '%s'\n",
                g_cur.line, what, g_cur.text[0] ? g_cur.text : "<EOF>");
        exit(1);
    }
    advance();
}

/* =========================================================
 *  AST NODES
 * ========================================================= */

typedef enum
{
    EXPR_NUMBER,
    EXPR_IDENT,
    EXPR_SCOPED_IDENT,
    EXPR_STRING,
    EXPR_BINOP,
    EXPR_UNARY_NEG,
    EXPR_CMP,    /* ==  !=  <  >  <=  >= — result is 1 or 0 */
    EXPR_RANDOM, /* random(min, max)                         */
} ExprKind;

typedef struct Expr
{
    ExprKind kind;
    char sval[512];
    double nval;
    char op;
    char op_str[3];
    struct Expr *left, *right;
    struct Expr *operand;
    /* EXPR_RANDOM reuses left=min, right=max */
    int line;
} Expr;

typedef enum
{
    STMT_VAR_DECL,
    STMT_TXT_DECL,
    STMT_AUGMENTED_ASSIGN,
    STMT_PRINT,
    STMT_LOOP,
    STMT_JUMP,
    STMT_IF,
} StmtKind;

typedef struct Stmt Stmt;
struct Stmt
{
    StmtKind kind;
    int line;

    int is_scoped;
    char var_name[256];
    Expr *var_value;

    Expr *print_val;

    Expr *loop_count;
    Stmt **loop_body;
    int loop_body_len;

    char jump_target[256];
    Expr **jump_args;
    int jump_arg_len;

    Expr *if_cond;
    Stmt **if_body;
    int if_body_len;
    Stmt **else_body;
    int else_body_len;

    char aug_op;
};

typedef struct
{
    char name[256];
    char params[32][256];
    int param_len;
    Stmt **body;
    int body_len;
    int line;
} FuncDef;

typedef struct
{
    FuncDef **funcs;
    int func_len;
} Program;

static Expr *new_expr(ExprKind k, int line)
{
    Expr *e = calloc(1, sizeof(Expr));
    e->kind = k;
    e->line = line;
    return e;
}
static Stmt *new_stmt(StmtKind k, int line)
{
    Stmt *s = calloc(1, sizeof(Stmt));
    s->kind = k;
    s->line = line;
    return s;
}

/* =========================================================
 *  PARSER
 * ========================================================= */

static Expr *parse_expr(void);

static Expr *parse_primary(void)
{
    int line = g_cur.line;

    if (check(TOK_MINUS))
    {
        advance();
        Expr *e = new_expr(EXPR_UNARY_NEG, line);
        e->operand = parse_primary();
        return e;
    }

    if (check(TOK_NUMBER))
    {
        Expr *e = new_expr(EXPR_NUMBER, line);
        e->nval = g_cur.num_val;
        advance();
        return e;
    }

    if (check(TOK_IDENT))
    {
        Expr *e = new_expr(EXPR_IDENT, line);
        strcpy(e->sval, g_cur.text);
        advance();
        return e;
    }

    if (check(TOK_SCOPED_VAR))
    {
        Expr *e = new_expr(EXPR_SCOPED_IDENT, line);
        strcpy(e->sval, g_cur.text);
        advance();
        return e;
    }

    if (check(TOK_STRING))
    {
        Expr *e = new_expr(EXPR_STRING, line);
        strcpy(e->sval, g_cur.text);
        advance();
        return e;
    }

    /* random(min, max) */
    if (check(TOK_KW_RANDOM))
    {
        advance();
        Expr *e = new_expr(EXPR_RANDOM, line);
        expect(TOK_LPAREN, "'('");
        e->left = parse_expr(); /* min */
        expect(TOK_COMMA, "','");
        e->right = parse_expr(); /* max */
        expect(TOK_RPAREN, "')'");
        return e;
    }

    if (check(TOK_LPAREN))
    {
        advance();
        Expr *e = parse_expr();
        expect(TOK_RPAREN, "')'");
        return e;
    }

    fprintf(stderr, "Line %d: unexpected token '%s' in expression\n",
            line, g_cur.text[0] ? g_cur.text : "<EOF>");
    exit(1);
}

static Expr *parse_term(void)
{
    Expr *left = parse_primary();
    while (check(TOK_STAR) || check(TOK_SLASH))
    {
        char op = check(TOK_STAR) ? '*' : '/';
        int line = g_cur.line;
        advance();
        Expr *right = parse_primary();
        Expr *bin = new_expr(EXPR_BINOP, line);
        bin->op = op;
        bin->left = left;
        bin->right = right;
        left = bin;
    }
    return left;
}

static Expr *parse_addsub(void)
{
    Expr *left = parse_term();
    while (check(TOK_PLUS) || check(TOK_MINUS))
    {
        char op = check(TOK_PLUS) ? '+' : '-';
        int line = g_cur.line;
        advance();
        Expr *right = parse_term();
        Expr *bin = new_expr(EXPR_BINOP, line);
        bin->op = op;
        bin->left = left;
        bin->right = right;
        left = bin;
    }
    return left;
}

static Expr *parse_expr(void)
{
    Expr *left = parse_addsub();
    while (check(TOK_EQEQ) || check(TOK_NEQ) ||
           check(TOK_LT) || check(TOK_GT) ||
           check(TOK_LTE) || check(TOK_GTE))
    {
        int line = g_cur.line;
        const char *op;
        switch (g_cur.kind)
        {
        case TOK_EQEQ:
            op = "==";
            break;
        case TOK_NEQ:
            op = "!=";
            break;
        case TOK_LT:
            op = "<";
            break;
        case TOK_GT:
            op = ">";
            break;
        case TOK_LTE:
            op = "<=";
            break;
        case TOK_GTE:
            op = ">=";
            break;
        default:
            op = "?";
            break;
        }
        advance();
        Expr *right = parse_addsub();
        Expr *cmp = new_expr(EXPR_CMP, line);
        strncpy(cmp->op_str, op, sizeof(cmp->op_str) - 1);
        cmp->left = left;
        cmp->right = right;
        left = cmp;
    }
    return left;
}

static Stmt **parse_body(int *out_len);

static Stmt *parse_statement(void)
{
    int line = g_cur.line;

    /* num var = expr */
    if (check(TOK_KW_NUM))
    {
        advance();
        Stmt *s = new_stmt(STMT_VAR_DECL, line);
        s->is_scoped = 0;
        if (!check(TOK_IDENT))
        {
            fprintf(stderr, "Line %d: expected variable name after 'num'\n", line);
            exit(1);
        }
        strcpy(s->var_name, g_cur.text);
        advance();
        expect(TOK_EQUALS, "'='");
        s->var_value = parse_expr();
        return s;
    }

    /* txt var = "string" */
    if (check(TOK_KW_TXT))
    {
        advance();
        Stmt *s = new_stmt(STMT_TXT_DECL, line);
        s->is_scoped = 0;
        if (!check(TOK_IDENT))
        {
            fprintf(stderr, "Line %d: expected variable name after 'txt'\n", line);
            exit(1);
        }
        strcpy(s->var_name, g_cur.text);
        advance();
        expect(TOK_EQUALS, "'='");
        if (!check(TOK_STRING))
        {
            fprintf(stderr, "Line %d: txt variable must be assigned a string literal\n", line);
            exit(1);
        }
        s->var_value = parse_expr();
        return s;
    }

    /* scoped num var = expr  |  scoped txt var = "string" */
    if (check(TOK_KW_SCOPED))
    {
        advance();
        if (check(TOK_KW_TXT))
        {
            advance();
            Stmt *s = new_stmt(STMT_TXT_DECL, line);
            s->is_scoped = 1;
            if (!check(TOK_IDENT))
            {
                fprintf(stderr, "Line %d: expected variable name after 'scoped txt'\n", line);
                exit(1);
            }
            strcpy(s->var_name, g_cur.text);
            advance();
            expect(TOK_EQUALS, "'='");
            if (!check(TOK_STRING))
            {
                fprintf(stderr, "Line %d: txt variable must be assigned a string literal\n", line);
                exit(1);
            }
            s->var_value = parse_expr();
            return s;
        }
        expect(TOK_KW_NUM, "'num' or 'txt'");
        Stmt *s = new_stmt(STMT_VAR_DECL, line);
        s->is_scoped = 1;
        if (!check(TOK_IDENT))
        {
            fprintf(stderr, "Line %d: expected variable name after 'scoped num'\n", line);
            exit(1);
        }
        strcpy(s->var_name, g_cur.text);
        advance();
        expect(TOK_EQUALS, "'='");
        s->var_value = parse_expr();
        return s;
    }

    /* name += expr  |  name -= expr */
    if (check(TOK_IDENT) &&
        (g_src[g_pos] == '+' || g_src[g_pos] == '-'))
    {
        char saved_name[256];
        strcpy(saved_name, g_cur.text);
        advance();
        if (check(TOK_PLUS_EQ) || check(TOK_MINUS_EQ))
        {
            Stmt *s = new_stmt(STMT_AUGMENTED_ASSIGN, line);
            s->is_scoped = 0;
            strcpy(s->var_name, saved_name);
            s->aug_op = check(TOK_PLUS_EQ) ? '+' : '-';
            advance();
            s->var_value = parse_expr();
            return s;
        }
        fprintf(stderr, "Line %d: unexpected token '%s' after identifier '%s'\n",
                line, g_cur.text[0] ? g_cur.text : "<EOF>", saved_name);
        exit(1);
    }

    /* $name += expr  |  $name -= expr */
    if (check(TOK_SCOPED_VAR))
    {
        char saved_name[256];
        strcpy(saved_name, g_cur.text);
        advance();
        if (check(TOK_PLUS_EQ) || check(TOK_MINUS_EQ))
        {
            Stmt *s = new_stmt(STMT_AUGMENTED_ASSIGN, line);
            s->is_scoped = 1;
            strcpy(s->var_name, saved_name);
            s->aug_op = check(TOK_PLUS_EQ) ? '+' : '-';
            advance();
            s->var_value = parse_expr();
            return s;
        }
        fprintf(stderr, "Line %d: unexpected token '%s' after '$%s'\n",
                line, g_cur.text[0] ? g_cur.text : "<EOF>", saved_name);
        exit(1);
    }

    /* print(...) */
    if (check(TOK_KW_PRINT))
    {
        advance();
        Stmt *s = new_stmt(STMT_PRINT, line);
        expect(TOK_LPAREN, "'('");
        s->print_val = parse_expr();
        expect(TOK_RPAREN, "')'");
        return s;
    }

    /* loop <expr> { ... } */
    if (check(TOK_KW_LOOP))
    {
        advance();
        Stmt *s = new_stmt(STMT_LOOP, line);
        s->loop_count = parse_expr();
        expect(TOK_LBRACE, "'{'");
        s->loop_body = parse_body(&s->loop_body_len);
        expect(TOK_RBRACE, "'}'");
        return s;
    }

    /* if <expr> { ... } [else { ... }]
     * Supports string comparisons: if (x == "hello") { ... } */
    if (check(TOK_KW_IF))
    {
        advance();
        Stmt *s = new_stmt(STMT_IF, line);
        s->if_cond = parse_expr();
        expect(TOK_LBRACE, "'{'");
        s->if_body = parse_body(&s->if_body_len);
        expect(TOK_RBRACE, "'}'");
        if (check(TOK_KW_ELSE))
        {
            advance();
            expect(TOK_LBRACE, "'{'");
            s->else_body = parse_body(&s->else_body_len);
            expect(TOK_RBRACE, "'}'");
        }
        else
        {
            s->else_body = NULL;
            s->else_body_len = 0;
        }
        return s;
    }

    /* jump _func(args) */
    if (check(TOK_KW_JUMP))
    {
        advance();
        Stmt *s = new_stmt(STMT_JUMP, line);
        if (!check(TOK_IDENT) || g_cur.text[0] != '_')
        {
            fprintf(stderr, "Line %d: expected function name after 'jump'\n", line);
            exit(1);
        }
        strcpy(s->jump_target, g_cur.text);
        advance();
        expect(TOK_LPAREN, "'('");
        s->jump_args = NULL;
        s->jump_arg_len = 0;
        while (!check(TOK_RPAREN) && !check(TOK_EOF))
        {
            Expr *arg = parse_expr();
            s->jump_args = realloc(s->jump_args,
                                   sizeof(Expr *) * (s->jump_arg_len + 1));
            s->jump_args[s->jump_arg_len++] = arg;
            if (check(TOK_COMMA))
                advance();
        }
        expect(TOK_RPAREN, "')'");
        return s;
    }

    fprintf(stderr, "Line %d: unexpected token '%s' in statement\n",
            line, g_cur.text[0] ? g_cur.text : "<EOF>");
    exit(1);
}

static Stmt **parse_body(int *out_len)
{
    Stmt **stmts = NULL;
    *out_len = 0;
    while (!check(TOK_RBRACE) && !check(TOK_EOF))
    {
        Stmt *s = parse_statement();
        stmts = realloc(stmts, sizeof(Stmt *) * (*out_len + 1));
        stmts[(*out_len)++] = s;
    }
    return stmts;
}

static FuncDef *parse_func_def(void)
{
    FuncDef *fn = calloc(1, sizeof(FuncDef));
    fn->line = g_cur.line;

    if (!check(TOK_IDENT) || g_cur.text[0] != '_')
    {
        fprintf(stderr, "Line %d: function name must start with '_'\n", g_cur.line);
        exit(1);
    }
    strcpy(fn->name, g_cur.text);
    advance();

    expect(TOK_LPAREN, "'('");
    fn->param_len = 0;
    while (!check(TOK_RPAREN) && !check(TOK_EOF))
    {
        if (!check(TOK_IDENT))
        {
            fprintf(stderr, "Line %d: expected parameter name\n", g_cur.line);
            exit(1);
        }
        strcpy(fn->params[fn->param_len++], g_cur.text);
        advance();
        if (check(TOK_COMMA))
            advance();
    }
    expect(TOK_RPAREN, "')'");
    expect(TOK_LBRACE, "'{'");
    fn->body = parse_body(&fn->body_len);
    expect(TOK_RBRACE, "'}'");
    return fn;
}

static Program *parse_program(void)
{
    Program *prog = calloc(1, sizeof(Program));
    advance();

    while (!check(TOK_EOF))
    {
        if (check(TOK_IDENT) && g_cur.text[0] == '_')
        {
            FuncDef *fn = parse_func_def();
            prog->funcs = realloc(prog->funcs,
                                  sizeof(FuncDef *) * (prog->func_len + 1));
            prog->funcs[prog->func_len++] = fn;
        }
        else
        {
            fprintf(stderr, "Line %d: only function definitions allowed at top level "
                            "(got '%s')\n",
                    g_cur.line, g_cur.text);
            exit(1);
        }
    }
    return prog;
}

/* =========================================================
 *  JSON EMITTER
 * ========================================================= */

static FILE *g_out;

static void ind(int d)
{
    for (int i = 0; i < d * 2; i++)
        fputc(' ', g_out);
}

static void emit_str(const char *s)
{
    fputc('"', g_out);
    for (; *s; s++)
    {
        if (*s == '"')
            fputs("\\\"", g_out);
        else if (*s == '\\')
            fputs("\\\\", g_out);
        else if (*s == '\n')
            fputs("\\n", g_out);
        else if (*s == '\t')
            fputs("\\t", g_out);
        else
            fputc(*s, g_out);
    }
    fputc('"', g_out);
}

static void emit_expr(const Expr *e, int d)
{
    fprintf(g_out, "{\n");
    switch (e->kind)
    {

    case EXPR_NUMBER:
        ind(d + 1);
        fprintf(g_out, "\"type\": \"Number\",\n");
        ind(d + 1);
        fprintf(g_out, "\"value\": %g,\n", e->nval);
        ind(d + 1);
        fprintf(g_out, "\"line\": %d\n", e->line);
        break;

    case EXPR_IDENT:
        ind(d + 1);
        fprintf(g_out, "\"type\": \"Identifier\",\n");
        ind(d + 1);
        fprintf(g_out, "\"name\": ");
        emit_str(e->sval);
        fputc('\n', g_out);
        ind(d + 1);
        fprintf(g_out, ",\"scoped\": false,\n");
        ind(d + 1);
        fprintf(g_out, "\"line\": %d\n", e->line);
        break;

    case EXPR_SCOPED_IDENT:
        ind(d + 1);
        fprintf(g_out, "\"type\": \"Identifier\",\n");
        ind(d + 1);
        fprintf(g_out, "\"name\": ");
        emit_str(e->sval);
        fprintf(g_out, ",\n");
        ind(d + 1);
        fprintf(g_out, "\"scoped\": true,\n");
        ind(d + 1);
        fprintf(g_out, "\"line\": %d\n", e->line);
        break;

    case EXPR_STRING:
        ind(d + 1);
        fprintf(g_out, "\"type\": \"String\",\n");
        ind(d + 1);
        fprintf(g_out, "\"value\": ");
        emit_str(e->sval);
        fprintf(g_out, ",\n");
        ind(d + 1);
        fprintf(g_out, "\"line\": %d\n", e->line);
        break;

    case EXPR_BINOP:
    {
        char op_str[3] = {e->op, '\0', '\0'};
        ind(d + 1);
        fprintf(g_out, "\"type\": \"BinaryOp\",\n");
        ind(d + 1);
        fprintf(g_out, "\"op\": \"%s\",\n", op_str);
        ind(d + 1);
        fprintf(g_out, "\"left\": ");
        emit_expr(e->left, d + 1);
        fprintf(g_out, ",\n");
        ind(d + 1);
        fprintf(g_out, "\"right\": ");
        emit_expr(e->right, d + 1);
        fprintf(g_out, ",\n");
        ind(d + 1);
        fprintf(g_out, "\"line\": %d\n", e->line);
        break;
    }

    case EXPR_CMP:
        ind(d + 1);
        fprintf(g_out, "\"type\": \"Cmp\",\n");
        ind(d + 1);
        fprintf(g_out, "\"op\": \"%s\",\n", e->op_str);
        ind(d + 1);
        fprintf(g_out, "\"left\": ");
        emit_expr(e->left, d + 1);
        fprintf(g_out, ",\n");
        ind(d + 1);
        fprintf(g_out, "\"right\": ");
        emit_expr(e->right, d + 1);
        fprintf(g_out, ",\n");
        ind(d + 1);
        fprintf(g_out, "\"line\": %d\n", e->line);
        break;

    case EXPR_UNARY_NEG:
        ind(d + 1);
        fprintf(g_out, "\"type\": \"UnaryNeg\",\n");
        ind(d + 1);
        fprintf(g_out, "\"operand\": ");
        emit_expr(e->operand, d + 1);
        fprintf(g_out, ",\n");
        ind(d + 1);
        fprintf(g_out, "\"line\": %d\n", e->line);
        break;

    case EXPR_RANDOM:
        ind(d + 1);
        fprintf(g_out, "\"type\": \"Random\",\n");
        ind(d + 1);
        fprintf(g_out, "\"min\": ");
        emit_expr(e->left, d + 1);
        fprintf(g_out, ",\n");
        ind(d + 1);
        fprintf(g_out, "\"max\": ");
        emit_expr(e->right, d + 1);
        fprintf(g_out, ",\n");
        ind(d + 1);
        fprintf(g_out, "\"line\": %d\n", e->line);
        break;
    }
    ind(d);
    fputc('}', g_out);
}

static void emit_stmt(const Stmt *s, int d);

static void emit_body(Stmt **body, int len, int d)
{
    fprintf(g_out, "[\n");
    for (int i = 0; i < len; i++)
    {
        ind(d + 1);
        emit_stmt(body[i], d + 1);
        fprintf(g_out, "%s\n", i < len - 1 ? "," : "");
    }
    ind(d);
    fputc(']', g_out);
}

static void emit_stmt(const Stmt *s, int d)
{
    fprintf(g_out, "{\n");
    switch (s->kind)
    {

    case STMT_VAR_DECL:
        ind(d + 1);
        fprintf(g_out, "\"type\": \"VarDecl\",\n");
        ind(d + 1);
        fprintf(g_out, "\"scoped\": %s,\n", s->is_scoped ? "true" : "false");
        ind(d + 1);
        fprintf(g_out, "\"name\": ");
        emit_str(s->var_name);
        fprintf(g_out, ",\n");
        ind(d + 1);
        fprintf(g_out, "\"value\": ");
        emit_expr(s->var_value, d + 1);
        fprintf(g_out, ",\n");
        ind(d + 1);
        fprintf(g_out, "\"line\": %d\n", s->line);
        break;

    case STMT_TXT_DECL:
        ind(d + 1);
        fprintf(g_out, "\"type\": \"TxtDecl\",\n");
        ind(d + 1);
        fprintf(g_out, "\"scoped\": %s,\n", s->is_scoped ? "true" : "false");
        ind(d + 1);
        fprintf(g_out, "\"name\": ");
        emit_str(s->var_name);
        fprintf(g_out, ",\n");
        ind(d + 1);
        fprintf(g_out, "\"value\": ");
        emit_expr(s->var_value, d + 1);
        fprintf(g_out, ",\n");
        ind(d + 1);
        fprintf(g_out, "\"line\": %d\n", s->line);
        break;

    case STMT_AUGMENTED_ASSIGN:
    {
        char op_str[3] = {s->aug_op, '=', '\0'};
        ind(d + 1);
        fprintf(g_out, "\"type\": \"AugAssign\",\n");
        ind(d + 1);
        fprintf(g_out, "\"scoped\": %s,\n", s->is_scoped ? "true" : "false");
        ind(d + 1);
        fprintf(g_out, "\"name\": ");
        emit_str(s->var_name);
        fprintf(g_out, ",\n");
        ind(d + 1);
        fprintf(g_out, "\"op\": \"%s\",\n", op_str);
        ind(d + 1);
        fprintf(g_out, "\"value\": ");
        emit_expr(s->var_value, d + 1);
        fprintf(g_out, ",\n");
        ind(d + 1);
        fprintf(g_out, "\"line\": %d\n", s->line);
        break;
    }

    case STMT_PRINT:
        ind(d + 1);
        fprintf(g_out, "\"type\": \"Print\",\n");
        ind(d + 1);
        fprintf(g_out, "\"value\": ");
        emit_expr(s->print_val, d + 1);
        fprintf(g_out, ",\n");
        ind(d + 1);
        fprintf(g_out, "\"line\": %d\n", s->line);
        break;

    case STMT_LOOP:
        ind(d + 1);
        fprintf(g_out, "\"type\": \"Loop\",\n");
        ind(d + 1);
        fprintf(g_out, "\"count\": ");
        emit_expr(s->loop_count, d + 1);
        fprintf(g_out, ",\n");
        ind(d + 1);
        fprintf(g_out, "\"body\": ");
        emit_body(s->loop_body, s->loop_body_len, d + 1);
        fprintf(g_out, ",\n");
        ind(d + 1);
        fprintf(g_out, "\"line\": %d\n", s->line);
        break;

    case STMT_IF:
        ind(d + 1);
        fprintf(g_out, "\"type\": \"If\",\n");
        ind(d + 1);
        fprintf(g_out, "\"condition\": ");
        emit_expr(s->if_cond, d + 1);
        fprintf(g_out, ",\n");
        ind(d + 1);
        fprintf(g_out, "\"body\": ");
        emit_body(s->if_body, s->if_body_len, d + 1);
        fprintf(g_out, ",\n");
        ind(d + 1);
        fprintf(g_out, "\"else_body\": ");
        if (s->else_body != NULL)
            emit_body(s->else_body, s->else_body_len, d + 1);
        else
            fprintf(g_out, "null");
        fprintf(g_out, ",\n");
        ind(d + 1);
        fprintf(g_out, "\"line\": %d\n", s->line);
        break;

    case STMT_JUMP:
        ind(d + 1);
        fprintf(g_out, "\"type\": \"Jump\",\n");
        ind(d + 1);
        fprintf(g_out, "\"target\": ");
        emit_str(s->jump_target);
        fprintf(g_out, ",\n");
        ind(d + 1);
        fprintf(g_out, "\"args\": [\n");
        for (int i = 0; i < s->jump_arg_len; i++)
        {
            ind(d + 2);
            emit_expr(s->jump_args[i], d + 2);
            fprintf(g_out, "%s\n", i < s->jump_arg_len - 1 ? "," : "");
        }
        ind(d + 1);
        fprintf(g_out, "],\n");
        ind(d + 1);
        fprintf(g_out, "\"line\": %d\n", s->line);
        break;
    }
    ind(d);
    fputc('}', g_out);
}

static void emit_func(const FuncDef *fn, int d)
{
    fprintf(g_out, "{\n");
    ind(d + 1);
    fprintf(g_out, "\"type\": \"FunctionDef\",\n");
    ind(d + 1);
    fprintf(g_out, "\"name\": ");
    emit_str(fn->name);
    fprintf(g_out, ",\n");
    ind(d + 1);
    fprintf(g_out, "\"params\": [");
    for (int i = 0; i < fn->param_len; i++)
    {
        emit_str(fn->params[i]);
        if (i < fn->param_len - 1)
            fprintf(g_out, ", ");
    }
    fprintf(g_out, "],\n");
    ind(d + 1);
    fprintf(g_out, "\"body\": ");
    emit_body(fn->body, fn->body_len, d + 1);
    fprintf(g_out, ",\n");
    ind(d + 1);
    fprintf(g_out, "\"line\": %d\n", fn->line);
    ind(d);
    fputc('}', g_out);
}

static void emit_program(const Program *prog)
{
    fprintf(g_out, "{\n");
    ind(1);
    fprintf(g_out, "\"type\": \"Program\",\n");
    ind(1);
    fprintf(g_out, "\"functions\": [\n");
    for (int i = 0; i < prog->func_len; i++)
    {
        ind(2);
        emit_func(prog->funcs[i], 2);
        fprintf(g_out, "%s\n", i < prog->func_len - 1 ? "," : "");
    }
    ind(1);
    fprintf(g_out, "]\n");
    fprintf(g_out, "}\n");
}

/* =========================================================
 *  ENTRY POINT
 * ========================================================= */

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <source_file> [output_file]\n", argv[0]);
        return 1;
    }

    FILE *fin = fopen(argv[1], "r");
    if (!fin)
    {
        perror(argv[1]);
        return 1;
    }

    fseek(fin, 0, SEEK_END);
    long sz = ftell(fin);
    fseek(fin, 0, SEEK_SET);

    char *src_buf = malloc((size_t)sz + 1);
    if (!src_buf)
    {
        fputs("Out of memory\n", stderr);
        fclose(fin);
        return 1;
    }

    fread(src_buf, 1, (size_t)sz, fin);
    src_buf[sz] = '\0';
    fclose(fin);

    g_out = stdout;
    if (argc >= 3)
    {
        g_out = fopen(argv[2], "w");
        if (!g_out)
        {
            perror(argv[2]);
            free(src_buf);
            return 1;
        }
    }

    g_src = src_buf;
    g_pos = 0;
    g_line = 1;

    Program *prog = parse_program();

    int found_main = 0;
    for (int i = 0; i < prog->func_len; i++)
        if (!strcmp(prog->funcs[i]->name, "_main"))
        {
            found_main = 1;
            break;
        }
    if (!found_main)
        fprintf(stderr, "Warning: no _main() entry point found\n");

    emit_program(prog);

    if (g_out != stdout)
        fclose(g_out);
    free(src_buf);
    return 0;
}