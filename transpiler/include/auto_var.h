#ifndef AUTO_VAR_H
#define AUTO_VAR_H

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h> // for bool

// ---------------- Variable tracking ----------------
#define MAX_VARS 256

extern char declared_vars[MAX_VARS][128];
extern int declared_count;

// Declare functions
int is_declared(const char *name);
void add_var(const char *name);

// ---------------- String utils ----------------
void trim(char *s);
int next_nonspace_idx(const char *s, int idx);
int prev_nonspace_idx(const char *s, int idx);

// ---------------- Type inference ----------------
const char *infer_type(const char *rhs);

// ---------------- Main core ----------------
void check_variable(char *line);

#endif // AUTO_VAR_H
