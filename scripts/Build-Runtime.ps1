[CmdletBinding()]
param([string]$Clang,[string]$Linker)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
if(-not $Clang){$command=Get-Command clang.exe -ErrorAction SilentlyContinue;if($command){$Clang=$command.Source}}
if(-not $Clang){$Clang=Get-ChildItem "$env:ProgramFiles\Unity\Hub\Editor\*\Editor\Data\PlaybackEngines\AndroidPlayer\NDK\toolchains\llvm\prebuilt\windows-x86_64\bin\clang.exe" -ErrorAction SilentlyContinue | Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName}
if(-not $Clang){throw 'Pass -Clang with the path to LLVM clang.exe.'}
$llvm=Split-Path $Clang
if(-not $Linker){$Linker=Join-Path $llvm 'ld.lld.exe'}
$out=Join-Path $repo 'build\loader'
New-Item -ItemType Directory -Path $out -Force | Out-Null
& (Join-Path $llvm 'llvm-dlltool.exe') -m i386:x86-64 -d (Join-Path $repo 'runtime\kernel32.def') -l (Join-Path $out 'runtime-kernel32.lib')
if($LASTEXITCODE){throw 'Runtime import library generation failed'}
$objects=@()
foreach($name in @('runtime','nav','nav_tests','hooks')){
    $object=Join-Path $out "$name.obj"
    & $Clang --target=x86_64-pc-windows-msvc -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-math-errno -fno-stack-protector -I (Join-Path $repo 'src') -c (Join-Path $repo "runtime\$name.c") -o $object
    if($LASTEXITCODE){throw "$name compilation failed"}
    $objects+=$object
}
& $Clang --target=x86_64-pc-windows-msvc -c (Join-Path $repo 'runtime\bridge.S') -o (Join-Path $out 'bridge.obj')
if($LASTEXITCODE){throw 'Movement bridge assembly failed'}
$objects+=(Join-Path $out 'bridge.obj')
$objects+=(Join-Path $repo 'build\pip.obj')
& $Linker -flavor link /dll /noentry /nodefaultlib /machine:x64 "/out:$(Join-Path $out 'PIP.Runtime.dll')" @objects (Join-Path $out 'runtime-kernel32.lib')
if($LASTEXITCODE){throw 'Runtime linking failed'}
Write-Output 'Built PIP.Runtime.dll. Build-Loader.ps1 runs its navigation and assembly self-tests.'
