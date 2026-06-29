<#
.SYNOPSIS
    End-to-end smoke test for the AppHub Windows privileged helper.

.DESCRIPTION
    Drives the built AppHubPrivilegedHelper.exe exactly the way the hub's
    privileged-helper client does: stage a bundle artifact, hand the helper a
    narrow install request, and confirm it installs the app into the protected
    machine-level root (redirected here with EACP_APPHUB_INSTALL_ROOT so CI does
    not need to touch a real Program Files location). Also asserts that the
    helper rejects a tampered artifact.

.PARAMETER Helper
    Path to the built AppHubPrivilegedHelper.exe.
#>
param(
    [Parameter(Mandatory = $true)]
    [string]$Helper
)

$ErrorActionPreference = 'Stop'

function Assert($condition, $message)
{
    if (-not $condition)
    {
        Write-Error "ASSERT FAILED: $message"
        exit 1
    }
    Write-Host "ok: $message"
}

Assert (Test-Path $Helper) "helper executable exists at $Helper"

$version = & $Helper --version
Assert ($LASTEXITCODE -eq 0) "helper --version exits 0"
Write-Host "helper version: $version"

$work = Join-Path $env:TEMP ("apphub-win-smoke-" + [guid]::NewGuid().ToString('N'))
$installRoot = Join-Path $work "Program Files\Tamber\Apps"
$env:EACP_APPHUB_INSTALL_ROOT = $installRoot

try
{
    # Stage a bundle artifact: a directory at the top of a zip, the same shape
    # the hub downloads and verifies before calling the helper.
    $bundleName = "Tamber Studio.app"
    $bundleParent = Join-Path $work "build"
    $bundle = Join-Path $bundleParent $bundleName
    New-Item -ItemType Directory -Force -Path $bundle | Out-Null
    Set-Content -Path (Join-Path $bundle "Tamber Studio.exe") -Value "studio-v1" -NoNewline -Encoding ascii

    $artifact = Join-Path $work "studio.app.zip"
    tar -a -c -f $artifact -C $bundleParent $bundleName
    Assert (Test-Path $artifact) "artifact zip created with tar"

    $hash = (Get-FileHash -Algorithm SHA256 $artifact).Hash.ToLower()

    function Invoke-Helper($requestObject)
    {
        $req = Join-Path $work "request.json"
        $res = Join-Path $work "result.json"
        if (Test-Path $res) { Remove-Item $res }
        ($requestObject | ConvertTo-Json -Compress) | Set-Content -Path $req -Encoding ascii
        & $Helper installAppBundle $req $res | Out-Null
        $exit = $LASTEXITCODE
        $parsed = if (Test-Path $res) { Get-Content $res -Raw | ConvertFrom-Json } else { $null }
        return [pscustomobject]@{ Exit = $exit; Result = $parsed }
    }

    # 1. Happy path: the helper installs the bundle into the protected root.
    $good = Invoke-Helper @{
        productId = 'com.tamber.studio'; name = 'Tamber Studio'; version = '1.0.0'
        bundleName = $bundleName; artifactPath = $artifact; artifactSha256 = $hash
        requiredTeamIdentifier = ''
    }
    Assert ($good.Exit -eq 0) "helper exits 0 for a valid request"
    Assert ($good.Result.ok -eq $true) "helper reports ok for a valid request"
    $installed = Join-Path $installRoot (Join-Path $bundleName "Tamber Studio.exe")
    Assert (Test-Path $installed) "app installed into protected root: $installed"
    Assert ((Get-Content $installed -Raw) -eq "studio-v1") "installed payload matches"

    # 2. Tampered artifact: the helper re-validates the hash and refuses.
    $bad = Invoke-Helper @{
        productId = 'com.tamber.studio'; name = 'Tamber Studio'; version = '1.0.0'
        bundleName = $bundleName; artifactPath = $artifact
        artifactSha256 = ('0' * 64); requiredTeamIdentifier = ''
    }
    Assert ($bad.Exit -ne 0) "helper exits non-zero for a hash mismatch"
    Assert ($bad.Result.ok -eq $false) "helper reports failure for a hash mismatch"
    Assert ($bad.Result.error -match 'hash') "helper error mentions the hash mismatch"

    Write-Host "`nAppHub Windows install-to-root smoke test PASSED"
}
finally
{
    Remove-Item env:EACP_APPHUB_INSTALL_ROOT -ErrorAction SilentlyContinue
    Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
}
