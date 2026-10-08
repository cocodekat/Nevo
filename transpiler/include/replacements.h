#ifndef REPLACEMENTS_H
#define REPLACEMENTS_H

typedef struct {
    const char *from; // string to search for
    const char *to;   // string to replace it with
} replacement_t;

static replacement_t replacements[] = {
    { "print(", "printf(" },
    { "get(", "scanf(" },
    { "stop()", "exit(0)" },
    { "exit()", "exit(0)" },
    { "gets(", "fgets(" },
    { "!", ";" },
    { "kaboom()", "exit(0)" },
    { "fart(", "printf(" },
    { "yap(", "printf(" },
    { "fread(", "fgets(" },
    { "fwritef(", "fprintf(" },
    { "freadf(", "fscanf(" },
};

static const size_t num_replacements = sizeof(replacements) / sizeof(replacement_t);

#endif
