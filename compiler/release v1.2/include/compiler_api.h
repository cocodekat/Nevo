#ifndef NEVO_COMPILER_API_H
#define NEVO_COMPILER_API_H

int nevo_format_files(int source_count, char **source_paths,
                      const char *output_path);
int nevo_codegen_file(const char *json_path, const char *assembly_path);

#endif
