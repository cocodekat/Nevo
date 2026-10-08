@echo off
setlocal

set "SOURCE_ROOT=%~dp0\..\.."
if "%~1"=="" (
    set "OUTPUT_DIR=%SOURCE_ROOT%\build\windows"
) else (
    set "OUTPUT_DIR=%~1"
)

if not exist "%OUTPUT_DIR%\Modes" mkdir "%OUTPUT_DIR%\Modes"
if not exist "%OUTPUT_DIR%\Libraries" mkdir "%OUTPUT_DIR%\Libraries"

cl /nologo /I "%SOURCE_ROOT%\include" "%SOURCE_ROOT%\src\core\nevo.c" /Fe:"%OUTPUT_DIR%\nevo.exe" || exit /b 1
cl /nologo /I "%SOURCE_ROOT%\include" "%SOURCE_ROOT%\src\modes\n.c" "%SOURCE_ROOT%\src\core\auto_var.c" "%SOURCE_ROOT%\src\core\ban_list.c" /Fe:"%OUTPUT_DIR%\Modes\n.exe" || exit /b 1
xcopy /E /I /Y "%SOURCE_ROOT%\libraries" "%OUTPUT_DIR%\Libraries" >nul || exit /b 1

echo Windows transpiler build created at: %OUTPUT_DIR%
