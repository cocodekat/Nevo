$ErrorActionPreference = "Stop"
if (Test-Path "C:\msys64\ucrt64\bin\gcc.exe") {
    $env:Path = "C:\msys64\ucrt64\bin;" + $env:Path
}
$ReleaseRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$Runner = Join-Path $PSScriptRoot "run.ps1"
$Fixtures = Join-Path $ReleaseRoot "tests\fixtures"
$TemporaryDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ("nevo-v12-tests-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $TemporaryDirectory | Out-Null

function Assert-Output {
    param([string] $Executable, [string[]] $Expected)
    [string[]] $Actual = & $Executable
    if ($LASTEXITCODE -ne 0) {
        throw "$Executable exited with code $LASTEXITCODE"
    }
    $Difference = @(Compare-Object -ReferenceObject $Expected -DifferenceObject $Actual -SyncWindow 0)
    if ($Difference.Count -ne 0) {
        throw "Unexpected output from $Executable`nExpected: $($Expected -join ' | ')`nActual: $($Actual -join ' | ')"
    }
}

function Build-And-Test {
    param([string] $Name, [string[]] $Sources, [string[]] $Expected)
    $Output = Join-Path $TemporaryDirectory $Name
    & $Runner @Sources -o $Output
    if ($LASTEXITCODE -ne 0) {
        throw "Compilation failed for $Name"
    }
    Assert-Output -Executable "$Output.exe" -Expected $Expected
}

Push-Location $ReleaseRoot
try {
    Build-And-Test -Name "returns" -Sources @("tests\fixtures\return_source.n") -Expected (Get-Content (Join-Path $Fixtures "return_expected.txt"))
    Build-And-Test -Name "includes" -Sources @("tests\fixtures\include_main.n") -Expected (Get-Content (Join-Path $Fixtures "include_expected.txt"))
    Build-And-Test -Name "multiple files" -Sources @(
        "tests\fixtures\file one.n",
        "tests\fixtures\file two.n",
        "tests\fixtures\file three.n"
    ) -Expected (Get-Content (Join-Path $Fixtures "multifile_expected.txt"))
    Build-And-Test -Name "eight parameters" -Sources @("tests\fixtures\windows_abi_source.n") -Expected @("36")
    Build-And-Test -Name "language features" -Sources @("tests\fixtures\windows_features_source.n") -Expected @(
        "-7",
        "7",
        "loop",
        "loop",
        "numeric comparison",
        "string comparison",
        "4",
        "4"
    )

    Copy-Item (Join-Path $Fixtures "target.txt") (Join-Path $Fixtures "work-target.txt") -Force
    Build-And-Test -Name "file writes" -Sources @("tests\fixtures\write_source.n") -Expected (Get-Content (Join-Path $Fixtures "write_expected.txt"))
    if ([System.IO.File]::ReadAllText((Join-Path $Fixtures "work-created.txt")) -ne "created") {
        throw "createf/writef produced unexpected content"
    }
    $ExpectedTarget = "hello`nS1`nS2`nthree"
    if ([System.IO.File]::ReadAllText((Join-Path $Fixtures "work-target.txt")) -ne $ExpectedTarget) {
        throw "line replacement produced unexpected content"
    }

    Write-Host "All Nevo v1.2 Windows x86-64 tests passed."
} finally {
    Pop-Location
    Remove-Item (Join-Path $Fixtures "work-target.txt") -Force -ErrorAction SilentlyContinue
    Remove-Item (Join-Path $Fixtures "work-created.txt") -Force -ErrorAction SilentlyContinue
    Remove-Item $TemporaryDirectory -Recurse -Force -ErrorAction SilentlyContinue
}
