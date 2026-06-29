<#
.SYNOPSIS
    End-to-end publish + update test for AppHub on Windows.

.DESCRIPTION
    Exercises the full hub flow against a locally generated catalog (the same
    EACP_APPHUB_DEV_CATALOG_PATH mechanism the build wires up), with artifacts
    served as file:// URLs so no network or signing is needed:

      1. Publish v1 of a product into a generated catalog.
      2. `AppHub catalog-install` it -> the privileged helper installs it into
         the protected machine root (redirected via EACP_APPHUB_INSTALL_ROOT).
      3. Publish v2 (bump the catalog + product version).
      4. `AppHub catalog-install` again -> the hub plans an Update and the helper
         atomically replaces the install, keeping a rollback.

    Asserts the helper-owned receipt moves from v1 to v2 and that a rollback of
    the previous version is retained.

.PARAMETER AppHub        Path to the built AppHub.exe.
.PARAMETER Helper        Path to the built AppHubPrivilegedHelper.exe.
.PARAMETER BundleDir     Built app directory to package (e.g. build/Apps/GPU/Maze).
.PARAMETER ProductId     Catalog product id (default com.eacp.maze).
.PARAMETER ProductName   Display name (default Maze).
.PARAMETER BundleName    Installed bundle name (default Maze.app).
#>
param(
    [Parameter(Mandatory = $true)] [string]$AppHub,
    [Parameter(Mandatory = $true)] [string]$Helper,
    [Parameter(Mandatory = $true)] [string]$BundleDir,
    [string]$ProductId = 'com.eacp.maze',
    [string]$ProductName = 'Maze',
    [string]$BundleName = 'Maze.app'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$generator = Join-Path $repoRoot 'Scripts/generate-apphub-local-catalog.mjs'

function Assert($condition, $message)
{
    if (-not $condition) { Write-Error "ASSERT FAILED: $message"; exit 1 }
    Write-Host "ok: $message"
}

foreach ($p in @($AppHub, $Helper, $generator)) { Assert (Test-Path $p) "exists: $p" }
Assert (Test-Path $BundleDir) "built bundle dir exists: $BundleDir"

$work = Join-Path $env:TEMP ("apphub-win-update-" + [guid]::NewGuid().ToString('N'))
$installRoot = Join-Path $work 'Program Files\Tamber\Apps'
$hubRoot = Join-Path $work 'hub'
$catalogPath = Join-Path $work 'catalog\apphub-catalog.json'
$artifactDir = Join-Path $work 'catalog\artifacts'

$env:EACP_APPHUB_INSTALL_ROOT = $installRoot
$env:EACP_APPHUB_HELPER_PATH = $Helper
$env:EACP_APPHUB_DEV_CATALOG_PATH = $catalogPath

function Publish($version)
{
    $descriptor = "$ProductName|$ProductId|$ProductName|$BundleName|$version|App||$BundleDir"
    $major = $version.Split('.')[0]
    node $generator --catalog $catalogPath --artifact-dir $artifactDir `
        --catalog-version $major --channel stable --product $descriptor
    Assert ($LASTEXITCODE -eq 0) "published catalog at v$version"
}

function ReceiptVersion
{
    $receipt = Join-Path $hubRoot "receipts\$ProductId.json"
    if (-not (Test-Path $receipt)) { return $null }
    return (Get-Content $receipt -Raw | ConvertFrom-Json).version
}

# AppHub is a GUI-subsystem executable, so `& app.exe` returns immediately
# without waiting. Start-Process -Wait blocks until the command finishes and
# RedirectStandardOutput captures the result line a console would not see.
function Invoke-AppHub([string[]]$appHubArgs)
{
    $outFile = Join-Path $work ("apphub-out-" + [guid]::NewGuid().ToString('N') + ".txt")
    $proc = Start-Process -FilePath $AppHub -ArgumentList $appHubArgs `
        -NoNewWindow -Wait -PassThru -RedirectStandardOutput $outFile
    if (Test-Path $outFile) { Get-Content $outFile | ForEach-Object { Write-Host "  apphub: $_" } }
    return $proc.ExitCode
}

try
{
    # 1. Publish + install v1.
    Publish '1.0.0'
    Assert ((Invoke-AppHub @('--root', $hubRoot, 'catalog-install', $ProductId)) -eq 0) "catalog-install v1 exits 0"
    $installedBundle = Join-Path $installRoot $BundleName
    Assert ((Test-Path $installedBundle) -and (Get-ChildItem $installedBundle -File -Recurse | Select-Object -First 1)) "v1 installed into protected root"
    Assert ((ReceiptVersion) -eq '1.0.0') "receipt records v1.0.0 (got '$(ReceiptVersion)')"

    # 2. Publish v2 and update.
    Publish '2.0.0'
    Assert ((Invoke-AppHub @('--root', $hubRoot, 'catalog-install', $ProductId)) -eq 0) "catalog-install (update) v2 exits 0"
    Assert ((ReceiptVersion) -eq '2.0.0') "receipt updated to v2.0.0 (got '$(ReceiptVersion)')"
    Assert (Test-Path (Join-Path $installRoot ($BundleName + '.rollback'))) "rollback of previous version retained"

    Write-Host "`nAppHub Windows publish + update flow PASSED"
}
finally
{
    Remove-Item env:EACP_APPHUB_INSTALL_ROOT -ErrorAction SilentlyContinue
    Remove-Item env:EACP_APPHUB_HELPER_PATH -ErrorAction SilentlyContinue
    Remove-Item env:EACP_APPHUB_DEV_CATALOG_PATH -ErrorAction SilentlyContinue
    Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
}

# Assert exits 1 on failure; make the success path's exit code explicit so it
# does not inherit $LASTEXITCODE from an earlier native command.
exit 0
