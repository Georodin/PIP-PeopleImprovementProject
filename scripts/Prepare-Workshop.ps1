[CmdletBinding()]
param([switch]$Draft)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$meta = Get-Content -LiteralPath (Join-Path $repo 'workshop\metadata.json') -Raw | ConvertFrom-Json
if (-not $Draft -and (-not $meta.runtimeIntegrated -or -not $meta.gameplayVerified -or -not $meta.workshopCodeLoadingVerified)) {
    throw 'Workshop release blocked: broader gameplay acceptance or Workshop code loading is unverified. Use -Draft for the development package or Prepare-Local.ps1 for local play.'
}
& (Join-Path $PSScriptRoot 'Build.ps1')
if ($LASTEXITCODE) { throw 'Build or tests failed.' }
& (Join-Path $PSScriptRoot 'Build-Loader.ps1')
if ($LASTEXITCODE) { throw 'Loader build or self-tests failed.' }
$dist = Join-Path $repo 'dist'
New-Item -ItemType Directory -Path $dist -Force | Out-Null
$stage = Join-Path $dist ('pip-' + $meta.version + '-' + [guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $stage | Out-Null
if ($Draft) {
    New-Item -ItemType Directory -Path (Join-Path $stage 'build\loader') -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $repo 'build\PIP.Core.dll') -Destination (Join-Path $stage 'build')
    foreach ($name in @('PIP.Loader.exe','PIP.Loader.dll','PIP.Loader.deps.json','PIP.Loader.runtimeconfig.json','PIP.Runtime.dll','PIP.Bootstrap.dll')) {
        Copy-Item -LiteralPath (Join-Path $repo "build\loader\$name") -Destination (Join-Path $stage 'build\loader')
    }
    foreach ($item in @('Enable-PIP.cmd','Disable-PIP.cmd','README.md','LICENSE','docs','workshop','src','runtime','art')) {
        Copy-Item -LiteralPath (Join-Path $repo $item) -Destination $stage -Recurse
    }
    Set-Content -LiteralPath (Join-Path $stage 'DEVELOPMENT-ONLY.txt') -Value 'WORKSHOP DEVELOPMENT DRAFT. Includes an experimental local launcher requiring .NET 9 x64 and the exact supported game build. This directory is not a verified Workshop payload; subscription alone will not activate PIP.'
} else {
    $payload = Join-Path $repo 'workshop\payload'
    if (-not (Test-Path -LiteralPath $payload -PathType Container)) { throw 'Missing reviewed workshop/payload directory with a real loadable implementation.' }
    Copy-Item -Path (Join-Path $payload '*') -Destination $stage -Recurse
}
$zip = "$stage.zip"
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip
$hash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash
Set-Content -LiteralPath "$zip.sha256" -Value "$hash  $([IO.Path]::GetFileName($zip))"
Write-Output "Created $zip"
Write-Output 'Nothing uploaded to Steam.'

