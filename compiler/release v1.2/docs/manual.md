# Nevo v1.2 file support

Files are read-only text values loaded into memory:

```python
file f = loadf("file1.json");
print(f);
```

Queries:

```python
print(f.filter("error"));
print(f.line(1));
print(f.count(1));
print(f.count(f.filter("error")));
print(f.count(word("error")));
```

- `filter` performs a case-sensitive literal substring match and returns all
  matching lines.
- `line` uses one-based indexing and returns an empty line when out of range.
- `count(number)` returns the total number of lines; the numeric argument is
  accepted as the line-count overload marker.
- `count(filter(...))` returns the number of matching lines.
- `count(word(...))` returns non-overlapping occurrences in the full file.
- Loading reports an operating-system error and produces an empty file value
  if the file cannot be read.
## Creating and writing files

`createf` creates or truncates a file and returns an empty `file` value:

```python
file output = createf("output.txt");
```

`writef` immediately updates both the file on disk and its in-memory value:

```python
file first = loadf("first.txt");
file second = loadf("second.txt");

writef(first, second);          // replace with another complete file
writef(first, second.line(2));  // replace with text from one line
writef(first, "hello");         // replace with literal text

writef(first.line(2), second);  // replace one line with a complete file
writef(first.line(2), "hello"); // replace one line with literal text
```

`write(...)` is an alias for `writef(...)`. Line replacements use one-based
indexes. A replacement is terminated with a newline when necessary, preventing
it from merging with the following line. An out-of-range line target leaves the
file unchanged.

Build one or more source files with:

```bash
./run.sh main.n helpers.n files.n -o out
./run.sh 'main file.n' 'helper file.n' -o 'my program'
```

All listed sources form one program and share the same function and global
variable namespace. The combined program must contain exactly one `_main()`;
duplicate function definitions are rejected. For compatibility, the previous
single-source form `./run.sh input.n out` is still supported. On Windows, use
`run.bat main.n helpers.n -o out`; it produces `out.exe` and NASM source in
`out.asm`.

Source files may also include dependencies directly:

```python
#include "add.n"
```

Include paths are resolved relative to the file containing the directive.
Includes are recursive, cycles are safe, and canonical-path deduplication means
it is valid to both include a file and list it on the `run.sh` command line.

An assignment to a previously undeclared name, such as `result = a + b`,
creates an implicit global numeric variable. Assignments to function parameters
remain local to that function call.

The compiler is separated by responsibility:

- `format_main.c` handles the frontend command line.
- `format.c` contains lexing, parsing, validation, and AST emission.
- `source_io.c` provides source and AST loading shared by both stages.
- `source_graph.c` resolves recursive `#include` dependencies and deduplicates
  compilation units.
- `codegen_main.c` handles the backend command line.
- `codegen.c` emits Apple Silicon assembly by default and Windows x86-64 NASM
  when built with `NEVO_TARGET_WINDOWS_X64`.
- `file_runtime.c` implements runtime file values and operations.
- `compiler_api.h`, `source_io.h`, and `source_graph.h` define module boundaries.

## Function returns

Declare a return type before a function name, then call the function anywhere
an expression of that type is accepted:

```python
_main() {
    num number = _number();
    txt message = _message();
    file input = _input();
}

num _number() {
    return 5;
}

txt _message() {
    return "hello";
}

file _input() {
    return loadf("input.txt");
}
```

Typed functions must return their declared type on every path. Untyped
functions do not return a value and use `return;` when they need to exit early.
Calls support up to eight arguments. The backend follows the native calling
convention of the selected platform. Existing tail jumps remain available, and
zero-argument jumps may be written as either `jump _name;` or `jump _name();`.

Use an ordinary call statement when execution should return to the following
line:

```python
_main() {
    _add(1, 2);
    print(result);
}

_add(a, b) {
    result = a + b;
    return;
}
```

`_add(1, 2);` emits a normal call and resumes in `_main`. In contrast,
`jump _add(1, 2);` remains a tail jump and does not resume after the jump.
