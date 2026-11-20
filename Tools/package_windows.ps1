[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$EngineRoot
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$project = Join-Path $projectRoot 'Survival.uproject'
$engine = Join-Path (Resolve-Path -LiteralPath $EngineRoot).Path 'Engine'
$build = Join-Path $engine 'Build\BatchFiles\Build.bat'
$editor = Join-Path $engine 'Binaries\Win64\UnrealEditor-Cmd.exe'
$uat = Join-Path $engine 'Build\BatchFiles\RunUAT.bat'
$appLocal = Join-Path $engine 'Binaries\ThirdParty\AppLocalDependencies'
$version = Get-Content -LiteralPath (Join-Path $engine 'Build\Build.version') -Raw | ConvertFrom-Json
if ($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8) {
    throw 'Use a Windows installation of Unreal Engine 5.8.'
}
foreach ($path in @($build, $editor, $uat, $appLocal)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required Unreal Engine build component is missing: $path"
    }
}

$dist = Join-Path $projectRoot 'Dist'
$work = Join-Path $dist ([Guid]::NewGuid().ToString('N'))
$stage = Join-Path $work 'Archive'
$marker = Join-Path $work 'crossroads-prepared.txt'
$logs = Join-Path $dist 'Logs'
$editorLog = Join-Path $logs 'PrepareCrossroads.log'
$zipName = 'Survival-Windows-x64.zip'
$zip = Join-Path $dist $zipName
New-Item -ItemType Directory -Path $stage, $logs -Force | Out-Null
$previousMarker = $env:SURVIVAL_PREPARED_MARKER

try {
    & $build SurvivalEditor Win64 Development "-Project=$project" -WaitMutex
    if ($LASTEXITCODE -ne 0) {
        throw "SurvivalEditor build failed with exit code $LASTEXITCODE."
    }

    $env:SURVIVAL_PREPARED_MARKER = $marker
    $prepare = Join-Path $PSScriptRoot 'prepare_crossroads.py'
    & $editor $project "-ExecutePythonScript=$prepare" "-abslog=$editorLog" -unattended -nosplash -nullrhi
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $marker)) {
        throw "Crossroads preparation failed. Check $editorLog."
    }

    & $uat BuildCookRun "-project=$project" -targetplatform=Win64 -clientconfig=Shipping `
        -build -cook -stage -pak -package -archive -unattended -utf8output `
        "-applocaldirectory=$appLocal" "-archivedirectory=$stage"
    if ($LASTEXITCODE -ne 0) {
        throw "Windows packaging failed with exit code $LASTEXITCODE."
    }

    $package = @($stage) + @(Get-ChildItem -LiteralPath $stage -Directory | ForEach-Object FullName)
    $package = @($package | Where-Object {
        (Test-Path -LiteralPath (Join-Path $_ 'Survival.exe') -PathType Leaf) -and
        (Test-Path -LiteralPath (Join-Path $_ 'Survival\Content\Paks') -PathType Container)
    })
    if ($package.Count -ne 1) {
        throw 'Expected one staged Windows package with Survival.exe and cooked content.'
    }

    $importedSidearm = Join-Path $projectRoot 'Content\Art\Crossroads\Building\SM_CrossroadsSidearm.uasset'
    if (Test-Path -LiteralPath $importedSidearm -PathType Leaf) {
        Copy-Item -LiteralPath (Join-Path $projectRoot 'Docs\SidearmAttribution.txt') -Destination $package[0]
    }

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $temporaryZip = Join-Path $work $zipName
    [System.IO.Compression.ZipFile]::CreateFromDirectory($package[0], $temporaryZip)
    Move-Item -LiteralPath $temporaryZip -Destination $zip -Force
    Write-Host "Windows playtest: $zip"
}
finally {
    $env:SURVIVAL_PREPARED_MARKER = $previousMarker
    Remove-Item -LiteralPath $work -Recurse -Force
}
