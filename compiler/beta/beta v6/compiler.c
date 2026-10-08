#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

// Custom includes
#include "errors.h"

#define MAX_LINE 512
#define MAX_VARS 256
#define FIRST_VAR_REG 1

typedef struct {
    char label_else[64];
    char label_end[64];
    bool has_else;
} IfLabel;

static IfLabel if_stack[256];
static int if_counter = 0;

typedef struct {
    char label_start[64];
    char label_end[64];
    char counter_var[64];
} LoopLabel;

static LoopLabel loop_stack[256];
static int loop_depth = 0;
static int loop_seq = 0;


typedef struct {
    char name[64];
    char label[64];   // e.g. "ram"
} Var;

static Var vars[MAX_VARS];
static int var_count = 0;

typedef struct {
    char label[64];
    char *text;
} StringLiteral;

static StringLiteral str_literals[256];
static int str_count = 0;

// Add a string literal with an auto-generated label
void add_string_literal(const char *text) {
    char label[64];
    snprintf(label, sizeof(label), "str_%d", str_count);
    strncpy(str_literals[str_count].label, label, sizeof(str_literals[str_count].label)-1);
    str_literals[str_count].text = strdup(text);
    str_count++;
}

// Emit all string literals to output file
void emit_all_string_literals(FILE *fout) {
    if (str_count == 0) return;
    fprintf(fout, ".data\n");
    for (int i = 0; i < str_count; i++) {
        fprintf(fout, "%s:\n", str_literals[i].label);
        fprintf(fout, "    .asciz \"%s\"\n", str_literals[i].text);
    }
}

void emit_all_variables(FILE *fout) {
    if (var_count == 0) return;
    fprintf(fout, ".data\n");
    for (int i = 0; i < var_count; i++) {
        fprintf(fout, ".align 2\n");
        fprintf(fout, "%s: .word 0\n", vars[i].label);
    }
}

// Add a string literal with a specified label
void add_string_literal_with_label(const char *text, const char *label) {
    if (str_count >= 256) {
        fprintf(stderr, "Too many string literals\n");
        exit(1);
    }
    strncpy(str_literals[str_count].label, label, sizeof(str_literals[str_count].label)-1);
    str_literals[str_count].label[sizeof(str_literals[str_count].label)-1] = '\0';
    str_literals[str_count].text = strdup(text);
    str_count++;
}


// Trim leading and trailing whitespace (in place), return pointer to trimmed start.
char *trim(char *s) {
    if (!s) return s;
    while (*s && (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')) s++;
    char *end = s + strlen(s) - 1;
    while (end >= s && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) { *end = '\0'; end--; }
    return s;
}

// find first top-level separator ',' or '=' not inside brackets []
char *find_top_level_sep(char *s) {
    int depth = 0;
    for (char *p = s; *p; ++p) {
        if (*p == '[') depth++;
        else if (*p == ']') { if (depth>0) depth--; }
        else if (depth == 0 && (*p == '=' || *p == ',')) return p;
    }
    return NULL;
}

char *get_var_label(const char *name) {
    for (int i = 0; i < var_count; i++)
        if (strcmp(vars[i].name, name) == 0)
            return vars[i].label;
    return NULL;
}

char *assign_var(const char *name) {
    if (!name) return NULL;

    // already exists?
    for (int i = 0; i < var_count; i++) {
        if (strcmp(vars[i].name, name) == 0)
            return vars[i].label;
    }

    if (var_count >= 256) {
        fprintf(stderr, "Too many variables\n");
        exit(1);
    }

    snprintf(vars[var_count].name, sizeof(vars[var_count].name), "%s", name);
    snprintf(vars[var_count].label, sizeof(vars[var_count].label), "%s", name);

    var_count++;
    return vars[var_count - 1].label;
}


bool is_number(const char *s) {
    if (!s || !*s) return false;
    const char *p = s;
    if (*p == '+' || *p == '-') p++;
    bool has = false;
    while (*p) {
        if (!isdigit((unsigned char)*p)) return false;
        has = true;
        p++;
    }
    return has;
}

bool is_register(const char *s) {
    if (!s) return false;
    if ((s[0] == 'w' || s[0] == 'x') && isdigit((unsigned char)s[1])) return true;
    return false;
}

int is_var(const char *s) {
    if (!s || !*s) return 0;
    if (is_number(s)) return 0;
    if (is_register(s)) return 0;
    if (s[0] == '[') return 0;
    return get_var_label(s) != NULL;
}

int main(int argc, char **argv) {
    // reserve integer format string for printf

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input.n> <output.s>\n", argv[0]);
        return 1;
    }

    FILE *fin = fopen(argv[1], "r");
    FILE *fout = fopen(argv[2], "w");
    if (!fin || !fout) { fprintf(stderr, "Could not open files\n"); return 1; }

    add_string_literal("%d"); // this will be used for printing numbers

    fprintf(fout, ".text\n");
    fprintf(fout, ".data\nstr_newline: .asciz \"\\n\"\n.text\n");

    char rawline[MAX_LINE];
    bool text_written = false;
    int line_num = 0;

    while (fgets(rawline, sizeof(rawline), fin)) {
        line_num++;
        // strip newline
        rawline[strcspn(rawline, "\n")] = '\0';
        char *line = trim(rawline);
        if (!line || *line == '\0') continue;



        // FUNCTION DETECTION WITH PARAMETERS
        size_t len = strlen(line);
        if (len >= 3 && line[len-1] == '{' && strchr(line, '(') && strchr(line, ')')) {

            char func[128] = {0};
            char param_str[128] = {0};

            // extract function name + params inside ()
            // ex: "add(c, d)" -> func="add", param_str="c, d"
            sscanf(line, "%127[^ (](%127[^)])", func, param_str);

            trim(func);
            trim(param_str);

            if (!text_written) { 
                fprintf(fout, ".text\n");
                text_written = true;
            }

            fprintf(fout, ".global %s\n%s:\n", func, func);

            // split parameters by comma
            char *p = strtok(param_str, ",");
            int reg = 0;

            while (p) {
                char *name = trim(p);                // variable name
                char *vlabel = assign_var(name);     // get label for variable storage

                // store incoming argument register into that variable
                fprintf(fout, "    // param %s\n", name);
                fprintf(fout, "    adrp x9, %s@PAGE\n", vlabel);
                fprintf(fout, "    str w%d, [x9, %s@PAGEOFF]\n", reg, vlabel);

                reg++;
                p = strtok(NULL, ",");
            }

            continue;
        }

        if (strcmp(line, "}") == 0) {
            if (loop_depth > 0) {
                LoopLabel *L = &loop_stack[loop_depth - 1];

                // decrement counter
                fprintf(fout, "    adrp x9, %s@PAGE\n", L->counter_var);
                fprintf(fout, "    ldr w0, [x9, %s@PAGEOFF]\n", L->counter_var);
                fprintf(fout, "    sub w0, w0, #1\n");
                fprintf(fout, "    str w0, [x9, %s@PAGEOFF]\n", L->counter_var);

                // jump back
                fprintf(fout, "    b %s\n", L->label_start);

                // exit label
                fprintf(fout, "%s:\n", L->label_end);

                loop_depth--;
                continue;
            }
            if (if_counter > 0) {
                IfLabel *curr = &if_stack[if_counter - 1];

                if (curr->has_else) {
                    fprintf(fout, "%s:\n", curr->label_end);
                } else {
                    fprintf(fout, "%s:\n", curr->label_else);
                }

                if_counter--;

            }
            continue;
        }

        // FUNCTION CALL WITH PARAMETERS:  func(a,b,c)
        if (strstr(line, "bl ") && strchr(line,'(') && strchr(line,')')) {

            char fname[64], params[128];
            sscanf(line, "bl %63[^ (](%127[^)])", fname, params);

            trim(fname);
            trim(params);

            // split params and move them into w0,w1,w2...
            char *p = strtok(params,",");
            int reg = 0;

            while (p) {
                char *arg = trim(p);
                char *vlabel = get_var_label(arg);

                // load argument variable into correct register
                fprintf(fout, "    adrp x9, %s@PAGE\n", vlabel);
                fprintf(fout, "    ldr w%d, [x9, %s@PAGEOFF]\n", reg, vlabel);

                reg++;
                p = strtok(NULL,",");
            }

            // finally call function
            fprintf(fout, "    bl %s\n", fname);
            continue;
        }

        if (strncmp(line, "loop ", 5) == 0) {
            char expr[256];

            // strip "loop " and optional "{"
            strcpy(expr, line + 5);
            char *brace = strchr(expr, '{');
            if (brace) *brace = '\0';
            trim(expr);

            LoopLabel *L = &loop_stack[loop_depth++];

            // create unique labels
            snprintf(L->label_start, sizeof(L->label_start), "_loop_%d", loop_seq);
            snprintf(L->label_end, sizeof(L->label_end), "_loop_end_%d", loop_seq);

            // create hidden counter variable
            snprintf(L->counter_var, sizeof(L->counter_var), "_loop_counter_%d", loop_seq);
            assign_var(L->counter_var);

            loop_seq++;

            /* ---- evaluate expression into w0 ---- */

            char a[128], b[128];
            char op;

            if (sscanf(expr, "%127s %c %127s", a, &op, b) == 3) {
                const char *asmop = NULL;
                if (op == '+') asmop = "add";
                else if (op == '-') asmop = "sub";
                else if (op == '*') asmop = "mul";
                else if (op == '/') asmop = "sdiv";
                else error_syntax(line_num, "Invalid operator in loop");

                // load a → w0
                if (is_number(a)) {
                    fprintf(fout, "    mov w0, #%s\n", a);
                } else {
                    char *al = get_var_label(a);
                    if (!al) error_undef(line_num, a);
                    fprintf(fout, "    adrp x9, %s@PAGE\n", al);
                    fprintf(fout, "    ldr w0, [x9, %s@PAGEOFF]\n", al);
                }

                // load b → w1
                if (is_number(b)) {
                    fprintf(fout, "    mov w1, #%s\n", b);
                } else {
                    char *bl = get_var_label(b);
                    if (!bl) error_undef(line_num, b);
                    fprintf(fout, "    adrp x9, %s@PAGE\n", bl);
                    fprintf(fout, "    ldr w1, [x9, %s@PAGEOFF]\n", bl);
                }

                fprintf(fout, "    %s w0, w0, w1\n", asmop);
            } else {
                // simple value
                if (is_number(expr)) {
                    fprintf(fout, "    mov w0, #%s\n", expr);
                } else {
                    char *el = get_var_label(expr);
                    if (!el) error_undef(line_num, expr);
                    fprintf(fout, "    adrp x9, %s@PAGE\n", el);
                    fprintf(fout, "    ldr w0, [x9, %s@PAGEOFF]\n", el);
                }
            }

            // store initial counter
            fprintf(fout, "    adrp x9, %s@PAGE\n", L->counter_var);
            fprintf(fout, "    str w0, [x9, %s@PAGEOFF]\n", L->counter_var);

            // loop start label
            fprintf(fout, "%s:\n", L->label_start);

            // if counter == 0 → exit loop
            fprintf(fout, "    adrp x9, %s@PAGE\n", L->counter_var);
            fprintf(fout, "    ldr w0, [x9, %s@PAGEOFF]\n", L->counter_var);
            fprintf(fout, "    cbz w0, %s\n", L->label_end);

            continue;
        }

        if (strncmp(line, "exit(", 5) == 0 && line[strlen(line)-1] == ')') {
            fprintf(fout,
                    "   ldr x16, =0x2000001\n"
                    "   mov x0, 0\n"
                    "   svc 0\n"
            );
            continue;
        }

        // ---- print(...) statement
        if (strncmp(line, "print(", 6) == 0 && line[strlen(line)-1] == ')') {
            char buf[MAX_LINE];
            size_t len = strlen(line);
            if (len < 7) {
                error_syntax(line_num, "too few arguments in print call");
                continue;
            }
            strncpy(buf, line + 6, len - 7); // extract inside parentheses
            buf[len - 7] = '\0';
            char *arg = trim(buf);
            char *comma = strchr(arg, ',');
            if (comma) error_syntax(line_num, "Too many arguments in print()");

            const char *label = NULL;
            size_t print_len = 0;

            // string literal
            if (arg[0] == '"' && arg[strlen(arg)-1] == '"') {
                arg[strlen(arg)-1] = '\0';
                char *strval = arg + 1;

                if (strcmp(strval, "\n") == 0) {
                    label = "str_newline";
                    print_len = 1;
                } else {
                    // generate a unique label
                    static int str_counter = 0;
                    char tmp_label[64];
                    snprintf(tmp_label, sizeof(tmp_label), "str_%d", str_count);
                    label = tmp_label;

                    // store in str_literals array; will emit later at top
                    add_string_literal_with_label(strval, label);
                    print_len = strlen(strval);
                }

                // emit write syscall
                fprintf(fout, "    // print string literal\n");
                fprintf(fout, "    ldr x16, =0x2000004\n");
                fprintf(fout, "    mov x0, 1\n");
                fprintf(fout, "    adrp x1, %s@PAGE\n", label);
                fprintf(fout, "    add x1, x1, %s@PAGEOFF\n", label);
                fprintf(fout, "    mov x2, %zu\n", print_len);
                fprintf(fout, "    svc 0\n");

            } else { 
                // numeric variable
                char *vlabel = get_var_label(arg);
                if (!vlabel) error_undef(line_num, arg);

                fprintf(fout, "    // print variable %s (convert to string)\n", arg);

                // load variable into w0
                fprintf(fout, "    adrp x9, %s@PAGE\n", vlabel);
                fprintf(fout, "    ldr w0, [x9, %s@PAGEOFF]\n", vlabel);

                // stack buffer
                fprintf(fout, "    sub sp, sp, #32\n");
                fprintf(fout, "    mov x1, sp\n");
                fprintf(fout, "    mov w2, #0\n");
                fprintf(fout, "    mov w4, #10\n");

                // convert number -> ASCII (reverse)
                fprintf(fout,
                    "1: udiv w3, w0, w4\n"
                    "   msub w5, w3, w4, w0\n"
                    "   add w5, w5, #'0'\n"
                    "   strb w5, [x1, w2, uxtw]\n"
                    "   add w2, w2, #1\n"
                    "   mov w0, w3\n"
                    "   cbnz w0, 1b\n"
                );

                // reverse buffer
                fprintf(fout,
                    "   mov w6, #0\n"
                    "   mov w7, w2\n"
                    "   sub w7, w7, #1\n"
                    "3: ldrb w8, [x1, w6, uxtw]\n"
                    "   ldrb w9, [x1, w7, uxtw]\n"
                    "   strb w8, [x1, w7, uxtw]\n"
                    "   strb w9, [x1, w6, uxtw]\n"
                    "   add w6, w6, #1\n"
                    "   sub w7, w7, #1\n"
                    "   cmp w6, w7\n"
                    "   blt 3b\n"
                );

                // write syscall
                fprintf(fout,
                    "   ldr x16, =0x2000004\n"
                    "   mov x0, #1\n"
                    "   svc 0\n"
                );

                // restore stack
                fprintf(fout, "    add sp, sp, #32\n");
            }

            continue;
        }

        if (strncmp(line, "if ", 3) == 0) {
            char *brace = strchr(line, '{');
            if (brace) *brace = '\0';
            trim(line);
            char cond[128];
            sscanf(line + 3, "%127[^{]", cond); // get everything up to '{'
            trim(cond);

            char val1[64], val2[64], op[3];
            if (sscanf(cond, "%63s %2s %63s", val1, op, val2) != 3) {
                error_syntax(line_num, "Malformed if condition");
            }

            // load val1 -> w0
            if (is_number(val1)) {
                fprintf(fout, "    mov w0, #%s\n", val1);
            } else {
                char *l = get_var_label(val1);
                if (!l) error_undef(line_num, val1);
                fprintf(fout, "    adrp x9, %s@PAGE\n", l);
                fprintf(fout, "    ldr w0, [x9, %s@PAGEOFF]\n", l);
            }

            // load val2 -> w1
            if (is_number(val2)) {
                fprintf(fout, "    mov w1, #%s\n", val2);
            } else {
                char *l = get_var_label(val2);
                if (!l) error_undef(line_num, val2);
                fprintf(fout, "    adrp x9, %s@PAGE\n", l);
                fprintf(fout, "    ldr w1, [x9, %s@PAGEOFF]\n", l);
            }

            fprintf(fout, "    cmp w0, w1\n");

            // generate unique labels
            int curr_if = if_counter;
            static int if_label_seq = 0;

            snprintf(if_stack[curr_if].label_else, sizeof(if_stack[curr_if].label_else),
                    "if_else_%d", if_label_seq);
            snprintf(if_stack[curr_if].label_end, sizeof(if_stack[curr_if].label_end),
                    "if_end_%d", if_label_seq);
            if_label_seq++;


            // branch based on operator
            if (strcmp(op, "<") == 0) fprintf(fout, "    b.ge %s\n", if_stack[curr_if].label_else);
            else if (strcmp(op, ">") == 0) fprintf(fout, "    b.le %s\n", if_stack[curr_if].label_else);
            else if (strcmp(op, "==") == 0) fprintf(fout, "    b.ne %s\n", if_stack[curr_if].label_else);
            else if (strcmp(op, "!=") == 0) fprintf(fout, "    b.eq %s\n", if_stack[curr_if].label_else);
            else if (strcmp(op, "=!") == 0) fprintf(fout, "    b.eq %s\n", if_stack[curr_if].label_else);
            else if (strcmp(op, "<=") == 0) fprintf(fout, "    b.gt %s\n", if_stack[curr_if].label_else);
            else if (strcmp(op, "=<") == 0) fprintf(fout, "    b.gt %s\n", if_stack[curr_if].label_else);
            else if (strcmp(op, ">=") == 0) fprintf(fout, "    b.lt %s\n", if_stack[curr_if].label_else);
            else if (strcmp(op, "=>") == 0) fprintf(fout, "    b.lt %s\n", if_stack[curr_if].label_else);
            else error_syntax(line_num, "Unsupported operator in if");

            // now increment counter
            if_counter++;


            continue;
        }

        // check for '} else {' first
        if (strstr(line, "} else {")) {
            if (if_counter == 0) error_syntax(line_num, "Unexpected else without matching if");

            IfLabel *curr = &if_stack[if_counter - 1];
            curr->has_else = true;

            // branch to skip else block
            fprintf(fout, "    b %s\n", curr->label_end);

            // else label
            fprintf(fout, "%s:\n", curr->label_else);

            continue;
        }

        // check for 'else' alone (possibly with '{')
        char *ltrim = trim(line);
        if (strncmp(ltrim, "else", 4) == 0) {
            if (if_counter == 0) error_syntax(line_num, "Unexpected else without matching if");

            // remove trailing '{' if present
            char *brace = strchr(ltrim, '{');
            if (brace) *brace = '\0';
            trim(ltrim);

            IfLabel *curr = &if_stack[if_counter - 1];
            curr->has_else = true;

            // branch to skip else block
            fprintf(fout, "    b %s\n", curr->label_end);

            // else label
            fprintf(fout, "%s:\n", curr->label_else);

            continue;
        }


        // ---- typed declaration: num <var> = <expr>
                // ---- typed declaration: num <var> = <expr>
        if (strncmp(line, "num ", 4) == 0) {
            char *rest = trim(line + 4);
            // split on top-level '='
            char *sep = find_top_level_sep(rest);
            if (!sep || *sep != '=') {
                error_syntax(line_num, "Unsupported decleration syntax in num type decleration");
                return 1;
            }
            *sep = '\0';
            char lhsbuf[128], rhsbuf[256];
            strncpy(lhsbuf, rest, sizeof(lhsbuf)-1); lhsbuf[sizeof(lhsbuf)-1] = '\0';
            strncpy(rhsbuf, sep+1, sizeof(rhsbuf)-1); rhsbuf[sizeof(rhsbuf)-1] = '\0';
            char *varname = trim(lhsbuf);
            char *rhs = trim(rhsbuf);
            // check redefinition
            if (get_var_label(varname) != NULL) error_redef(line_num, varname);
            // assign var (returns a memory label like "var_3")
            char *vlabel = assign_var(varname);

            // check if rhs is math: a <op> b
            char a[128], b[128];
            char op;
            if (sscanf(rhs, "%127s %c %127s", a, &op, b) == 3) {
                const char *asmop = NULL;
                switch (op) {
                    case '+': asmop = "add"; break;
                    case '-': asmop = "sub"; break;
                    case '*': asmop = "mul"; break;
                    case '/': asmop = "sdiv"; break;
                }
                if (!asmop) { error_syntax(line_num, "Unsupported operator in function call"); return 1; }

                // load operand a into w0
                if (is_number(a)) {
                    fprintf(fout, "    mov w0, #%s\n", a);
                } else if (is_register(a)) {
                    fprintf(fout, "    mov w0, %s\n", a);
                } else {
                    char *alabel = get_var_label(a);
                    if (!alabel) error_undef(line_num, a);
                    fprintf(fout, "    adrp x9, %s@PAGE\n", alabel);
                    fprintf(fout, "    ldr  w0, [x9, %s@PAGEOFF]\n", alabel);
                }

                // load operand b into w1
                if (is_number(b)) {
                    fprintf(fout, "    mov w1, #%s\n", b);
                } else if (is_register(b)) {
                    fprintf(fout, "    mov w1, %s\n", b);
                } else {
                    char *blabel = get_var_label(b);
                    if (!blabel) error_undef(line_num, b);
                    fprintf(fout, "    adrp x9, %s@PAGE\n", blabel);
                    fprintf(fout, "    ldr  w1, [x9, %s@PAGEOFF]\n", blabel);
                }

                // compute into w2 and store into variable memory
                fprintf(fout, "    %s w2, w0, w1\n", asmop);
                fprintf(fout, "    adrp x9, %s@PAGE\n", vlabel);
                fprintf(fout, "    str  w2, [x9, %s@PAGEOFF]\n", vlabel);
            } else {
                // simple assign: num x = <literal/register/var>
                // simple assign: num x = <literal/register/var>
                if (is_number(rhs)) {

                    fprintf(fout, "    mov w0, #%s\n", rhs);
                    fprintf(fout, "    adrp x9, %s@PAGE\n", vlabel);
                    fprintf(fout, "    str  w0, [x9, %s@PAGEOFF]\n", vlabel);

                } else if (is_register(rhs)) {

                    fprintf(fout, "    adrp x9, %s@PAGE\n", vlabel);
                    fprintf(fout, "    str  %s, [x9, %s@PAGEOFF]\n", rhs, vlabel);

                } else {

                    // variable RHS: load then store
                    char *rlabel = get_var_label(rhs);
                    if (!rlabel) error_undef(line_num, rhs);

                    fprintf(fout, "    adrp x9, %s@PAGE\n", rlabel);
                    fprintf(fout, "    ldr  w0, [x9, %s@PAGEOFF]\n", rlabel);

                    fprintf(fout, "    adrp x9, %s@PAGE\n", vlabel);
                    fprintf(fout, "    str  w0, [x9, %s@PAGEOFF]\n", vlabel);
                }
            }
            continue;
        }

        // ---- setr / setm (supports both '=' and ',') robust parsing
        if (strncmp(line, "setr", 4) == 0 || strncmp(line, "setm", 4) == 0) {
            bool is_setr = (strncmp(line, "setr", 4) == 0);
            char *rest = trim(line + 4);
            char *sep = find_top_level_sep(rest);
            if (!sep) { error_syntax(line_num, "Expected ',' or '=' in setr/setm function call"); continue; }
            char sepch = *sep;
            *sep = '\0';
            char lhsbuf[MAX_LINE], rhsbuf[MAX_LINE];
            strncpy(lhsbuf, rest, sizeof(lhsbuf)-1); lhsbuf[sizeof(lhsbuf)-1]='\0';
            strncpy(rhsbuf, sep+1, sizeof(rhsbuf)-1); rhsbuf[sizeof(rhsbuf)-1]='\0';
            char *lhs = trim(lhsbuf);
            char *rhs = trim(rhsbuf);

            // count commas in the rest of the statement
            int comma_count = 0;
            for (char *p = rest; *p; ++p) {
                if (*p == ',') comma_count++;
            }

            if (comma_count > 1) {
                error_syntax(line_num, "Too many arguments in setr/setm function call");
            }

            if (is_setr) {
                // setr dest = src  OR setr dest, src
                // dest is expected to be a register (e.g. w0) - keep that behavior
                if (!is_register(lhs)) {
                    // user attempted to set a non-register with setr: allow register-like usage for lhs by treating it as memory target?
                    // Here we treat lhs as a register name if it looks like one; otherwise error.
                    fprintf(stderr, "Error: setr destination must be a register (line %d): %s\n", line_num, lhs);
                    return 1;
                }

                if (rhs[0] == '[') {
                    // memory operand form preserved as-is
                    fprintf(fout, "    ldr %s, %s\n", lhs, rhs);
                } else if (is_number(rhs)) {
                    fprintf(fout, "    mov %s, #%s\n", lhs, rhs);
                } else if (is_register(rhs)) {
                    fprintf(fout, "    mov %s, %s\n", lhs, rhs);
                } else {
                    // rhs can now be a variable in RAM: load it into the register
                    char *rlabel = get_var_label(rhs);
                    if (!rlabel) error_undef(line_num, rhs);
                    fprintf(fout, "    adrp x9, %s@PAGE\n", rlabel);
                    fprintf(fout, "    ldr  %s, [x9, %s@PAGEOFF]\n", lhs, rlabel);
                }
            } else {
                // setm MEM, SRC
                int lhs_is_mem = (lhs[0] == '[');
                int lhs_is_var = is_var(lhs);

                if (!lhs_is_mem && !lhs_is_var) {
                    fprintf(stderr,
                        "Error: setm destination must be memory (line %d): %s\n",
                        line_num, lhs);
                    return 1;
                }

                /* ---- load RHS into w0 if needed ---- */

                if (is_number(rhs)) {

                    fprintf(fout, "    mov w0, #%s\n", rhs);

                } else if (is_register(rhs)) {

                    fprintf(fout, "    mov w0, %s\n", rhs);

                } else if (is_var(rhs)) {

                    char *rlabel = get_var_label(rhs);
                    fprintf(fout, "    adrp x9, %s@PAGE\n", rlabel);
                    fprintf(fout, "    ldr  w0, [x9, %s@PAGEOFF]\n", rlabel);

                } else {
                    error_undef(line_num, rhs);
                }

                /* ---- store w0 into LHS ---- */

                if (lhs_is_mem) {

                    // e.g. [sp, #4]
                    fprintf(fout, "    str w0, %s\n", lhs);

                } else {
                    // lhs is a variable
                    char *llabel = get_var_label(lhs);
                    fprintf(fout, "    adrp x9, %s@PAGE\n", llabel);
                    fprintf(fout, "    str  w0, [x9, %s@PAGEOFF]\n", llabel);
                }

            }
            continue;
        }

        // ---- math: dest = a <op> b  (dest can be a register or a declared variable (memory))
        {
            char dest[128], a[128], b[128];
            char oper;
            if (sscanf(line, "%127s = %127s %c %127s", dest, a, &oper, b) == 4) {
                const char *asmop = NULL;
                switch (oper) {
                    case '+': asmop = "add"; break;
                    case '-': asmop = "sub"; break;
                    case '*': asmop = "mul"; break;
                    case '/': asmop = "sdiv"; break;
                }
                if (!asmop) { fprintf(stderr, "Unsupported operator (line %d)\n", line_num); return 1; }

                bool dest_is_reg = is_register(dest);
                char *dest_label = NULL;
                if (!dest_is_reg) {
                    dest_label = get_var_label(dest);
                    if (!dest_label) error_undef(line_num, dest);
                }

                // load a -> w0
                if (is_number(a)) {
                    fprintf(fout, "    mov w0, #%s\n", a);
                } else if (is_register(a)) {
                    fprintf(fout, "    mov w0, %s\n", a);
                } else {
                    char *alabel = get_var_label(a);
                    if (!alabel) error_undef(line_num, a);
                    fprintf(fout, "    adrp x9, %s@PAGE\n", alabel);
                    fprintf(fout, "    ldr  w0, [x9, %s@PAGEOFF]\n", alabel);
                }

                // load b -> w1
                if (is_number(b)) {
                    fprintf(fout, "    mov w1, #%s\n", b);
                } else if (is_register(b)) {
                    fprintf(fout, "    mov w1, %s\n", b);
                } else {
                    char *blabel = get_var_label(b);
                    if (!blabel) error_undef(line_num, b);
                    fprintf(fout, "    ldr w1, %s\n", blabel);
                }

                // compute -> w2
                fprintf(fout, "    %s w2, w0, w1\n", asmop);

                // store result to dest (register or memory)
                if (dest_is_reg) {
                    fprintf(fout, "    mov %s, w2\n", dest);
                } else {

                    fprintf(fout, "    adrp x9, %s@PAGE\n", dest_label);
                    fprintf(fout, "    str w2, [x9, %s@PAGEOFF]\n", dest_label);
                }
                continue;
            }
        }

        // ---- simple assignment: left = right  (left can be a register or a declared variable in RAM)
        {
            // ---- simple assignment: left = right (left can be reg or variable)
            {
                char left[128], right[256];
                if (sscanf(line, "%127s = %255s", left, right) == 2) {

                    bool left_is_reg = is_register(left);
                    char *left_label = NULL;
                    if (!left_is_reg) {
                        left_label = get_var_label(left);
                        if (!left_label) error_undef(line_num, left);
                    }

                    // Number
                    if (is_number(right)) {
                        fprintf(fout, "    mov w0, #%s\n", right);

                        if (left_is_reg) {
                            fprintf(fout, "    mov %s, w0\n", left);
                        } else {
                            fprintf(fout, "    adrp x9, %s@PAGE\n", left_label);
                            fprintf(fout, "    str  w0, [x9, %s@PAGEOFF]\n", left_label);
                        }
                        continue;
                    }

                    // Register
                    if (is_register(right)) {
                        if (left_is_reg) {
                            fprintf(fout, "    mov %s, %s\n", left, right);
                        } else {
                            fprintf(fout, "    adrp x9, %s@PAGE\n", left_label);
                            fprintf(fout, "    str  %s, [x9, %s@PAGEOFF]\n", right, left_label);
                        }
                        continue;
                    }

                    // Variable in RAM
                    char *rlabel = get_var_label(right);
                    if (!rlabel) error_undef(line_num, right);

                    // Load rlabel → w0
                    fprintf(fout, "    adrp x9, %s@PAGE\n", rlabel);
                    fprintf(fout, "    ldr  w0, [x9, %s@PAGEOFF]\n", rlabel);

                    if (left_is_reg) {
                        fprintf(fout, "    mov %s, w0\n", left);
                    } else {
                        fprintf(fout, "    adrp x9, %s@PAGE\n", left_label);
                        fprintf(fout, "    str  w0, [x9, %s@PAGEOFF]\n", left_label);
                    }

                    continue;
                }
            }

        }
        

        // fallback: emit raw (indented)
        if (strcmp(line, "{") == 0 || strcmp(line, "}") == 0)
            continue; // skip literal braces


        trim(line);
        if (line[0] == '\0') continue;

        fprintf(fout, "    %s\n", line);

    }

    fprintf(fout, "    ldr x16, =0x2000001   // exit syscall\n");
    fprintf(fout, "    mov x0, 0\n");
    fprintf(fout, "    svc 0\n");

    emit_all_variables(fout);
    emit_all_string_literals(fout);

    fclose(fin);
    fclose(fout);
    printf("Transpilation complete: %s -> %s\n", argv[1], argv[2]);
    return 0;
}