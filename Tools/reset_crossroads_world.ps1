[CmdletBinding(SupportsShouldProcess)]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not (Test-Path -LiteralPath (Join-Path $projectRoot 'Survival.uproject') -PathType Leaf)) {
    throw 'Run this script from the Tools directory inside the Survival project.'
}

# Run with Unreal closed so it cannot save old packages over the regenerated world.
$paths = @(
    'Content\Maps\L_CrossroadsRelief.umap'
    'Content\__ExternalActors__\Maps\L_CrossroadsRelief'
    'Content\__ExternalObjects__\Maps\L_CrossroadsRelief'
    'Content\Art\Crossroads\World'
    'Saved\WorldBake'
    'Saved\CrossroadsPreparation.sha256'
)
$present = @($paths | Where-Object { Test-Path -LiteralPath (Join-Path $projectRoot $_) })
if ($present.Count -eq 0) {
    Write-Host 'No generated Crossroads world files found. Open Survival.uproject to bake the region.'
    return
}

$backupName = 'CrossroadsWorldBackup-' + [Guid]::NewGuid().ToString('N')
$backup = Join-Path (Join-Path $projectRoot 'Saved') $backupName
if (-not $PSCmdlet.ShouldProcess($backup, "Back up generated Crossroads files: $($present -join ', ')")) {
    return
}

$moved = [System.Collections.Generic.List[string]]::new()
try {
    foreach ($path in $present) {
        $destination = Join-Path $backup $path
        New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
        Move-Item -LiteralPath (Join-Path $projectRoot $path) -Destination $destination
        $moved.Add($path)
    }
}
catch {
    $moveError = $_
    foreach ($path in $moved) {
        try {
            Move-Item -LiteralPath (Join-Path $backup $path) -Destination (Join-Path $projectRoot $path)
        }
        catch {
            Write-Warning "Could not restore $path from $backup. $($_.Exception.Message)"
        }
    }
    throw $moveError
}

Write-Host "Crossroads world backup: $backup"
Write-Host 'Open Survival.uproject and wait for Crossroads: region ready. You can press Play.'
