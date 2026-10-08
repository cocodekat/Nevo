#include "compiler_api.h"

#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <ast.json> <output.asm>\n", argv[0]);
        return 1;
    }
    return nevo_codegen_file(argv[1], argv[2]);
}
