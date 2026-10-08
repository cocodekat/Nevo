# Nevo v1.2

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
