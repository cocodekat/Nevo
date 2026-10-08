# Nevo v1.4

Build a program from one or more source files:

```bash
./run.sh examples/input.n build/out
./run.sh main.n helpers.n -o out
```

On Windows x86-64, install NASM and MinGW-w64 GCC, then use the same arguments:

```bat
run.bat main.n helpers.n -o out
```

The Windows command produces `out.exe` and `out.asm`. The generated assembly uses NASM's `win64` format and the Microsoft x64 calling convention. See [`platform/windows/README.md`](platform/windows/README.md) for setup details.

Use `quit()` or `kaboom()` to stop a running program immediately.

Use `rvar(name);` to end a variable name's compile-time lifetime so it may be
declared again, and top-level `rfunc(_name);` to remove a previous function
definition before replacing it. See the manual for examples and ordering rules.

Inside a function, `rfunc(_name);` followed by a matching function definition
installs that definition at runtime. Conditions support `&&`/`and`, `||`/`or`,
and either `=` or `==` for equality. `rand(min, max)` is an alias for
`random(min, max)`.

Read terminal input at runtime with `txt name = input("name: ");`. A `num`
declaration parses the entered line as a signed integer.

`print(value)` writes exactly the value and does not append a newline. Use
`print("\n")` wherever a line break is wanted.

v1.3 also supports `bool`, `true`/`false`, `if !value`, numeric arrays,
`while`, C-style `for`, `break`, `continue`, chained `else if`,
`sleep(milliseconds)`, and range-validated numeric input.

Every simple statement must end with `;`. Function and control-block closing
braces do not require a semicolon, and multiple statements may share one line.

In v1.4, declarations are local to their function by default. Prefix a
declaration with `global` when helpers need to share it:

```python
_main() {
    global num gold = 0;
    num room = 1;
    _earnGold();
}

_earnGold() {
    gold = gold + 5;
    return;
}
```

Terminal and text utilities include `clear()`, `keypress()`, `.length()`,
`.contains(...)`, `.trim()`, `.split(...)`, and `str(number)`. Chance-driven
programs can use `wheel(a, b, ...)`, the random boolean `maybe`, and the
negated-condition shorthand `if'nt condition { ... }`.

Nevo also ships with an optional slang vocabulary: `yap`, `sus`, `nah`,
`fr`, `cap`, `ish`, `vibe`, `gimme`, `nap`, `bonk`, `glowup`, `ghost`,
`yeet`, `yoink`, `lockin`, `bet`, `bruh`, `skillissue`, `maincharacter`,
`sidequest`, and `lore`. The ordinary spellings remain fully supported.

## Layout

- `src/` — lexer, parser, validation, code generation, and file runtime.
- `include/` — interfaces shared by compiler modules.
- `tests/` — runnable test scripts.
- `tests/fixtures/` — source programs and expected test data.
- `examples/` — a small example program and its input file.
- `docs/` — language manual and change notes.
- `platform/windows/` — Windows x86-64 NASM build runner and documentation.
- `build/` — local generated artifacts; safe to recreate.

See [the manual](docs/manual.md) for language syntax and build details.

## Complete example

[`examples/all_features.n`](examples/all_features.n) is an interactive dungeon
tour of the language. It imports [`examples/showcase_helpers.n`](examples/showcase_helpers.n),
reads [`examples/showcase_data.txt`](examples/showcase_data.txt), and demonstrates
the complete v1.4 syntax in one program:

```bash
./run.sh examples/all_features.n -o build/all-features
./build/all-features
```
