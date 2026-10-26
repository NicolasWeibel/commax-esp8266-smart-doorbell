param(
    [string]$Esp8266CorePath = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repositoryRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$sketchDirectory = Join-Path $repositoryRoot "SmartDoorbell"

if ([string]::IsNullOrWhiteSpace($Esp8266CorePath)) {
    $Esp8266CorePath = Join-Path `
        $env:LOCALAPPDATA `
        "Arduino15\packages\esp8266\hardware\esp8266\3.1.2"
}

if (-not (Test-Path $Esp8266CorePath)) {
    throw "ESP8266 Arduino Core 3.1.2 was not found at: $Esp8266CorePath"
}

$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) {
    $python = Get-Command py -ErrorAction SilentlyContinue
}
if (-not $python) {
    throw "Python 3 was not found in PATH."
}

# The Mozilla generator expects an executable named ar. Arduino installs it as
# xtensa-lx106-elf-ar.exe, so expose a temporary alias without modifying the
# repository or the permanent user PATH.
$arSource = Get-ChildItem `
    -Path (Join-Path $env:LOCALAPPDATA "Arduino15\packages\esp8266\tools") `
    -Filter "xtensa-lx106-elf-ar.exe" `
    -Recurse `
    -File `
    -ErrorAction SilentlyContinue |
    Select-Object -First 1

if (-not $arSource) {
    throw "xtensa-lx106-elf-ar.exe was not found in the installed ESP8266 toolchain."
}

$tempTools = Join-Path $env:TEMP "esp8266-cert-tools"
New-Item -ItemType Directory -Path $tempTools -Force | Out-Null
Copy-Item `
    -Path $arSource.FullName `
    -Destination (Join-Path $tempTools "ar.exe") `
    -Force
$env:PATH = "$tempTools;$env:PATH"

$openssl = Get-Command openssl -ErrorAction SilentlyContinue
if (-not $openssl) {
    $openssl = Get-ChildItem `
        -Path "C:\Program Files\Git", "$env:LOCALAPPDATA\Programs\Git" `
        -Filter "openssl.exe" `
        -Recurse `
        -File `
        -ErrorAction SilentlyContinue |
        Select-Object -First 1

    if ($openssl) {
        $env:PATH = "$($openssl.DirectoryName);$env:PATH"
    }
}

if (-not (Get-Command openssl -ErrorAction SilentlyContinue)) {
    throw "OpenSSL was not found. Git for Windows normally includes it under mingw64\bin."
}

$tempGenerator = Join-Path $env:TEMP "certs-from-mozilla-current.py"
$currentGeneratorUrl = "https://raw.githubusercontent.com/esp8266/Arduino/master/libraries/ESP8266WiFi/examples/BearSSL_CertStore/certs-from-mozilla.py"

try {
    Invoke-WebRequest `
        -Uri $currentGeneratorUrl `
        -OutFile $tempGenerator `
        -UseBasicParsing
}
catch {
    Write-Warning "Could not download the current official generator. Falling back to the copy bundled with ESP8266 Core 3.1.2."

    $bundledGenerator = Get-ChildItem `
        -Path $Esp8266CorePath `
        -Filter "certs-from-mozilla.py" `
        -Recurse `
        -File |
        Select-Object -First 1

    if (-not $bundledGenerator) {
        throw "The bundled certs-from-mozilla.py generator was not found."
    }

    $generatorText = Get-Content $bundledGenerator.FullName -Raw
    $generatorText = $generatorText.Replace(
        "https://ccadb-public.secure.force.com/mozilla/IncludedCACertificateReportPEMCSV",
        "https://ccadb.my.salesforce-sites.com/mozilla/IncludedCACertificateReportPEMCSV"
    )
    Set-Content `
        -Path $tempGenerator `
        -Value $generatorText `
        -Encoding UTF8
}

Push-Location $sketchDirectory
try {
    & $python.Source $tempGenerator
    if ($LASTEXITCODE -ne 0) {
        throw "The certificate-store generator returned exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}

$archive = Join-Path $sketchDirectory "data\certs.ar"
if (-not (Test-Path $archive)) {
    throw "The generator completed, but SmartDoorbell/data/certs.ar was not created."
}

$archiveInfo = Get-Item $archive
if ($archiveInfo.Length -le 0) {
    throw "SmartDoorbell/data/certs.ar was created but is empty."
}

Write-Host "Certificate store created successfully:" -ForegroundColor Green
Write-Host $archiveInfo.FullName
Write-Host "Bytes: $($archiveInfo.Length)"
Write-Host "Upload the SmartDoorbell/data directory to LittleFS before using the firmware."
