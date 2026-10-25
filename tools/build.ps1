param([string]$Distribution = 'kali-linux')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
& python (Join-Path $PSScriptRoot 'prepare_assets.py')
if ($LASTEXITCODE -ne 0) { throw 'Original icon asset preparation failed' }
& python (Join-Path $PSScriptRoot 'prepare_prompts.py')
if ($LASTEXITCODE -ne 0) { throw 'Original controller sheet preparation failed' }
$linuxRoot = '/mnt/' + $projectRoot.Substring(0,1).ToLower() + $projectRoot.Substring(2).Replace('\','/')
& wsl -d $Distribution -- bash "$linuxRoot/tools/build.sh"
if ($LASTEXITCODE -ne 0) { throw "PS4 build failed with exit code $LASTEXITCODE" }
$env:DOTNET_ROLL_FORWARD = 'LatestMajor'
$version = Get-Content (Join-Path $projectRoot 'version.json') -Raw | ConvertFrom-Json
$pkg = Join-Path $projectRoot ('dist\' + $version.release_directory + '\IV0000-JFPS40000_00-JELLYFINPS400000.pkg')
& "$projectRoot\tools\OpenOrbis\PS4Toolchain\bin\windows\PkgTool.Core.exe" pkg_validate $pkg
if ($LASTEXITCODE -ne 0) { throw "PKG validation failed" }
Get-FileHash -LiteralPath $pkg -Algorithm SHA256
