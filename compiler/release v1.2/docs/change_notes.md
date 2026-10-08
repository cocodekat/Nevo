Nevo v1.2

Windows x86-64 support:

- Added a backend that emits NASM `win64` assembly.
- Added `run.bat` and a PowerShell build driver using NASM and MinGW-w64 GCC.
- Kept the parser, type validation, imports, and language syntax shared with macOS.
- Implemented Microsoft x64 argument passing for up to eight parameters.
- Reused the file runtime for loading, querying, creating, and writing files.

- Added the read-only `file` primitive and `loadf(path)`.
- Added literal-substring line filtering with `file.filter(pattern)`.
- Added one-based line lookup with `file.line(number)`.
- Added `file.count(number)` for total lines, `file.count(file.filter(pattern))`
  for matching lines, and `file.count(word(text))` for non-overlapping text
  occurrence counts.
- Added support for optional semicolons and `//` line comments.
- File values now retain their source path so mutations update both disk and
  the in-memory value used by later queries.
- Added `writef(file, value)` for complete replacement from a file, query
  result, or string. `write` is accepted as an alias.
- Added `writef(file.line(number), value)` for one-based line replacement,
  including multi-line replacement from another file.
- Added `createf(path)` for creating or truncating files.
- Added typed functions: `num _name()`, `txt _name()`, and `file _name()`.
- Added function-call expressions such as `num x = _name()` and native
  argument passing for up to eight parameters on both targets.
- Added typed `return value;` and untyped `return;`, with return-type and
  guaranteed-return validation.
- Added plain assignment (`x = value`) and optional parentheses for zero-arg
  jumps (`jump _name;`).
- Added multi-source compilation with `./run.sh source1.n source2.n -o out`,
  including quoted source and output paths containing spaces.
- Source units are merged before validation, allowing cross-file function calls
  and globals while enforcing exactly one `_main()` and unique function names.
- Preserved the legacy `./run.sh input.n out` form.
- Split command-line handling, source loading, frontend logic, backend logic,
  and the file runtime into separate source/header modules.
- Added recursive `#include "path.n"` discovery with paths relative to the
  including source, cycle handling, and canonical-path deduplication.
- Fixed plain assignment to an undeclared numeric name so its global storage is
  emitted instead of leaving an undefined `_gv_*` linker symbol.
- Assignments to parameters remain local and no longer risk creating a global
  symbol during the pre-scan.
- Added ordinary function-call statements such as `_add(1, 2);`. These return
  to the following statement, unlike the existing tail-call `jump` operation.
- The existing `clang -arch arm64` macOS target remains available unchanged;
  Windows builds use NASM `win64` objects and MinGW-w64 GCC for final linking.

File values are text loaded into memory and retain their backing path. Filter
patterns are literal substrings, not regular expressions.
