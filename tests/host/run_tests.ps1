param(
  [string]$Compiler = "g++",
  [string[]]$CompilerArguments = @()
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$Build = Join-Path $Root "tests\host\build"
New-Item -ItemType Directory -Force -Path $Build | Out-Null
$Output = Join-Path $Build "doorbell_logic_tests.exe"

& $Compiler @CompilerArguments -std=gnu++17 -Wall -Wextra -Werror `
  -I"$Root\tests\host" `
  -I"$Root\SmartDoorbell" `
  "$Root\tests\host\DoorbellLogicTests.cpp" `
  "$Root\SmartDoorbell\SessionManager.cpp" `
  "$Root\SmartDoorbell\DeliveryManager.cpp" `
  "$Root\SmartDoorbell\ConnectivityManager.cpp" `
  "$Root\SmartDoorbell\OfflineSummaryManager.cpp" `
  "$Root\SmartDoorbell\BellDetector.cpp" `
  -o $Output
if ($LASTEXITCODE -ne 0) {
  throw "Host test compilation failed (exit $LASTEXITCODE)."
}

& $Output
if ($LASTEXITCODE -ne 0) {
  throw "Host logic tests failed (exit $LASTEXITCODE)."
}

$MessagesOutput = Join-Path $Build "message_formatter_tests.exe"
& $Compiler @CompilerArguments -std=gnu++17 -Wall -Wextra -Werror `
  -I"$Root\tests\host" `
  -I"$Root\SmartDoorbell" `
  "$Root\tests\host\MessageFormatterTests.cpp" `
  "$Root\SmartDoorbell\MessageFormatter.cpp" `
  "$Root\SmartDoorbell\OfflineSummaryManager.cpp" `
  -o $MessagesOutput
if ($LASTEXITCODE -ne 0) {
  throw "Message test compilation failed (exit $LASTEXITCODE)."
}
& $MessagesOutput
if ($LASTEXITCODE -ne 0) {
  throw "Message tests failed (exit $LASTEXITCODE)."
}
