#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#define MAX_LINE 512
#define MAX_VARS 256
#define FIRST_VAR_REG 1

typedef struct {
    char name[64];
    char reg[8];   // e.g. "w1"
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

char *get_var_reg(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < var_count; ++i) {
        if (strcmp(vars[i].name, name) == 0) return vars[i].reg;
    }
    return NULL;
}

// Assign a new register to a variable (w1..w30). Return pointer to reg string.
char *assign_var(const char *name) {
    if (!name) return NULL;
    if (var_count >= 30) { fprintf(stderr, "Too many variables (limit 30)\n"); exit(1); }
    strncpy(vars[var_count].name, name, sizeof(vars[var_count].name)-1);
    vars[var_count].name[sizeof(vars[var_count].name)-1] = '\0';
    snprintf(vars[var_count].reg, sizeof(vars[var_count].reg), "w%d", FIRST_VAR_REG + var_count); // start from w4
    var_count++;
    return vars[var_count-1].reg;
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

void error_redef(int line_num) {
    fprintf(stderr, "Error: Redefinition occurred (line %d)\n", line_num);
    exit(1);
}

void error_undef(int line_num, const char *name) {
    fprintf(stderr, "Error: Variable not defined (line %d): %s\n", line_num, name);
    exit(1);
}

int main(int argc, char **argv) {
    // reserve integer format string for printf
    add_string_literal("%d"); // this will be used for printing numbers

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input.n> <output.s>\n", argv[0]);
        return 1;
    }

    FILE *fin = fopen(argv[1], "r");
    FILE *fout = fopen(argv[2], "w");
    if (!fin || !fout) { fprintf(stderr, "Could not open files\n"); return 1; }

    emit_all_string_literals(fout);
    fprintf(fout, ".text\n");

    char rawline[MAX_LINE];
    bool text_written = false;
    int line_num = 0;

    while (fgets(rawline, sizeof(rawline), fin)) {
        line_num++;
        // strip newline
        rawline[strcspn(rawline, "\n")] = '\0';
        char *line = trim(rawline);
        if (!line || *line == '\0') continue;



        // function detection
        size_t len = strlen(line);
        if (len >= 3 && line[len-1] == '{' && strstr(line, "()")) {
            char func[128] = {0};
            sscanf(line, "%127[^()]()", func);
            trim(func);
            if (!text_written) { 
                fprintf(fout, ".text\n"); 
                text_written = true; 
            }
            fprintf(fout, ".global %s\n%s:\n", func, func);
            continue;
        }
        if (strcmp(line, "}") == 0) continue;

        // ---- print(...) statement
        if (strncmp(line, "print(", 6) == 0 && line[strlen(line)-1] == ')') {
            char buf[MAX_LINE];
            size_t len = strlen(line);
            if (len < 7) continue; // minimal "print()" length
            strncpy(buf, line + 6, len - 7); // extract inside parentheses
            buf[len - 7] = '\0';
            char *arg = trim(buf);

            // check if string literal: starts and ends with quotes
            if (arg[0] == '"' && arg[strlen(arg)-1] == '"') {
                arg[strlen(arg)-1] = '\0'; // remove trailing quote
                char *strval = arg + 1;    // skip leading quote

                // emit string literal label
                char label[64];
                snprintf(label, sizeof(label), "str_%d", str_count++);

                // emit string literal immediately
                fprintf(fout, ".data\n%s:\n    .asciz \"%s\"\n.text\n", label, strval);

                // emit code to write string safely on macOS ARM64
                fprintf(fout, "    // print string literal\n");
                fprintf(fout, "    ldr x16, =0x2000004\n");               // write syscall
                fprintf(fout, "    mov x0, 1\n");                          // stdout
                fprintf(fout, "    adrp x1, %s@PAGE\n", label);            // page of string
                fprintf(fout, "    add x1, x1, %s@PAGEOFF\n", label);      // offset in page
                fprintf(fout, "    mov x2, %zu\n", strlen(strval));       // length
                fprintf(fout, "    svc 0\n");

            } else {
                // placeholder
            }
            continue;
        }





        // ---- typed declaration: num <var> = <expr>
        if (strncmp(line, "num ", 4) == 0) {
            char *rest = trim(line + 4);
            // split on top-level '='
            char *sep = find_top_level_sep(rest);
            if (!sep || *sep != '=') {
                fprintf(stderr, "Syntax error in declaration (line %d)\n", line_num);
                return 1;
            }
            *sep = '\0';
            char lhsbuf[128], rhsbuf[256];
            strncpy(lhsbuf, rest, sizeof(lhsbuf)-1); lhsbuf[sizeof(lhsbuf)-1] = '\0';
            strncpy(rhsbuf, sep+1, sizeof(rhsbuf)-1); rhsbuf[sizeof(rhsbuf)-1] = '\0';
            char *varname = trim(lhsbuf);
            char *rhs = trim(rhsbuf);
            // check redefinition
            if (get_var_reg(varname) != NULL) error_redef(line_num);
            // assign var
            char *vreg = assign_var(varname);

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
                if (!asmop) { fprintf(stderr, "Unsupported operator (line %d)\n", line_num); return 1; }

                // resolve a
                char *areg = NULL;
                if (is_number(a)) {
                    fprintf(fout, "    mov w0, #%s\n", a);
                    areg = "w0";
                } else if (is_register(a)) {
                    areg = a;
                } else {
                    // variable must be defined
                    char *r = get_var_reg(a);
                    if (!r) error_undef(line_num, a);
                    areg = r;
                }

                // resolve b
                char *breg = NULL;
                if (is_number(b)) {
                    fprintf(fout, "    mov w0, #%s\n", b);
                    breg = "w0";
                } else if (is_register(b)) {
                    breg = b;
                } else {
                    char *r = get_var_reg(b);
                    if (!r) error_undef(line_num, b);
                    breg = r;
                }

                fprintf(fout, "    %s %s, %s, %s\n", asmop, vreg, areg, breg);
            } else {
                // simple assign: num x = <literal/register/var>
                if (is_number(rhs)) {
                    fprintf(fout, "    mov %s, #%s\n", vreg, rhs);
                } else if (is_register(rhs)) {
                    fprintf(fout, "    mov %s, %s\n", vreg, rhs);
                } else {
                    // variable RHS must be defined
                    char *r = get_var_reg(rhs);
                    if (!r) error_undef(line_num, rhs);
                    fprintf(fout, "    mov %s, %s\n", vreg, r);
                }
            }
            continue;
        }

        // ---- setr / setm (supports both '=' and ',') robust parsing
        if (strncmp(line, "setr", 4) == 0 || strncmp(line, "setm", 4) == 0) {
            bool is_setr = (strncmp(line, "setr", 4) == 0);
            char *rest = trim(line + 4);
            char *sep = find_top_level_sep(rest);
            if (!sep) { fprintf(fout, "    %s\n", line); continue; }
            char sepch = *sep;
            *sep = '\0';
            char lhsbuf[MAX_LINE], rhsbuf[MAX_LINE];
            strncpy(lhsbuf, rest, sizeof(lhsbuf)-1); lhsbuf[sizeof(lhsbuf)-1]='\0';
            strncpy(rhsbuf, sep+1, sizeof(rhsbuf)-1); rhsbuf[sizeof(rhsbuf)-1]='\0';
            char *lhs = trim(lhsbuf);
            char *rhs = trim(rhsbuf);

            if (is_setr) {
                // setr dest = src  OR setr dest, src
                if (rhs[0] == '[') {
                    fprintf(fout, "    ldr %s, %s\n", lhs, rhs);
                } else if (is_number(rhs)) {
                    fprintf(fout, "    mov %s, #%s\n", lhs, rhs);
                } else if (is_register(rhs)) {
                    fprintf(fout, "    mov %s, %s\n", lhs, rhs);
                } else {
                    // rhs must be a declared variable
                    char *r = get_var_reg(rhs);
                    if (!r) error_undef(line_num, rhs);
                    fprintf(fout, "    mov %s, %s\n", lhs, r);
                }
            } else {
                // setm lhs = rhs  OR setm lhs, rhs
                if (rhs[0] == '[') {
                    // mem->mem not supported
                    fprintf(fout, "    /* unsupported: setm %s = %s (mem->mem) */\n", lhs, rhs);
                } else if (is_number(rhs)) {
                    fprintf(fout, "    mov w0, #%s\n", rhs);
                    fprintf(fout, "    str w0, %s\n", lhs);
                } else if (is_register(rhs)) {
                    fprintf(fout, "    str %s, %s\n", rhs, lhs);
                } else {
                    // rhs must be declared variable
                    char *r = get_var_reg(rhs);
                    if (!r) error_undef(line_num, rhs);
                    fprintf(fout, "    str %s, %s\n", r, lhs);
                }
            }
            continue;
        }

        // ---- math: dest = a <op> b  (dest must be declared variable or a register)
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

                // dest resolution: if register ok, else must be previously declared variable
                char *dest_reg = NULL;
                if (is_register(dest)) dest_reg = dest;
                else dest_reg = get_var_reg(dest);
                if (!dest_reg) error_undef(line_num, dest);

                // resolve a
                char *areg = NULL;
                if (is_number(a)) {
                    fprintf(fout, "    mov w0, #%s\n", a);
                    areg = "w0";
                } else if (is_register(a)) {
                    areg = a;
                } else {
                    areg = get_var_reg(a);
                    if (!areg) error_undef(line_num, a);
                }

                // resolve b
                char *breg = NULL;
                if (is_number(b)) {
                    fprintf(fout, "    mov w0, #%s\n", b);
                    breg = "w0";
                } else if (is_register(b)) {
                    breg = b;
                } else {
                    breg = get_var_reg(b);
                    if (!breg) error_undef(line_num, b);
                }

                fprintf(fout, "    %s %s, %s, %s\n", asmop, dest_reg, areg, breg);
                continue;
            }
        }

        // ---- simple assignment: left = right  (left must be declared variable or register)
        {
            char left[128], right[256];
            if (sscanf(line, "%127s = %255s", left, right) == 2) {
                // left must be register or declared variable
                char *left_reg = NULL;
                if (is_register(left)) left_reg = left;
                else left_reg = get_var_reg(left);
                if (!left_reg) error_undef(line_num, left);

                // right resolution
                if (is_number(right)) {
                    fprintf(fout, "    mov %s, #%s\n", left_reg, right);
                } else if (is_register(right)) {
                    fprintf(fout, "    mov %s, %s\n", left_reg, right);
                } else {
                    char *r = get_var_reg(right);
                    if (!r) error_undef(line_num, right);
                    fprintf(fout, "    mov %s, %s\n", left_reg, r);
                }
                continue;
            }
        }

        // fallback: emit raw (indented)
        fprintf(fout, "    %s\n", line);
    }

    fprintf(fout, "    ldr x16, =0x2000001   // exit syscall\n");
    fprintf(fout, "    mov x0, 0\n");
    fprintf(fout, "    svc 0\n");

    fclose(fin);
    fclose(fout);
    printf("Transpilation complete: %s -> %s\n", argv[1], argv[2]);
    return 0;
}
