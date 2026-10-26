$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$Package = $PSScriptRoot
$LogDirectory = Join-Path $env:LOCALAPPDATA "HiFaceDemo/logs"
New-Item -ItemType Directory -Force -Path $LogDirectory | Out-Null
$Report = Join-Path $LogDirectory ("self-test-" + (Get-Date -Format "yyyyMMdd-HHmmss") + "-$PID.txt")
function Record([string]$Text) {
    Write-Host $Text
    Add-Content -LiteralPath $Report -Value $Text -Encoding UTF8
}
try {
    Record "HiFace Windows x64 diagnostic report"
    Record ("Date: " + (Get-Date -Format o))
    Record ("OS: " + [Environment]::OSVersion.VersionString)
    Record ("CPU: " + $env:PROCESSOR_IDENTIFIER)
    Record ("64-bit OS: " + [Environment]::Is64BitOperatingSystem)
    Record ("Package: " + $Package)
    if (-not [Environment]::Is64BitOperatingSystem) { throw "Windows x64 is required." }
    Record (Get-Content -LiteralPath (Join-Path $Package "sdk-metadata.json") -Raw -Encoding UTF8)
    $Checksums = Get-Content -LiteralPath (Join-Path $Package "checksums.json") -Raw -Encoding UTF8 | ConvertFrom-Json
    $Count = 0
    foreach ($Entry in $Checksums.files.PSObject.Properties) {
        $File = Join-Path $Package $Entry.Name
        if (-not (Test-Path -LiteralPath $File -PathType Leaf)) { throw "Missing file: $($Entry.Name)" }
        $Actual = (Get-FileHash -LiteralPath $File -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($Actual -ne $Entry.Value) { throw "SHA-256 mismatch: $($Entry.Name)" }
        $Count++
    }
    Record "Package integrity: PASS ($Count files)"
    $Stdout = "$Report.stdout.txt"
    $Stderr = "$Report.stderr.txt"
    # Start-Process avoids PowerShell 5 treating SDK stderr logging as a terminating error.
    $Process = Start-Process -FilePath (Join-Path $Package "bin/hiface_cli.exe") `
        -ArgumentList @("self-test") `
        -WorkingDirectory $Package -Wait -PassThru -NoNewWindow `
        -RedirectStandardOutput $Stdout -RedirectStandardError $Stderr
    Record "--- SDK self-test stdout ---"
    if (Test-Path -LiteralPath $Stdout) { Record (Get-Content -LiteralPath $Stdout -Raw -Encoding UTF8) }
    Record "--- SDK self-test stderr ---"
    if (Test-Path -LiteralPath $Stderr) { Record (Get-Content -LiteralPath $Stderr -Raw -Encoding UTF8) }
    if ($Process.ExitCode -ne 0) { throw "SDK self-test failed with exit code $($Process.ExitCode)." }
    Record "RESULT: PASS. Camera and real-face recognition still require the GUI test."
    Record ("Report: " + $Report)
    exit 0
} catch {
    Record ("RESULT: FAIL | " + $_.Exception.Message)
    Record ("Report: " + $Report)
    exit 1
}
