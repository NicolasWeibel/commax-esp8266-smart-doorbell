#!/usr/bin/env sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)
BUILD_DIR="$SCRIPT_DIR/build"

mkdir -p "$BUILD_DIR"

c++ \
  -std=c++11 \
  -Wall \
  -Wextra \
  -Wpedantic \
  -I"$SCRIPT_DIR" \
  -I"$ROOT_DIR/SmartDoorbell" \
  "$SCRIPT_DIR/FakeArduino.cpp" \
  "$ROOT_DIR/SmartDoorbell/BellDetector.cpp" \
  "$SCRIPT_DIR/BellDetectorTests.cpp" \
  -o "$BUILD_DIR/bell_detector_tests"

"$BUILD_DIR/bell_detector_tests"
