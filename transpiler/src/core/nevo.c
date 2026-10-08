#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <mode> <input file> <output file>\n", argv[0]);
        printf("Modes:\n");
        printf("  -n   Compile .n file (C transpile + compile)\n");
        return 1;
    }

    const char *mode = argv[1];

    if (strcmp(mode, "-version") == 0 || strcmp(mode, "--version") == 0) {
        #if defined(_WIN32) || defined(_WIN64)
            printf("[nevo] Version 2.4 Windows\n");
        #else
            printf("[nevo] Version 2.4 macOS\n");
        #endif
        return 0;
    }

    if (argc < 4) {
        fprintf(stderr, "Mode %s requires an input and output file.\n", mode);
        return 1;
    }

    const char *input = argv[2];
    const char *output = argv[3];
    char cmd[1024];

    // -------------------------
    // Mode: -n (run n compiler)
    // -------------------------
    if (strcmp(mode, "-n") == 0) {
        #if defined(_WIN32) || defined(_WIN64)
            snprintf(cmd, sizeof(cmd), "C:\\nevo\\Modes\\n %s %s", input, output);
        #else
            snprintf(cmd, sizeof(cmd), "$HOME/nevo/Modes/n %s %s", input, output);
        #endif
            int ret = system(cmd);
            if (ret != 0) {
                fprintf(stderr, "[nevo] n compiler failed with code %d\n", ret);
                return ret;
            }
            return 0;
    }
    else {
        fprintf(stderr, "Unknown mode: %s\n", mode);
        return 2;
    }
}
