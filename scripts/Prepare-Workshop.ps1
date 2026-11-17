[CmdletBinding()]
param([switch]$Draft)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$meta = Get-Content -LiteralPath (Join-Path $repo 'workshop\metadata.json') -Raw | ConvertFrom-Json
if (-not $Draft -and (-not $meta.runtimeIntegrated -or -not $meta.gameplayVerified -or -not $meta.workshopCodeLoadingVerified)) {
    throw 'Workshop release blocked: no verified game bridge, gameplay, or Workshop code loading. Use -Draft for the development package.'
}
& (Join-Path $PSScriptRoot 'Build.ps1')
if ($LASTEXITCODE) { throw 'Build or tests failed.' }
$dist = Join-Path $repo 'dist'
New-Item -ItemType Directory -Path $dist -Force | Out-Null
$stage = Join-Path $dist ('pip-' + $meta.version + '-' + [guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $stage | Out-Null
if ($Draft) {
    Copy-Item -LiteralPath (Join-Path $repo 'build\PIP.Core.dll') -Destination $stage
    foreach ($item in @('README.md','LICENSE','docs','workshop','src','art')) {
        Copy-Item -LiteralPath (Join-Path $repo $item) -Destination $stage -Recurse
    }
    Set-Content -LiteralPath (Join-Path $stage 'DEVELOPMENT-ONLY.txt') -Value 'NOT A PLAYABLE MOD. Native policy engine only. Game integration and Workshop loading are not verified.'
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

