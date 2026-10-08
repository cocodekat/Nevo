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

// Assign a new register to a variable (w1..w30). Return pointer to reg string.
char *assign_var(const char *name) {
    if (!name) return NULL;
    if (var_count >= 256) { fprintf(stderr, "Too many variables\n"); exit(1); }

    snprintf(vars[var_count].name, sizeof(vars[var_count].name), "%s", name);
    snprintf(vars[var_count].label, sizeof(vars[var_count].label), "var_%d", var_count);

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
            if (get_var_label(varname) != NULL) error_redef(line_num);
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
                if (!asmop) { fprintf(stderr, "Unsupported operator (line %d)\n", line_num); return 1; }

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
                if (is_number(rhs)) {
                    fprintf(fout, "    mov w0, #%s\n", rhs);
                    fprintf(fout, "    adrp x9, %s@PAGE\n", vlabel);
                    fprintf(fout, "    str  w0, [x9, %s@PAGEOFF]\n", vlabel);
                } else if (is_register(rhs)) {
                    fprintf(fout, "    str %s, %s\n", rhs, vlabel);
                } else {
                    // variable RHS must be defined (load and store)
                    char *rlabel = get_var_label(rhs);
                    if (!rlabel) error_undef(line_num, rhs);
                    fprintf(fout, "    ldr w0, %s\n", rlabel);
                    fprintf(fout, "    str w0, %s\n", vlabel);
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
                // setm lhs = rhs  OR setm lhs, rhs
                // lhs is a memory destination (could be a variable label or a memory expression)
                if (rhs[0] == '[') {
                    // mem->mem not supported
                    fprintf(fout, "    /* unsupported: setm %s = %s (mem->mem) */\n", lhs, rhs);
                } else if (is_number(rhs)) {
                    fprintf(fout, "    mov w0, #%s\n", rhs);
                    // if lhs is a register-like (user passed a register), store into memory pointed by that register
                    if (is_register(lhs)) {
                        fprintf(fout, "    str w0, %s\n", lhs);
                    } else {
                        // lhs expected to be a variable label or memory label:
                        char *llabel = get_var_label(lhs);
                        if (llabel) {
                            fprintf(fout, "    str w0, %s\n", llabel);
                        } else {
                            // treat lhs as literal memory expression (fallback)
                            fprintf(fout, "    str w0, %s\n", lhs);
                        }
                    }
                } else if (is_register(rhs)) {
                    if (is_register(lhs)) {
                        fprintf(fout, "    str %s, %s\n", rhs, lhs);
                    } else {
                        char *llabel = get_var_label(lhs);
                        if (llabel) {
                            fprintf(fout, "    str %s, %s\n", rhs, llabel);
                        } else {
                            fprintf(fout, "    /* setm: unknown lhs %s */\n", lhs);
                        }
                    }
                } else {
                    // rhs is a variable: load and then store
                    char *rlabel = get_var_label(rhs);
                    if (!rlabel) error_undef(line_num, rhs);
                    fprintf(fout, "    ldr w0, %s\n", rlabel);

                    if (is_register(lhs)) {
                        // store into memory location described by register-like lhs (fallback)
                        fprintf(fout, "    str w0, %s\n", lhs);
                    } else {
                        char *llabel = get_var_label(lhs);
                        if (llabel) {
                            fprintf(fout, "    str w0, %s\n", llabel);
                        } else {
                            fprintf(fout, "    /* setm: unknown lhs %s */\n", lhs);
                        }
                    }
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
                    fprintf(fout, "    str w2, %s\n", dest_label);
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
        fprintf(fout, "    %s\n", line);
    }

    fprintf(fout, "    ldr x16, =0x2000001   // exit syscall\n");
    fprintf(fout, "    mov x0, 0\n");
    fprintf(fout, "    svc 0\n");

    emit_all_variables(fout);

    fclose(fin);
    fclose(fout);
    printf("Transpilation complete: %s -> %s\n", argv[1], argv[2]);
    return 0;
}
