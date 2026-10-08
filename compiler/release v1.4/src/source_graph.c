#include "source_graph.h"
#include "source_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#endif

static char *canonical_path(const char *path)
{
#ifdef _WIN32
    char absolute[_MAX_PATH];
    if (!_fullpath(absolute, path, sizeof absolute))
        return NULL;
    return strdup(absolute);
#else
    return realpath(path, NULL);
#endif
}

static int is_absolute_path(const char *path)
{
#ifdef _WIN32
    return (path[0] && path[1] == ':') ||
           ((path[0] == '\\' || path[0] == '/') &&
            (path[1] == '\\' || path[1] == '/'));
#else
    return path[0] == '/';
#endif
}

static int contains_path(const NevoSourceList *sources, const char *path)
{
    for (int i = 0; i < sources->count; i++)
        if (!strcmp(sources->paths[i], path))
            return 1;
    return 0;
}

static char *resolve_include(const char *including_path,
                             const char *included_path)
{
    if (is_absolute_path(included_path))
        return strdup(included_path);
    const char *slash = strrchr(including_path, '/');
#ifdef _WIN32
    const char *backslash = strrchr(including_path, '\\');
    if (!slash || (backslash && backslash > slash))
        slash = backslash;
#endif
    size_t directory_length = slash ? (size_t)(slash - including_path) : 1;
    const char *directory = slash ? including_path : ".";
    size_t included_length = strlen(included_path);
    char *joined = malloc(directory_length + 1 + included_length + 1);
    if (!joined)
        return NULL;
    memcpy(joined, directory, directory_length);
    joined[directory_length] =
#ifdef _WIN32
        '\\';
#else
        '/';
#endif
    memcpy(joined + directory_length + 1, included_path, included_length + 1);
    return joined;
}

static int collect_one(const char *path, NevoSourceList *sources)
{
    char *canonical = canonical_path(path);
    if (!canonical)
    {
        perror(path);
        return 0;
    }
    if (contains_path(sources, canonical))
    {
        free(canonical);
        return 1;
    }

    sources->paths = realloc(sources->paths,
                             sizeof(char *) * (sources->count + 1));
    if (!sources->paths)
    {
        free(canonical);
        fputs("Out of memory\n", stderr);
        return 0;
    }
    sources->paths[sources->count++] = canonical;

    char *text = nevo_read_text_file(canonical);
    if (!text)
        return 0;
    const char *line = text;
    int line_number = 1;
    while (*line)
    {
        const char *line_end = strchr(line, '\n');
        if (!line_end)
            line_end = line + strlen(line);
        const char *cursor = line;
        while (cursor < line_end && (*cursor == ' ' || *cursor == '\t'))
            cursor++;
        int include_length = 0;
        const char *directive_name = NULL;
        if ((size_t)(line_end - cursor) >= 8 &&
            !strncmp(cursor, "#include", 8) &&
            (cursor + 8 == line_end || cursor[8] == ' ' || cursor[8] == '\t'))
        {
            include_length = 8;
            directive_name = "#include";
        }
        else if ((size_t)(line_end - cursor) >= 4 &&
                 !strncmp(cursor, "lore", 4) &&
                 (cursor + 4 == line_end || cursor[4] == ' ' || cursor[4] == '\t'))
        {
            include_length = 4;
            directive_name = "lore";
        }
        if (include_length)
        {
            cursor += include_length;
            while (cursor < line_end && (*cursor == ' ' || *cursor == '\t'))
                cursor++;
            if (cursor == line_end || *cursor != '"')
            {
                fprintf(stderr, "%s:%d: expected quoted path after %s\n",
                        canonical, line_number, directive_name);
                free(text);
                return 0;
            }
            const char *start = ++cursor;
            while (cursor < line_end && *cursor != '"')
                cursor++;
            if (cursor == line_end)
            {
                fprintf(stderr, "%s:%d: unterminated %s path\n",
                        canonical, line_number, directive_name);
                free(text);
                return 0;
            }
            size_t length = (size_t)(cursor - start);
            char *included = malloc(length + 1);
            if (!included)
            {
                free(text);
                return 0;
            }
            memcpy(included, start, length);
            included[length] = '\0';
            char *resolved = resolve_include(canonical, included);
            free(included);
            if (!resolved || !collect_one(resolved, sources))
            {
                free(resolved);
                free(text);
                return 0;
            }
            free(resolved);
        }
        if (!*line_end)
            break;
        line = line_end + 1;
        line_number++;
    }
    free(text);
    return 1;
}

int nevo_collect_sources(int root_count, char **root_paths,
                         NevoSourceList *sources)
{
    sources->paths = NULL;
    sources->count = 0;
    for (int i = 0; i < root_count; i++)
        if (!collect_one(root_paths[i], sources))
            return 0;
    return 1;
}

void nevo_free_source_list(NevoSourceList *sources)
{
    for (int i = 0; i < sources->count; i++)
        free(sources->paths[i]);
    free(sources->paths);
    sources->paths = NULL;
    sources->count = 0;
}
