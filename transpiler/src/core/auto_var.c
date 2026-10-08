#include "auto_var.h"

// ---------------- Variable tracking ----------------
char declared_vars[MAX_VARS][128];
int declared_count = 0;

int is_declared(const char *name) {
    for (int i = 0; i < declared_count; i++) {
        if (strcmp(declared_vars[i], name) == 0) return 1;
    }
    return 0;
}

void add_var(const char *name) {
    if (!is_declared(name) && declared_count < MAX_VARS) {
        strncpy(declared_vars[declared_count++], name, sizeof(declared_vars[0]) - 1);
        declared_vars[declared_count-1][sizeof(declared_vars[0]) - 1] = '\0';
    }
}

// ---------------- String utils ----------------
void trim(char *s) {
    char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);

    char *end = s + strlen(s) - 1;
    while (end >= s && isspace((unsigned char)*end)) *end-- = '\0';
}

int next_nonspace_idx(const char *s, int idx) {
    int i = idx;
    while (s[i]) {
        if (!isspace((unsigned char)s[i])) return i;
        i++;
    }
    return -1;
}

int prev_nonspace_idx(const char *s, int idx) {
    int i = idx;
    while (i >= 0) {
        if (!isspace((unsigned char)s[i])) return i;
        i--;
    }
    return -1;
}

// ---------------- Type inference ----------------
const char *infer_type(const char *rhs) {
    char buf[1024];
    strncpy(buf, rhs, sizeof(buf));
    buf[sizeof(buf)-1] = '\0';
    trim(buf);

    int len = strlen(buf);

    if (len > 1 && (buf[len-1] == 'f' || buf[len-1] == 'F')) return "float";
    for (int i = 0; buf[i]; i++) if (buf[i] == '.') return "float";

    int all_digits = 1, has_digit = 0;
    for (int i = 0; buf[i]; i++) {
        if (isdigit((unsigned char)buf[i])) has_digit = 1;
        else if (!isspace((unsigned char)buf[i]) &&
                 buf[i] != '+' && buf[i] != '-' &&
                 buf[i] != '*' && buf[i] != '/' &&
                 buf[i] != '(' && buf[i] != ')' &&
                 buf[i] != '%') {
            all_digits = 0; break;
        }
    }
    if (has_digit && all_digits) return "int";

    if (strstr(buf, "rand(")) return "int";
    if (strstr(buf, "time(")) return "time_t";
    if (strstr(buf, "malloc(")) return "void*";
    if (strstr(buf, "carr(")) return "Array*";
    if (strstr(buf, "h1(")) return "int";
    if (strstr(buf, "sha256(")) return "char*";
    if (strstr(buf, "cap")) return "bool";
    if (strstr(buf, "nocap")) return "bool";

    // new bools
    if (strstr(buf, "cap")) return "bool";
    if (strstr(buf, "nocap")) return "bool";
    if (strstr(buf, "maybe")) return "bool";
    if (strstr(buf, "sometimes")) return "bool";
    if (strstr(buf, "can you repeat the question")) return "bool";

    if (strstr(buf, "true")) return "bool";
    if (strstr(buf, "false")) return "bool";
    if (strstr(buf, "yes")) return "bool";
    if (strstr(buf, "no")) return "bool";

    if (strstr(buf, "unknown")) return "bool";
    if (strstr(buf, "definitely")) return "bool";
    if (strstr(buf, "possibly")) return "bool";
    if (strstr(buf, "impossible")) return "bool";
    if (strstr(buf, "because")) return "bool";

    return "float";
}

// ---------------- Main core ----------------
void check_variable(char *line) {
    trim(line);
    if (strlen(line) == 0) return;

    // Skip lines that already start with a type
    const char *types[] = {
        "void", "char", "short", "int", "long", "float", "double", "unsigned",
        "time_t", "size_t", "Array", "FILE",
        "int8_t","int16_t","int32_t","int64_t","int128_t",
        "uint8_t","uint16_t","uint32_t","uint64_t","uint128_t",
        "void*","char*","short*","int*","long*","float*","double*","unsigned*",
        "time_t*","size_t*","Array*","FILE*",
        "int8_t*","int16_t*","int32_t*","int64_t*","int128_t*",
        "uint8_t*","uint16_t*","uint32_t*","uint64_t*","uint128_t*"
    };

    for (int i = 0; i < (int)(sizeof(types)/sizeof(types[0])); i++) {
        size_t len = strlen(types[i]);
        if (strncmp(line, types[i], len) == 0 &&
            (line[len] == '\0' || isspace((unsigned char)line[len]) || line[len] == '*')) {
            char tmp[128];
            const char *p = line + len;
            while (*p && isspace((unsigned char)*p)) p++;
            int j = 0;
            while (*p && (isalnum((unsigned char)*p) || *p == '_' || *p == '*')) {
                if (j < (int)sizeof(tmp)-1) tmp[j++] = *p;
                p++;
            }
            tmp[j] = '\0';
            trim(tmp);
            add_var(tmp);
            return;
        }
    }

    // Skip keywords
    const char *keywords[] = {"if","else","else if","for","while","switch","return"};
    for (int i = 0; i < (int)(sizeof(keywords)/sizeof(keywords[0])); i++) {
        size_t len = strlen(keywords[i]);
        if (strncmp(line, keywords[i], len) == 0 &&
            (line[len] == '\0' || isspace((unsigned char)line[len]) || line[len] == '(')) {
            return;
        }
    }

    // Find assignment '='
    int eq_pos = -1;

    for (int i = 0; line[i]; i++) {
        if (line[i] == '=') {
            int next = next_nonspace_idx(line, i+1);
            int prev = prev_nonspace_idx(line, i-1);
            if (next != -1 && line[next] == '=') continue;
            if (prev != -1 && (line[prev] == '!' || line[prev] == '<' || line[prev] == '>')) continue;
            eq_pos = i;
            break;
        }

    }

    if (eq_pos != -1) {
        // '=' assignment found
    }
     else {
        return;
    }

    // Split var and rhs
    char var[128], rhs[512];
    strncpy(var, line, eq_pos);
    var[eq_pos] = '\0';
    strcpy(rhs, line + eq_pos + 1);
    trim(var);
    trim(rhs);

    // Validate identifier
    if (strlen(var) == 0) return;
    int valid_ident = 1;
    int start = 0;
    if (var[start] == '*') start++;
    for (int i = start; var[i]; i++) {
        if (!(isalnum((unsigned char)var[i]) || var[i] == '_')) { valid_ident = 0; break; }
    }
    if (!valid_ident) return;

    // If already declared, leave assignment as-is
    if (is_declared(var)) return;

    // Infer type and rewrite line
    const char *type = infer_type(rhs);
    char new_line[1024];
    snprintf(new_line, sizeof(new_line), "%s %s = %s;", type, var, rhs);
    strcpy(line, new_line);

    add_var(var);
}
