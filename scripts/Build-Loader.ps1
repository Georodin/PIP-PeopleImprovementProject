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
& (Join-Path $llvm 'llvm-dlltool.exe') -m i386:x86-64 -d (Join-Path $repo 'loader\kernel32.def') -l (Join-Path $out 'kernel32.lib')
if($LASTEXITCODE){throw 'Import library generation failed'}
& $Clang --target=x86_64-pc-windows-msvc -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-stack-protector -c (Join-Path $repo 'loader\bootstrap.c') -o (Join-Path $out 'bootstrap.obj')
if($LASTEXITCODE){throw 'Bootstrap compilation failed'}
& $Linker -flavor link /dll /noentry /nodefaultlib /machine:x64 "/out:$(Join-Path $out 'PIP.Bootstrap.dll')" (Join-Path $out 'bootstrap.obj') (Join-Path $out 'kernel32.lib')
if($LASTEXITCODE){throw 'Bootstrap linking failed'}
& (Join-Path $PSScriptRoot 'Build-Runtime.ps1') -Clang $Clang -Linker $Linker
if($LASTEXITCODE){throw 'Runtime build failed'}
& dotnet build (Join-Path $repo 'loader\PIP.Loader.csproj') --configuration Release --output $out --nologo
if($LASTEXITCODE){throw 'Launcher build failed'}
& (Join-Path $out 'PIP.Loader.exe') --self-test $repo
if($LASTEXITCODE){throw 'Loader self-test failed'}
