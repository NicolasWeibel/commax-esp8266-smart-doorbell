$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$rootDir = Resolve-Path (Join-Path $scriptDir "..\..")
$buildDir = Join-Path $scriptDir "build"

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

$compiler = Get-Command g++ -ErrorAction SilentlyContinue
if (-not $compiler) {
    throw "g++ was not found. Install a C++ compiler or run these tests in a CI environment."
}

$output = Join-Path $buildDir "bell_detector_tests.exe"

& $compiler.Source `
    -std=c++11 `
    -Wall `
    -Wextra `
    -Wpedantic `
    "-I$scriptDir" `
    "-I$(Join-Path $rootDir 'SmartDoorbell')" `
    (Join-Path $scriptDir "FakeArduino.cpp") `
    (Join-Path $rootDir "SmartDoorbell\BellDetector.cpp") `
    (Join-Path $scriptDir "BellDetectorTests.cpp") `
    -o $output

if ($LASTEXITCODE -ne 0) {
    throw "Host test compilation failed."
}

& $output
if ($LASTEXITCODE -ne 0) {
    throw "One or more BellDetector host tests failed."
}
