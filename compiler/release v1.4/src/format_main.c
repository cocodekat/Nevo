#include "compiler_api.h"
#include "source_graph.h"

#include <stdio.h>
#include <string.h>

static void usage(const char *program)
{
    fprintf(stderr, "Usage: %s <source.n> [source.n ...] -o <ast.json>\n",
            program);
}

static int format_with_includes(int source_count, char **source_paths,
                                const char *output_path)
{
    NevoSourceList sources;
    if (!nevo_collect_sources(source_count, source_paths, &sources))
        return 1;
    int result = nevo_format_files(sources.count, sources.paths, output_path);
    nevo_free_source_list(&sources);
    return result;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        usage(argv[0]);
        return 1;
    }

    /* Preserve v1.1's: format input.n output.json */
    if (argc == 3 && strcmp(argv[1], "-o") && strcmp(argv[2], "-o"))
        return format_with_includes(1, &argv[1], argv[2]);

    int output_index = -1;
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "-o"))
        {
            output_index = i;
            break;
        }

    if (output_index < 2 || output_index + 2 != argc)
    {
        usage(argv[0]);
        return 1;
    }
    return format_with_includes(output_index - 1, &argv[1],
                                argv[output_index + 1]);
}
