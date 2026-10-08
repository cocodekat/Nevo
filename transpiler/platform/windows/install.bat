@echo off
setlocal

set TARGET_DIR=C:\nevo
set "SOURCE_ROOT=%~dp0\..\.."

set "ZIP_PATH=%TARGET_DIR%\tcc.zip"
mkdir C:\nevo\Libraries\Images
mkdir C:\nevo\Modes

set URL=https://download.savannah.gnu.org/releases/tinycc/tcc-0.9.26-win64-bin.zip

echo Installing tcc... (This might take a moment)

powershell -Command "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri '%URL%' -OutFile '%ZIP_PATH%'"

:: Extract the zip
set "EXTRACT_PATH=%TARGET_DIR%\tcc"
echo Extracting TCC...
powershell -Command "Expand-Archive -Path '%ZIP_PATH%' -DestinationPath '%EXTRACT_PATH%' -Force"

:: Delete the zip
del "%ZIP_PATH%"

REM Compile directly from the platform-independent source tree.
C:\nevo\tcc\tcc\tcc.exe "%SOURCE_ROOT%\src\core\nevo.c" -o C:\nevo\nevo.exe
C:\nevo\tcc\tcc\tcc.exe -I"%SOURCE_ROOT%\include" "%SOURCE_ROOT%\src\modes\n.c" "%SOURCE_ROOT%\src\core\auto_var.c" "%SOURCE_ROOT%\src\core\ban_list.c" -o C:\nevo\Modes\n.exe

REM Copy runtime libraries; leave the checked-out source tree intact.
xcopy /E /I /Y "%SOURCE_ROOT%\libraries" "C:\nevo\Libraries" >nul

echo done! Everything you need is inside of C:\nevo Compile files using nevo.exe -flag input_file"
pause
