#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#include <io.h>
#else
#include <unistd.h>
#include <termios.h>
#endif

typedef struct
{
    char *path;
    char *text;
} NevoFile;

static char *copy_text(const char *text)
{
    size_t length = strlen(text);
    char *copy = malloc(length + 1);
    if (!copy)
        abort();
    memcpy(copy, text, length + 1);
    return copy;
}

char *nevo_input(const char *prompt)
{
    if (prompt)
        fputs(prompt, stdout);
    fflush(stdout);

    size_t capacity = 64;
    size_t length = 0;
    char *text = malloc(capacity);
    if (!text)
        abort();

    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF)
    {
        if (ch == '\r')
            continue;
        if (length + 1 >= capacity)
        {
            capacity *= 2;
            text = realloc(text, capacity);
            if (!text)
                abort();
        }
        text[length++] = (char)ch;
    }
    text[length] = '\0';
    return text;
}

char *nevo_keypress(void)
{
    int ch;
#ifdef _WIN32
    ch = _isatty(_fileno(stdin)) ? _getch() : getchar();
#else
    if (isatty(STDIN_FILENO))
    {
        struct termios original, raw;
        if (tcgetattr(STDIN_FILENO, &original) == 0)
        {
            raw = original;
            raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
            tcsetattr(STDIN_FILENO, TCSANOW, &raw);
            ch = getchar();
            tcsetattr(STDIN_FILENO, TCSANOW, &original);
        }
        else
            ch = getchar();
    }
    else
        ch = getchar();
#endif
    char *result = malloc(2);
    if (!result) abort();
    result[0] = ch == EOF ? '\0' : (char)ch;
    result[1] = '\0';
    return result;
}

void nevo_clear(void)
{
    fputs("\033[2J\033[H", stdout);
    fflush(stdout);
}

long long nevo_text_length(const char *text)
{
    return text ? (long long)strlen(text) : 0;
}

long long nevo_text_contains(const char *text, const char *needle)
{
    return text && needle && strstr(text, needle) != NULL;
}

char *nevo_text_trim(const char *text)
{
    if (!text) return copy_text("");
    const unsigned char *start = (const unsigned char *)text;
    while (*start && isspace(*start)) start++;
    const unsigned char *end = start + strlen((const char *)start);
    while (end > start && isspace(end[-1])) end--;
    size_t length = (size_t)(end - start);
    char *result = malloc(length + 1);
    if (!result) abort();
    memcpy(result, start, length);
    result[length] = '\0';
    return result;
}

char *nevo_num_to_text(long long value)
{
    char buffer[32];
    snprintf(buffer, sizeof buffer, "%lld", value);
    return copy_text(buffer);
}

long long nevo_input_num(const char *prompt)
{
    for (;;)
    {
        char *text = nevo_input(prompt);
        char *end = NULL;
        long long value = strtoll(text, &end, 10);
        while (end && isspace((unsigned char)*end)) end++;
        int valid = text[0] && end && !*end;
        free(text);
        if (valid) return value;
        fputs("Invalid number. Try again.\n", stdout);
    }
}

long long nevo_input_num_range(const char *prompt, long long minimum, long long maximum)
{
    for (;;)
    {
        long long value = nevo_input_num(prompt);
        if (value >= minimum && value <= maximum) return value;
        fputs("Number out of range. Try again.\n", stdout);
    }
}

void nevo_sleep_ms(long long milliseconds)
{
    if (milliseconds <= 0) return;
#ifdef _WIN32
    Sleep((DWORD)milliseconds);
#else
    usleep((useconds_t)((unsigned long long)milliseconds * 1000ULL));
#endif
}

typedef struct { long long length; long long capacity; long long *values; } NevoArray;

NevoArray *nevo_array_new(long long length)
{
    if (length < 0) length = 0;
    NevoArray *array = calloc(1, sizeof *array);
    if (!array) abort();
    array->length = length;
    array->capacity = length > 4 ? length : 4;
    array->values = calloc((size_t)array->capacity, sizeof(long long));
    if (!array->values) abort();
    return array;
}

long long nevo_array_get(const NevoArray *array, long long index)
{
    return array && index >= 0 && index < array->length ? array->values[index] : 0;
}

void nevo_array_set(NevoArray *array, long long index, long long value)
{
    if (array && index >= 0 && index < array->length) array->values[index] = value;
}

long long nevo_array_length(const NevoArray *array)
{
    return array ? array->length : 0;
}

void nevo_array_push(NevoArray *array, long long value)
{
    if (!array) return;
    if (array->length >= array->capacity)
    {
        long long capacity = array->capacity > 0 ? array->capacity * 2 : 4;
        long long *values = realloc(array->values, (size_t)capacity * sizeof(long long));
        if (!values) abort();
        array->values = values;
        array->capacity = capacity;
    }
    array->values[array->length++] = value;
}

long long nevo_array_pop(NevoArray *array)
{
    if (!array || array->length <= 0) return 0;
    return array->values[--array->length];
}

long long nevo_clamp(long long value, long long minimum, long long maximum)
{
    if (minimum > maximum)
    {
        long long temporary = minimum;
        minimum = maximum;
        maximum = temporary;
    }
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

void nevo_skillissue(const char *message)
{
    fprintf(stderr, "skill issue: %s\n", message ? message : "");
    exit(1);
}

void nevo_bruh(long long line)
{
    fprintf(stderr, "bruh: reached source line %lld\n", line);
}

void nevo_assert(long long condition, long long line)
{
    if (!condition)
    {
        fprintf(stderr, "bet failed at source line %lld\n", line);
        exit(1);
    }
}

NevoArray *nevo_text_split(const char *text, const char *delimiter)
{
    if (!text) text = "";
    if (!delimiter || !delimiter[0])
    {
        NevoArray *single = nevo_array_new(1);
        single->values[0] = (long long)(intptr_t)copy_text(text);
        return single;
    }

    size_t delimiter_length = strlen(delimiter);
    long long count = 1;
    const char *scan = text;
    while ((scan = strstr(scan, delimiter)) != NULL)
    {
        count++;
        scan += delimiter_length;
    }

    NevoArray *parts = nevo_array_new(count);
    const char *start = text;
    for (long long i = 0; i < count; i++)
    {
        const char *end = i + 1 < count ? strstr(start, delimiter) : start + strlen(start);
        size_t length = (size_t)(end - start);
        char *part = malloc(length + 1);
        if (!part) abort();
        memcpy(part, start, length);
        part[length] = '\0';
        parts->values[i] = (long long)(intptr_t)part;
        start = end + (i + 1 < count ? delimiter_length : 0);
    }
    return parts;
}

static char *read_path(const char *path)
{
    FILE *input = fopen(path, "rb");
    if (!input)
    {
        perror(path);
        return copy_text("");
    }
    if (fseek(input, 0, SEEK_END) != 0)
    {
        perror(path);
        fclose(input);
        return copy_text("");
    }
    long size = ftell(input);
    if (size < 0 || fseek(input, 0, SEEK_SET) != 0)
    {
        perror(path);
        fclose(input);
        return copy_text("");
    }

    char *text = malloc((size_t)size + 1);
    if (!text)
        abort();
    size_t read = fread(text, 1, (size_t)size, input);
    if (ferror(input))
        perror(path);
    text[read] = '\0';
    fclose(input);
    return text;
}

static NevoFile *new_file(const char *path, char *text)
{
    NevoFile *file = malloc(sizeof *file);
    if (!file)
        abort();
    file->path = copy_text(path);
    file->text = text;
    return file;
}

NevoFile *nevo_loadf(const char *path)
{
    return new_file(path, read_path(path));
}

NevoFile *nevo_createf(const char *path)
{
    FILE *output = fopen(path, "wb");
    if (!output)
        perror(path);
    else if (fclose(output) != 0)
        perror(path);
    return new_file(path, copy_text(""));
}

void nevo_print_text(const char *text)
{
    fputs(text, stdout);
}

void nevo_print_file(const NevoFile *file)
{
    nevo_print_text(file->text);
}

void nevo_print_num(long long value)
{
    printf("%lld", value);
}

long long nevo_random_range(long long minimum, long long maximum)
{
    static int seeded = 0;
    if (!seeded)
    {
        srand((unsigned int)time(NULL));
        seeded = 1;
    }
    if (maximum < minimum)
    {
        long long temporary = minimum;
        minimum = maximum;
        maximum = temporary;
    }
    unsigned long long range =
        (unsigned long long)maximum - (unsigned long long)minimum + 1ULL;
    if (range == 0)
        return (long long)(((uint64_t)(unsigned)rand() << 48) ^
                           ((uint64_t)(unsigned)rand() << 32) ^
                           ((uint64_t)(unsigned)rand() << 16) ^
                           (uint64_t)(unsigned)rand());
    uint64_t random_value = ((uint64_t)(unsigned)rand() << 48) ^
                            ((uint64_t)(unsigned)rand() << 32) ^
                            ((uint64_t)(unsigned)rand() << 16) ^
                            (uint64_t)(unsigned)rand();
    return minimum + (long long)(random_value % range);
}

long long nevo_maybe(void)
{
    return nevo_random_range(0, 1);
}

static int range_contains(const char *start, size_t length, const char *needle)
{
    size_t needle_length = strlen(needle);
    if (needle_length == 0)
        return 1;
    if (needle_length > length)
        return 0;
    for (size_t i = 0; i + needle_length <= length; i++)
        if (memcmp(start + i, needle, needle_length) == 0)
            return 1;
    return 0;
}

char *nevo_file_filter(const NevoFile *file, const char *pattern)
{
    const char *text = file->text;
    size_t text_length = strlen(text);
    char *result = malloc(text_length + 1);
    if (!result)
        abort();

    const char *cursor = text;
    const char *end = text + text_length;
    size_t output_length = 0;
    int matched = 0;
    while (cursor < end)
    {
        const char *line_end = memchr(cursor, '\n', (size_t)(end - cursor));
        if (!line_end)
            line_end = end;
        size_t line_length = (size_t)(line_end - cursor);
        if (range_contains(cursor, line_length, pattern))
        {
            if (matched)
                result[output_length++] = '\n';
            memcpy(result + output_length, cursor, line_length);
            output_length += line_length;
            matched = 1;
        }
        cursor = line_end < end ? line_end + 1 : end;
    }
    result[output_length] = '\0';
    return result;
}

char *nevo_file_line(const NevoFile *file, long long number)
{
    const char *text = file->text;
    if (number < 1)
        return copy_text("");

    const char *cursor = text;
    const char *end = text + strlen(text);
    long long current = 1;
    while (cursor < end && current < number)
    {
        const char *newline = memchr(cursor, '\n', (size_t)(end - cursor));
        if (!newline)
            return copy_text("");
        cursor = newline + 1;
        current++;
    }
    if (cursor >= end)
        return copy_text("");

    const char *line_end = memchr(cursor, '\n', (size_t)(end - cursor));
    if (!line_end)
        line_end = end;
    if (line_end > cursor && line_end[-1] == '\r')
        line_end--;
    size_t length = (size_t)(line_end - cursor);
    char *line = malloc(length + 1);
    if (!line)
        abort();
    memcpy(line, cursor, length);
    line[length] = '\0';
    return line;
}

long long nevo_file_count_lines(const NevoFile *file)
{
    const char *text = file->text;
    if (!text[0])
        return 0;
    long long count = 0;
    for (const char *p = text; *p; p++)
        if (*p == '\n')
            count++;
    if (text[strlen(text) - 1] != '\n')
        count++;
    return count;
}

long long nevo_file_count_filter(const NevoFile *file, const char *pattern)
{
    const char *text = file->text;
    const char *cursor = text;
    const char *end = text + strlen(text);
    long long count = 0;
    while (cursor < end)
    {
        const char *line_end = memchr(cursor, '\n', (size_t)(end - cursor));
        if (!line_end)
            line_end = end;
        if (range_contains(cursor, (size_t)(line_end - cursor), pattern))
            count++;
        cursor = line_end < end ? line_end + 1 : end;
    }
    return count;
}

long long nevo_file_count_word(const NevoFile *file, const char *word)
{
    size_t word_length = strlen(word);
    if (word_length == 0)
        return 0;
    long long count = 0;
    const char *cursor = file->text;
    while ((cursor = strstr(cursor, word)) != NULL)
    {
        count++;
        cursor += word_length;
    }
    return count;
}

static int persist(NevoFile *file, const char *text)
{
    FILE *output = fopen(file->path, "wb");
    if (!output)
    {
        perror(file->path);
        return 0;
    }
    size_t length = strlen(text);
    int write_ok = fwrite(text, 1, length, output) == length;
    int close_ok = fclose(output) == 0;
    if (!write_ok || !close_ok)
    {
        perror(file->path);
        return 0;
    }
    return 1;
}

static void replace_contents(NevoFile *file, const char *text)
{
    char *replacement = copy_text(text);
    if (persist(file, replacement))
    {
        free(file->text);
        file->text = replacement;
    }
    else
    {
        free(replacement);
    }
}

void nevo_write_text(NevoFile *target, const char *text)
{
    replace_contents(target, text);
}

void nevo_write_file(NevoFile *target, const NevoFile *source)
{
    replace_contents(target, source->text);
}

static void replace_line(NevoFile *file, long long number, const char *replacement)
{
    if (number < 1)
        return;

    const char *text = file->text;
    const char *end = text + strlen(text);
    const char *line_start = text;
    long long current = 1;
    while (line_start < end && current < number)
    {
        const char *newline = memchr(line_start, '\n', (size_t)(end - line_start));
        if (!newline)
            return;
        line_start = newline + 1;
        current++;
    }
    if (line_start >= end)
        return;

    const char *line_end = memchr(line_start, '\n', (size_t)(end - line_start));
    const char *suffix = line_end ? line_end + 1 : end;
    size_t prefix_length = (size_t)(line_start - text);
    size_t replacement_length = strlen(replacement);
    int needs_newline = replacement_length == 0 || replacement[replacement_length - 1] != '\n';
    size_t suffix_length = (size_t)(end - suffix);
    size_t total = prefix_length + replacement_length + (size_t)needs_newline + suffix_length;
    char *updated = malloc(total + 1);
    if (!updated)
        abort();

    size_t offset = 0;
    memcpy(updated + offset, text, prefix_length);
    offset += prefix_length;
    memcpy(updated + offset, replacement, replacement_length);
    offset += replacement_length;
    if (needs_newline)
        updated[offset++] = '\n';
    memcpy(updated + offset, suffix, suffix_length);
    offset += suffix_length;
    updated[offset] = '\0';

    if (persist(file, updated))
    {
        free(file->text);
        file->text = updated;
    }
    else
    {
        free(updated);
    }
}

void nevo_write_line_text(NevoFile *target, long long number, const char *text)
{
    replace_line(target, number, text);
}

void nevo_write_line_file(NevoFile *target, long long number, const NevoFile *source)
{
    replace_line(target, number, source->text);
}
