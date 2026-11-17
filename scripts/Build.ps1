[CmdletBinding()]
param([string]$Clang, [string]$Linker, [switch]$SkipTests)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$build = Join-Path $repo 'build'
New-Item -ItemType Directory -Path $build -Force | Out-Null
if (-not $Clang) {
    $cmd = Get-Command clang.exe -ErrorAction SilentlyContinue
    if ($cmd) { $Clang = $cmd.Source }
    if (-not $Clang) {
        $editors = Join-Path $env:ProgramFiles 'Unity\Hub\Editor'
        $Clang = Get-ChildItem "$editors\*\Editor\Data\PlaybackEngines\AndroidPlayer\NDK\toolchains\llvm\prebuilt\windows-x86_64\bin\clang.exe" -ErrorAction SilentlyContinue |
            Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
    }
}
if (-not $Clang -or -not (Test-Path -LiteralPath $Clang)) { throw 'Clang required. Pass -Clang with its full path.' }
if (-not $Linker) { $Linker = Join-Path (Split-Path $Clang) 'ld.lld.exe' }
if (-not (Test-Path -LiteralPath $Linker)) { throw 'LLVM ld.lld.exe required. Pass -Linker with its full path.' }
$obj = Join-Path $build 'pip.obj'
$dll = Join-Path $build 'PIP.Core.dll'
& $Clang --target=x86_64-pc-windows-msvc -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-stack-protector -c (Join-Path $repo 'src\pip.c') -o $obj
if ($LASTEXITCODE) { throw 'C compilation failed.' }
& $Linker -flavor link /dll /noentry /nodefaultlib /machine:x64 "/out:$dll" $obj
if ($LASTEXITCODE) { throw 'DLL linking failed.' }
Write-Output "Built $dll (native x64 policy engine; Build-Loader.ps1 builds the diagnostic loader)."
if (-not $SkipTests) {
    & dotnet run --project (Join-Path $repo 'tests\Pip.Tests.csproj') --configuration Release -- $dll
    if ($LASTEXITCODE) { throw 'Behaviour tests failed.' }
}

