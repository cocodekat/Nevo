#include "source_io.h"

#include <stdio.h>
#include <stdlib.h>

char *nevo_read_text_file(const char *path)
{
    FILE *input = fopen(path, "rb");
    if (!input)
    {
        perror(path);
        return NULL;
    }
    if (fseek(input, 0, SEEK_END) != 0)
    {
        perror(path);
        fclose(input);
        return NULL;
    }
    long size = ftell(input);
    if (size < 0 || fseek(input, 0, SEEK_SET) != 0)
    {
        perror(path);
        fclose(input);
        return NULL;
    }

    char *text = malloc((size_t)size + 1);
    if (!text)
    {
        fputs("Out of memory\n", stderr);
        fclose(input);
        return NULL;
    }
    size_t read = fread(text, 1, (size_t)size, input);
    if (ferror(input))
    {
        perror(path);
        free(text);
        fclose(input);
        return NULL;
    }
    text[read] = '\0';
    fclose(input);
    return text;
}
