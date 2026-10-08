#ifndef BAN_LIST_H
#define BAN_LIST_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// ---------------- Globals ----------------
extern int error;

// Whitelist
extern const char* white_list[];
extern const int white_count;

// Base types
extern const char* types[];
extern const int types_count;

// Typedef registry
#define MAX_TYPEDEFS 100
extern char* typedef_names[MAX_TYPEDEFS];
extern int typedef_count;

// Function registry
#define MAX_FUNCS 100
extern char* func_names[MAX_FUNCS];
extern int func_count;

// ---------------- Function declarations ----------------
void trim_line(char* buffer);
void register_functions_in_line(const char* line);
void check_function_calls_in_line(const char* line);
int ban_error(void);

#endif // BAN_LIST_H
