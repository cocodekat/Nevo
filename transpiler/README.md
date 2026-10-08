# Nevo transpiler

The transpiler is kept separate from the ARM64 compiler in `../compiler` so both implementations can evolve independently.

## Layout

- `src/core/` — launcher and shared transpiler support code.
- `src/modes/` — language-mode implementations, such as `n.c`.
- `include/` — shared headers used by the C sources.
- `libraries/` — runtime headers and bundled libraries copied into an installation.
- `platform/macos/` — macOS build and installation scripts.
- `platform/windows/` — Windows build and installation scripts.
- `tests/fixtures/` — current input fixtures; `tests/legacy/` preserves older tests.
- `build/` — local build output (not source).
- `archive/` — historical releases, installers, and source snapshots.

## Build

On macOS, create a local distributable with:

```sh
bash platform/macos/build.sh
```

It produces `build/macos/nevo`, `build/macos/Modes/n`, and the required `Libraries/` tree. To install to the default `~/nevo` location, run:

```sh
bash platform/macos/install.sh
```

For Windows, `platform/windows/build.bat` builds with the Visual C compiler, while `platform/windows/install.bat` retains the TCC-based installer path. Both consume the same sources from `src/`, headers from `include/`, and runtime files from `libraries/`.
