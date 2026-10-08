#include "ban_list.h"

// ---------------- Globals ----------------
int error = 0;

// Whitelist
const char* white_list[] = {
    "arradd(", "carr(", "farr(", "parr(", "get(", "gets(",
    "rand", "srand(", "sha256(", "sha256_bin(", "h1(", "h1_bin(", "kaboom(", "pbin(", "unless(",
    "print(", "exit(", "stop(", "yap(", "fart(",
    "fread(", "freadf(", "fwritef(", "fopen(", "fclose(",
    "image(", "compile(", "depile(",
    "case(", "do", "else", "else if", "for", "if", "switch(", "while",
    "?", ":", "++", "--", "= (", "{ ",
    "return",
    "int ", "char ", "float ", "double ", "long ", "short ",
    "unsigned ", "void ",
    "int8_t", "int16_t", "int32_t", "int64_t", "int128_t",
    "uint8_t", "uint16_t", "uint32_t", "uint64_t", "uint128_t",
    "nocap", "cap",
    "#define", "typedef"
};
const int white_count = sizeof(white_list)/sizeof(white_list[0]);

// Base types
const char* types[] = { "int", "char", "float", "double", "long", "short", "unsigned", "void" };
const int types_count = sizeof(types)/sizeof(types[0]);

// Typedef registry
char* typedef_names[MAX_TYPEDEFS];
int typedef_count = 0;

// Function registry
char* func_names[MAX_FUNCS];
int func_count = 0;

// ---------------- Helper: trim line ----------------
void trim_line(char* buffer) {
    size_t len = strlen(buffer);
    while (len > 0 && isspace((unsigned char)buffer[len-1])) buffer[--len] = '\0';
    if (len > 0 && buffer[len-1] == '!') buffer[--len] = '\0';
}

// ---------------- Register functions from a line ----------------
void register_functions_in_line(const char* line) {
    if (!line || line[0]=='\0') return;

    char buffer[1024];
    strncpy(buffer, line, sizeof(buffer)-1);
    buffer[sizeof(buffer)-1] = '\0';
    trim_line(buffer);

    const char* p = buffer;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '{' || *p == '}') return;

    // Typedef alias
    if (strncmp(p, "typedef", 7) == 0 && isspace((unsigned char)p[7])) {
        const char* alias = strrchr(p, ' ');
        if (alias && typedef_count < MAX_TYPEDEFS) {
            alias++;
            char* clean = strdup(alias);
            clean[strcspn(clean, ";\r\n")] = '\0';
            typedef_names[typedef_count++] = clean;
        }
        return;
    }

    // Register function
    for (int i = 0; i < types_count; i++) {
        size_t tlen = strlen(types[i]);
        if (strncmp(p, types[i], tlen) == 0 && isspace((unsigned char)p[tlen])) {
            const char* name_start = p + tlen;
            while (*name_start && isspace((unsigned char)*name_start)) name_start++;

            const char* name_end = name_start;
            while (*name_end && (isalnum((unsigned char)*name_end) || *name_end=='_')) name_end++;

            if (*name_end != '(') continue;

            if (func_count < MAX_FUNCS) {
                size_t fn_len = name_end - name_start;
                char* fn = malloc(fn_len+1);
                strncpy(fn, name_start, fn_len);
                fn[fn_len] = '\0';
                func_names[func_count++] = fn;
            }
        }
    }
}

// ---------------- Check function calls in a line ----------------
void check_function_calls_in_line(const char* line) {
    error = 0;
    if (!line || line[0]=='\0' || line[0]=='#') return;

    char buffer[1024];
    strncpy(buffer, line, sizeof(buffer)-1);
    buffer[sizeof(buffer)-1] = '\0';
    trim_line(buffer);

    const char* p = buffer;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '{' || *p == '}') return;

    // Whitelist check
    for (int i = 0; i < white_count; i++) {
        if (strstr(p, white_list[i])) return;
    }

    // Typedef aliases
    for (int i = 0; i < typedef_count; i++) {
        size_t n = strlen(typedef_names[i]);
        if (strncmp(p, typedef_names[i], n) == 0 &&
            (isspace((unsigned char)p[n]) || p[n]=='(')) return;
    }

    // Detect function call
    const char* open_paren = strchr(p, '(');
    if (!open_paren) return;

    const char* name_end = open_paren;
    const char* name_start = name_end;
    while (name_start > p && (isalnum((unsigned char)name_start[-1]) || name_start[-1]=='_'))
        name_start--;

    size_t nlen = name_end - name_start;
    if (nlen == 0) return;

    char name[64];
    if (nlen >= sizeof(name)) nlen = sizeof(name)-1;
    strncpy(name, name_start, nlen);
    name[nlen] = '\0';

    int allowed = 0;

    // ✅ Whitelist
    for (int i = 0; i < white_count; i++) {
        size_t wl = strlen(white_list[i]);
        if (strncmp(name, white_list[i], nlen) == 0) { allowed = 1; break; }
    }

    // ✅ User-defined
    if (!allowed) {
        for (int i = 0; i < func_count; i++) {
            if (strcmp(name, func_names[i]) == 0) { allowed = 1; break; }
        }
    }

    if (!allowed) {
        fprintf(stderr, "Error: unknown function '%s' in line: %s\n", name, p);
        error = 1;
    }
}

// ---------------- Return error flag ----------------
int ban_error(void) {
    return error;
}
