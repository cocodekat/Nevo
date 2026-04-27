#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define LINE_MAX_LEN 1024

char current_func[64] = "";

/* Trim leading whitespace */
char *ltrim(char *s)
{
    while (isspace(*s))
        s++;
    return s;
}

/* Check if line starts with keyword */
int starts_with(const char *line, const char *kw)
{
    return strncmp(line, kw, strlen(kw)) == 0;
}

/* Extract function name from a line like:
   name(params) {
   or func name(params) {
*/
void parse_func_name(char *line)
{
    char *p = ltrim(line);

    // If starts with 'func ', skip it
    if (starts_with(p, "func "))
    {
        p += 5; // skip 'func '
    }

    // Extract function name until '(' or whitespace
    int i = 0;
    while (*p && *p != '(' && !isspace(*p) && i < 63)
    {
        current_func[i++] = *p++;
    }
    current_func[i] = '\0';
}

/* Detect function definitions even without 'func' */
int is_func_def(char *line)
{
    char *t = ltrim(line);

    // Skip empty lines or lines starting with non-letters/underscore
    if (!isalpha(t[0]) && t[0] != '_')
        return 0;

    // Look for '(' in the line
    char *paren = strchr(t, '(');
    if (!paren)
        return 0;

    // Check that there is a '{' after ')'
    char *brace = strchr(t, '{');
    if (!brace)
        return 0;

    return 1;
}

/* Replace $x with func_var_x or x */
void replace_var_refs(char *line, FILE *out)
{
    for (int i = 0; line[i]; i++)
    {
        if (line[i] == '$')
        {
            i++;
            char var[64];
            int j = 0;

            while (isalnum(line[i]) || line[i] == '_')
            {
                var[j++] = line[i++];
            }
            var[j] = '\0';
            i--;

            if (current_func[0])
            {
                fprintf(out, "%s_var_%s", current_func, var);
            }
            else
            {
                fprintf(out, "%s", var);
            }
        }
        else
        {
            fputc(line[i], out);
        }
    }
}

/* Handle scoped var while preserving indentation */
void transpile_line(char *line, FILE *out)
{
    // Count leading spaces/tabs
    int indent_len = 0;
    while (line[indent_len] == ' ' || line[indent_len] == '\t')
        indent_len++;

    char *t = line + indent_len;

    if (starts_with(t, "scoped num "))
    {
        char *rest = t + 11; // after "scoped num "

        // Print original indentation
        for (int i = 0; i < indent_len; i++)
            fputc(line[i], out);

        if (current_func[0])
        {
            fprintf(out, "num %s_var_", current_func);
            replace_var_refs(rest, out);
        }
        else
        {
            fprintf(out, "num ");
            replace_var_refs(rest, out);
        }
        fputc('\n', out);
        return;
    }
    if (starts_with(t, "jump "))
    {
        char *rest = t + 5; // skip "bl "

        // preserve indentation
        for (int i = 0; i < indent_len; i++)
            fputc(line[i], out);

        fprintf(out, "bl ");
        replace_var_refs(rest, out);
        fputc('\n', out);
        return;
    }

    // Print line with $ replaced (with indentation preserved)
    for (int i = 0; i < indent_len; i++)
        fputc(line[i], out);
    replace_var_refs(line + indent_len, out);
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        printf("Usage: %s <input> <output>\n", argv[0]);
        return 1;
    }

    FILE *in = fopen(argv[1], "r");
    FILE *out = fopen(argv[2], "w");

    if (!in || !out)
    {
        printf("File error\n");
        return 1;
    }

    char line[LINE_MAX_LEN];

    while (fgets(line, sizeof(line), in))
    {
        char *t = ltrim(line);

        // Detect function definition (with or without 'func')
        if (starts_with(t, "func ") || is_func_def(t))
        {
            parse_func_name(t);
            fputs(line, out);
            continue;
        }

        if (starts_with(t, "}"))
        {
            current_func[0] = '\0';
            fputs(line, out);
            continue;
        }

        transpile_line(line, out);
    }

    fclose(in);
    fclose(out);
    return 0;
}
