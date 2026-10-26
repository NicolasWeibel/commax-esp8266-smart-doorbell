#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="$ROOT/tests/host/build"
mkdir -p "$BUILD"

g++ -std=gnu++17 -Wall -Wextra -Werror \
  -I"$ROOT/tests/host" \
  -I"$ROOT/SmartDoorbell" \
  "$ROOT/tests/host/DoorbellLogicTests.cpp" \
  "$ROOT/SmartDoorbell/SessionManager.cpp" \
  "$ROOT/SmartDoorbell/DeliveryManager.cpp" \
  "$ROOT/SmartDoorbell/ConnectivityManager.cpp" \
  "$ROOT/SmartDoorbell/OfflineSummaryManager.cpp" \
  "$ROOT/SmartDoorbell/BellDetector.cpp" \
  -o "$BUILD/doorbell_logic_tests"

"$BUILD/doorbell_logic_tests"

g++ -std=gnu++17 -Wall -Wextra -Werror \
  -I"$ROOT/tests/host" \
  -I"$ROOT/SmartDoorbell" \
  "$ROOT/tests/host/MessageFormatterTests.cpp" \
  "$ROOT/SmartDoorbell/MessageFormatter.cpp" \
  "$ROOT/SmartDoorbell/OfflineSummaryManager.cpp" \
  -o "$BUILD/message_formatter_tests"

"$BUILD/message_formatter_tests"
