# Nevo v1.3 language manual

## Statement terminators

Every declaration, assignment, call, `print`, `return`, `break`, and
`continue` statement must end with `;`. Function definitions and control
blocks end with `}` and do not take a trailing semicolon. Source layout is not
significant, so compact one-line programs are valid:

```python
_main(){num x = 5; print(x); print("\n");}
```

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

## Stopping a program

Use either `quit()` or `kaboom()` to terminate the entire program immediately.
Both forms are aliases and accept no arguments:

```python
_main() {
    print("stopping now");
    quit();
    print("this does not run");
}
```

## Compile-time removal

`rvar(name);` ends a variable name's current lifetime. It emits no runtime
instruction, so it is useful for making the name available to a later
declaration in the same function:

```python
_main() {
    num x = 5;
    print(x);
    rvar(x);
    num x = 6;
    print(x);
}
```

`rfunc(_name);` is a top-level directive that removes an earlier function
definition. A later definition with that name replaces it. It must appear
after the definition it removes, including when sources are passed as separate
files in build order:

```python
num _value() { return 1; }
rfunc(_value);
num _value() { return 2; }
```

Both directives are compile-time operations. They do not dynamically unload
machine code or free live runtime values.

### Runtime function replacement

Inside a function, `rfunc(_name);` has a different meaning. The matching
definition immediately following it is compiled as a replacement, and is
installed when execution reaches the directive:

```python
num _getVersion() {
    return 2;
}

_main() {
    print(_getVersion());

    rfunc(_getVersion);
    num _getVersion() {
        return 1;
    }

    print(_getVersion());
}
```

This prints `2` and then `1`. A replacement must keep the original function's
return type and number of parameters. Its body is an independent function and
does not capture `scoped` variables from the surrounding function.

The compact form is also accepted:

```python
rfunc num _getVersion() {
    return 1;
}
```

### Logical conditions and random numbers

Logical AND may be written as `&&` or `and`. Logical OR may be written as `||`
or `or`. They short-circuit, and AND has higher precedence than OR. Within an
expression, both `=` and `==` compare for equality:

```python
if a = 1 && b = 2 {
    print("matched");
}
```

`rand(min, max)` is an alias for `random(min, max)`. Both include the minimum
and maximum values:

```python
num dice = rand(1, 6);
```

## Terminal input

`input(prompt)` prints its prompt without adding a newline, waits for one line
of terminal input, and removes the final line ending:

```python
_main() {
    txt name = input("name: ");
    num age = input("age: ");
    print(name);
    print(age);
}
```

In a `txt` declaration, the entered line is stored as text. In a `num`
declaration, it is parsed as a signed decimal integer. Invalid or empty numeric
input becomes `0`, and end-of-file produces an empty string or `0`.

## Exact printing

`print(value)` writes exactly the supplied value. It does not automatically
add a space or newline. Escape sequences in strings are supported, including
`\n` for a line break:

```python
_main() {
    txt name = input("name: ");
    num score = 7;
    print("Hello, ");
    print(name);
    print("! Your score is ");
    print(score);
    print(".\n");
}
```

## Booleans and control flow

Booleans use `bool`, `true`, and `false`. Conditions accept boolean or numeric
expressions, and `!` negates a condition:

```python
bool running = true;
if running { print("running\n"); }
if !running { print("stopped\n"); }
```

`else if`, `while`, and C-style `for` loops are supported. `break` exits the
nearest loop and `continue` starts its next iteration:

```python
for num i = 0; i < 10; i = i + 1 {
    if i = 3 { continue; }
    if i = 8 { break; }
}

while running {
    running = false;
}
```

Pause execution with `sleep(milliseconds)`.

## Arrays

Arrays currently contain numeric or boolean values, use zero-based indexing,
and have a fixed length:

```python
array scores = [10, 20, 30];
scores[1] = 25;
print(scores[1]);
print(scores.length());
```

Reading outside the array returns `0`; an out-of-range write is ignored.

## Validated numeric input

Give numeric input a minimum and maximum to keep prompting until the user
enters a valid signed integer in range:

```python
num age = input("age: ", 1, 120);
```

Invalid input prints an explanation and repeats the original prompt.
