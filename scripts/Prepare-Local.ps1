[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
& (Join-Path $PSScriptRoot 'Build.ps1')
& (Join-Path $PSScriptRoot 'Build-Loader.ps1')
$meta=Get-Content -LiteralPath (Join-Path $repo 'workshop\metadata.json') -Raw | ConvertFrom-Json
$stage=Join-Path $repo ('dist\pip-local-'+$meta.version+'-'+[guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path (Join-Path $stage 'build\loader') -Force | Out-Null
foreach($name in @('PIP.Loader.exe','PIP.Loader.dll','PIP.Loader.deps.json','PIP.Loader.runtimeconfig.json','PIP.Runtime.dll','PIP.Bootstrap.dll')){
    Copy-Item -LiteralPath (Join-Path $repo "build\loader\$name") -Destination (Join-Path $stage 'build\loader')
}
Copy-Item -LiteralPath (Join-Path $repo 'build\PIP.Core.dll') -Destination (Join-Path $stage 'build')
foreach($name in @('Enable-PIP.cmd','Disable-PIP.cmd','README.md','LICENSE','docs','art')){
    Copy-Item -LiteralPath (Join-Path $repo $name) -Destination $stage -Recurse
}
'Experimental local singleplayer package. Requires .NET 9 x64 runtime and the exact supported Steam game build. Start the game in Steam, then run Enable-PIP.cmd. This is not a Workshop subscription payload.' | Set-Content -LiteralPath (Join-Path $stage 'LOCAL-INSTALL.txt')
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath "$stage.zip"
$hash=(Get-FileHash -LiteralPath "$stage.zip" -Algorithm SHA256).Hash
"$hash  $([IO.Path]::GetFileName($stage)).zip" | Set-Content -LiteralPath "$stage.zip.sha256"
Write-Output "Created $stage.zip"
