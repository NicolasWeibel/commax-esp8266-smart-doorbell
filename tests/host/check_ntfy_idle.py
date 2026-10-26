#!/usr/bin/env python3
"""Exercise the actual ntfy idle tick without mocking the entire HTTP stack."""
import argparse
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--compiler", nargs="+", default=["g++"])
args = parser.parse_args()
build = ROOT / "tests/host/build"
build.mkdir(exist_ok=True)

source = (ROOT / "SmartDoorbell/NtfyClient.cpp").read_text(encoding="utf-8")
start = source.index("void NtfyClient::tick(")
end = source.index("\nvoid NtfyClient::close()", start)
tick = source[start:end]
header = (ROOT / "SmartDoorbell/NtfyClient.h").read_text(encoding="utf-8")
activity_field = re.search(r"uint(?:32|64)_t lastActivityMs_ = 0;", header)
assert activity_field is not None

# The production method and field declaration are copied verbatim. Only socket
# closure is substituted; this test makes no claims about TLS/HTTP behavior.
harness = r'''
#include <cassert>
#include <cstdint>
#include <iostream>
#define F(value) value
namespace AppConfig {
constexpr uint32_t kNtfyKeepAliveIdleMs = 30000;
constexpr bool kVerboseOperationalLogs = false;
}
class NtfyClient {
 public:
  bool httpReady_ = true;
  ACTIVITY_FIELD
  unsigned closes = 0;
  void tick(uint64_t nowMs, bool wifiConnected);
  void resetTransport(const char*, bool, bool = true) {
    ++closes;
    httpReady_ = false;
  }
};
TICK_METHOD
int main() {
  constexpr uint64_t wrap = 1ULL << 32;
  // Before wrap, spanning wrap, after wrap, and exactly at millis() == 0.
  const uint64_t activities[] = {1000, wrap - 10000, wrap + 1000, wrap};
  for (uint64_t activity : activities) {
    NtfyClient client;
    client.lastActivityMs_ = static_cast<uint32_t>(activity);
    client.tick(activity + 29999, true);
    assert(client.closes == 0);
    client.tick(activity + 30000, true);
    assert(client.closes == 1);
    client.tick(activity + 60000, true);
    assert(client.closes == 1);
  }
  NtfyClient disconnected;
  disconnected.tick(1000, false);
  assert(disconnected.closes == 1);
  std::cout << "ntfy idle clock tests passed.\n";
}
'''.replace("ACTIVITY_FIELD", activity_field.group()).replace("TICK_METHOD", tick)
cpp = build / "ntfy_idle_tests.cpp"
exe = build / "ntfy_idle_tests.exe"
cpp.write_text(harness, encoding="utf-8")
subprocess.run(args.compiler + ["-std=c++17", "-Wall", "-Wextra", "-Werror",
                              str(cpp), "-o", str(exe)], check=True)
subprocess.run([str(exe)], check=True)
