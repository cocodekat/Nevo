Nevo v1.4

- Added the optional slang vocabulary: `yap`, `sus`, `nah`, `fr`, `cap`,
  `ish`, `vibe`, `gimme`, `nap`, `bonk`, `glowup`, `ghost`, `yeet`,
  `yoink`, `lockin`, `bet`, `bruh`, `skillissue`, `maincharacter`,
  `sidequest`, and `lore`.
- Made arrays growable so `array.yeet(value)`/`yeet(array, value)` can append
  and `array.yoink()`/`yoink(array)` can remove and return the final value.
- Added `clear()` and raw single-character `keypress()` terminal operations.
- Added text `.length()`, `.contains(text)`, `.trim()`, `.split(delimiter)`,
  and `str(number)` helpers. Split results are indexable text arrays.
- Added `wheel(a, b, ...)` for choosing one supplied value at random.
- Added `if'nt condition { ... }` as a negated-if shorthand.
- Added the deliberately unpredictable `maybe` boolean value.
- Made declarations function-local by default for `num`, `txt`, `bool`,
  `array`, and `file` values.
- Added the `global` declaration prefix for state intentionally shared across
  functions, for example `global num score = 0;`.
- Applied the same storage rules to the Apple Silicon and Windows x86-64 NASM
  backends.

Nevo v1.3

- Added `quit()` and `kaboom()` as aliases that terminate the entire running
  program with exit status 0. They work from `_main()` or any helper function.
- Added compile-time removal directives: `rvar(name);` ends a variable name's
  current lifetime so it can be declared again, and top-level `rfunc(_name);`
  removes an earlier function definition so a later one can replace it.
- Added runtime function replacement using `rfunc(_name);` followed by a
  matching nested replacement definition. Calls use replaceable dispatch slots
  on both ARM64 and Windows x86-64.
- Added short-circuit `&&`/`and` and `||`/`or`, single-`=` equality in
  expressions, and `rand(min, max)` as an alias for `random(min, max)`.
- Added cross-platform terminal input with `input(prompt)`. Text declarations
  retain the entered line and numeric declarations parse signed integers.
- Changed `print(value)` to write its value exactly without an implicit
  newline. Programs now add line breaks explicitly with `print("\n")`.
- Added booleans (`bool`, `true`, `false`) and unary logical negation (`!`).
- Added fixed-length, zero-based numeric arrays with literals, indexed reads
  and writes, and `length()`.
- Added `while`, C-style `for`, `break`, `continue`, and chained `else if`.
- Added cross-platform `sleep(milliseconds)`.
- Added `input(prompt, minimum, maximum)` with retrying integer validation.
- Made `;` mandatory after every simple statement while retaining compact
  one-line source and brace-terminated functions/control blocks.

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
- Added `//` line comments. Semicolons were optional in early v1.3 builds but
  are now mandatory for simple statements.
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
