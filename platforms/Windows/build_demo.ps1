param([string]$BuildDirectory = "", [string]$PublisherKey = "", [switch]$SkipTests)
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
if ($env:OS -ne "Windows_NT") { throw "Windows x64 and Visual Studio 2022 C++ tools are required." }
if (-not $BuildDirectory) { $BuildDirectory = Join-Path $PSScriptRoot "build" }
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
$Sdk = Join-Path $PSScriptRoot "sdk"
$Version = (Get-Content -LiteralPath "$Sdk/sdk-metadata.json" -Raw | ConvertFrom-Json).version
$BindPackage = [version]$Version -ge [version]"0.0.6"
if ($BindPackage) {
    if (-not $PublisherKey) { throw "Pass -PublisherKey with your stable Ed25519 PKCS#8 PEM outside this checkout; Python cryptography is required." }
    $PublisherKey = (Resolve-Path -LiteralPath $PublisherKey).Path
    if ($PublisherKey.StartsWith($PSScriptRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Keep the application publisher private key outside this checkout."
    }
}
foreach ($Forbidden in @("sdk/src", "sdk/models", "sdk/data", "demo/models")) {
    if (Test-Path -LiteralPath (Join-Path $PSScriptRoot $Forbidden)) {
        throw "Unexpected private resource in binary SDK distribution: $Forbidden"
    }
}
function Invoke-Checked([string]$Program, [string[]]$Parameters) {
    & $Program @Parameters
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}
foreach ($Role in @("sdk", "demo")) {
    $Source = if ($Role -eq "sdk") { Join-Path $PSScriptRoot "examples" } else { Join-Path $PSScriptRoot "demo/source" }
    $Build = Join-Path $BuildDirectory $Role
    Invoke-Checked "cmake" @("-S", $Source, "-B", $Build, "-G", "Visual Studio 17 2022", "-A", "x64", "-DCMAKE_PREFIX_PATH=$Sdk")
    Invoke-Checked "cmake" @("--build", $Build, "--config", "Release", "--parallel", "2")
    if ($BindPackage) {
        $Runtime = Join-Path $Build "bin/Release"
        foreach ($Application in (Get-ChildItem -LiteralPath $Runtime -Filter "*.exe")) {
            Invoke-Checked "python" @("$PSScriptRoot/demo/tools/sign_product.py", "--platform", "windows",
                "--root", $Runtime, "--entry", $Application.FullName, "--sdk", "$Runtime/hiface.dll",
                "--private-key", $PublisherKey, "--product-id", "com.hiface.demo", "--version", $Version, "--replace")
        }
    }
    if (-not $SkipTests) {
        Push-Location "$Build/bin/Release"
        try {
            if ($Role -eq "sdk") {
                foreach ($Name in @("hiface_c_consumer", "hiface_cpp_consumer", "hiface_model_consumer")) {
                    Invoke-Checked "./$Name.exe" @()
                }
            } else {
                Invoke-Checked "./hiface_cli.exe" @("self-test")
                $TestData = Join-Path $env:TEMP ("HiFace Demo data " + [Guid]::NewGuid().ToString("N"))
                New-Item -ItemType Directory -Path $TestData | Out-Null
                try {
                    $Database = Join-Path $TestData "hiface-search.db"
                    $Roundtrip = Join-Path $TestData "roundtrip.db"
                    $Export = Join-Path $TestData "features.json"
                    Invoke-Checked "./hiface_cli.exe" @("import-json", $Database, (Join-Path $PSScriptRoot "demo/data/features_10000.json"))
                    Invoke-Checked "./hiface_cli.exe" @("export-json", $Database, $Export)
                    Invoke-Checked "./hiface_cli.exe" @("import-json", $Roundtrip, $Export)
                    $Count = & "./hiface_cli.exe" count $Roundtrip
                    if ($LASTEXITCODE -ne 0 -or ($Count -join "`n").Trim() -ne "count=10000") { throw "AI Demo data roundtrip failed" }
                    Write-Host "AI Demo data: 10000 HF1 entries imported and roundtripped."
                } finally { Remove-Item -LiteralPath $TestData -Recurse -Force }
            }
        } finally { Pop-Location }
    }
}
Write-Host "Demo: $BuildDirectory/demo/bin/Release/hiface_demo.exe"
