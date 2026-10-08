#include "errors.h"

void error_undef(int line_num, const char *varname) {
    fprintf(stderr, "[compiler error] variable '%s' not defined! line: %d\n", varname, line_num);
    exit(1);
}

void error_redef(int line_num, const char *varname) {
    fprintf(stderr, "[compiler error] variable '%s' redefined! line: %d\n", varname, line_num);
    exit(1);
}

void error_func_args(int line_num, const char *funcname) {
    fprintf(stderr, "[compiler error] function '%s' called with wrong number of arguments! line: %d\n", funcname, line_num);
    exit(1);
}

void error_syntax(int line_num, const char *msg) {
    fprintf(stderr, "[compiler error] %s line: %d\n", msg, line_num);
    exit(1);
}
