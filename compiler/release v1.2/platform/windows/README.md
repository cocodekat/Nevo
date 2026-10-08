# Windows x86-64 build

The Windows backend emits 64-bit NASM assembly in the `win64` object format and links it into a console executable with MinGW-w64 GCC.

## Requirements

- 64-bit Windows 10 or newer
- NASM available as `nasm` on `PATH`
- MinGW-w64 GCC available as `gcc` on `PATH`
- Windows PowerShell 5.1 or PowerShell 7

MSYS2's UCRT64 environment is a supported setup. Install its GCC and NASM
packages with:

```sh
pacman -Syu
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-nasm
```

When installed in the standard `C:\msys64` location, `run.bat` automatically
adds `C:\msys64\ucrt64\bin` to its temporary `PATH`.

## Compile

From the release directory:

```bat
run.bat main.n helpers.n -o out
```

Quoted filenames are supported:

```bat
run.bat "file one.n" "file two.n" -o "my program"
```

This creates `out.exe` and keeps the generated NASM source as `out.asm`. Imports and explicitly listed source files use the same frontend as the macOS release, so the Nevo language syntax is identical on both platforms.

Run the Windows integration suite with:

```powershell
powershell -ExecutionPolicy Bypass -File platform\windows\test.ps1
```

The suite compiles and runs function returns, eight-parameter calls, imports,
multiple files with spaces in their names, and file creation/query/write cases.
