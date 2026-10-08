param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]] $CompilerArguments
)

$ErrorActionPreference = "Stop"
$ReleaseRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))

function Show-Usage {
    Write-Error "Usage: .\run.bat <source.n> [source.n ...] -o <output>"
}

foreach ($Tool in @("gcc", "nasm")) {
    if (-not (Get-Command $Tool -ErrorAction SilentlyContinue)) {
        throw "Required tool '$Tool' was not found on PATH. Install MinGW-w64 GCC and NASM."
    }
}

$Sources = [System.Collections.Generic.List[string]]::new()
$Output = $null
if ($CompilerArguments.Count -eq 2 -and $CompilerArguments[0] -ne "-o" -and $CompilerArguments[1] -ne "-o") {
    $Sources.Add($CompilerArguments[0])
    $Output = $CompilerArguments[1]
} else {
    for ($Index = 0; $Index -lt $CompilerArguments.Count; $Index++) {
        if ($CompilerArguments[$Index] -eq "-o") {
            if ($null -ne $Output -or $Index + 1 -ge $CompilerArguments.Count) {
                Show-Usage
                exit 1
            }
            $Index++
            $Output = $CompilerArguments[$Index]
        } else {
            $Sources.Add($CompilerArguments[$Index])
        }
    }
}

if ($Sources.Count -eq 0 -or [string]::IsNullOrWhiteSpace($Output)) {
    Show-Usage
    exit 1
}

[string[]] $ResolvedSources = foreach ($Source in $Sources) {
    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) {
        throw "Source file not found: $Source"
    }
    (Resolve-Path -LiteralPath $Source).Path
}

$OutputPath = [System.IO.Path]::GetFullPath($Output)
$ExecutablePath = if ($OutputPath.EndsWith(".exe", [System.StringComparison]::OrdinalIgnoreCase)) {
    $OutputPath
} else {
    "$OutputPath.exe"
}
$AssemblyPath = "$OutputPath.asm"
$OutputDirectory = Split-Path -Parent $ExecutablePath
if ($OutputDirectory -and -not (Test-Path -LiteralPath $OutputDirectory)) {
    New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
}

$TemporaryDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ("nevo-v12-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $TemporaryDirectory | Out-Null

try {
    $Formatter = Join-Path $TemporaryDirectory "nevo-format.exe"
    $Codegen = Join-Path $TemporaryDirectory "nevo-codegen-windows.exe"
    $Ast = Join-Path $TemporaryDirectory "program.json"
    $Object = Join-Path $TemporaryDirectory "program.obj"
    $Include = Join-Path $ReleaseRoot "include"
    $SourceRoot = Join-Path $ReleaseRoot "src"

    & gcc -w -DNEVO_LIBRARY_BUILD "-I$Include" `
        (Join-Path $SourceRoot "format_main.c") `
        (Join-Path $SourceRoot "format.c") `
        (Join-Path $SourceRoot "source_io.c") `
        (Join-Path $SourceRoot "source_graph.c") `
        -o $Formatter
    if ($LASTEXITCODE -ne 0) { throw "Failed to build the Nevo formatter." }

    & $Formatter @ResolvedSources -o $Ast
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    & gcc -w -DNEVO_LIBRARY_BUILD -DNEVO_TARGET_WINDOWS_X64 "-I$Include" `
        (Join-Path $SourceRoot "codegen_main.c") `
        (Join-Path $SourceRoot "codegen.c") `
        (Join-Path $SourceRoot "source_io.c") `
        -o $Codegen
    if ($LASTEXITCODE -ne 0) { throw "Failed to build the Windows code generator." }

    & $Codegen $Ast $AssemblyPath
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    & nasm -f win64 $AssemblyPath -o $Object
    if ($LASTEXITCODE -ne 0) { throw "NASM failed to assemble the generated program." }

    & gcc $Object (Join-Path $SourceRoot "file_runtime.c") "-Wl,--subsystem,console" -o $ExecutablePath
    if ($LASTEXITCODE -ne 0) { throw "MinGW-w64 GCC failed to link the generated program." }

    Write-Host "Created $ExecutablePath"
    Write-Host "Generated assembly: $AssemblyPath"
} finally {
    Remove-Item -LiteralPath $TemporaryDirectory -Recurse -Force -ErrorAction SilentlyContinue
}
