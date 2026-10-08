#ifndef ERRORS_H
#define ERRORS_H

#include <stdio.h>
#include <stdlib.h>

void error_undef(int line_num, const char *varname);
void error_redef(int line_num, const char *varname);
void error_func_args(int line_num, const char *funcname);
void error_syntax(int line_num, const char *msg);

#endif // ERRORS_H
