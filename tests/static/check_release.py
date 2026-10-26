#!/usr/bin/env python3
from pathlib import Path
import re
import sys
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "SmartDoorbell"
failures: list[str] = []


def require(condition: bool, message: str) -> None:
    if not condition:
        failures.append(message)


def text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


config = text("SmartDoorbell/AppConfig.h")
app_types = text("SmartDoorbell/AppTypes.h")
controller_h = text("SmartDoorbell/DoorbellController.h")
controller = text("SmartDoorbell/DoorbellController.cpp")
connectivity = text("SmartDoorbell/ConnectivityManager.cpp")
delivery = text("SmartDoorbell/DeliveryManager.cpp")
offline_h = text("SmartDoorbell/OfflineSummaryManager.h")
offline = text("SmartDoorbell/OfflineSummaryManager.cpp")
formatter = text("SmartDoorbell/MessageFormatter.cpp")
persistent = text("SmartDoorbell/PersistentStore.cpp")
network = text("SmartDoorbell/NetworkDiagnostics.cpp")
ntfy_h = text("SmartDoorbell/NtfyClient.h")
ntfy = text("SmartDoorbell/NtfyClient.cpp")
ino = text("SmartDoorbell/SmartDoorbell.ino")
timekeeper = text("SmartDoorbell/TimeKeeper.cpp")
wifi = text("SmartDoorbell/WifiManager.cpp")
wifi_h = text("SmartDoorbell/WifiManager.h")
tcp_diag = text("SmartDoorbell/TcpDiagnostics.cpp")
bell_detector_h = text("SmartDoorbell/BellDetector.h")
bell_detector = text("SmartDoorbell/BellDetector.cpp")
recovery_policy = text("SmartDoorbell/RecoveryCompletionPolicy.h")
wifi_outage = text("SmartDoorbell/WifiOutageAggregator.h")

version_match = re.search(r'kFirmwareVersion\[\]\s*=\s*"([^"]+)"', config)
version = version_match.group(1) if version_match else ''
require(bool(version), "firmware version is missing")
require(bool(version) and f'## {version}' in text('CHANGELOG.md'),
        "current firmware version is missing from the changelog")
require(bool(version) and version in text('docs/VALIDATION.md') and
        version in text('README.md'),
        "README/validation firmware identity does not match AppConfig.h")
require('quieter diagnostics' in config and 'DHCP-only networking' in config,
        "production implementation stage is missing")
require('kOfflineAfterFailures = 4' in config and
        'kWifiOfflineConfirmationMs = 5000' in config and
        'kOfflineFailureWindowMs = 60000' in config and
        'kOfflineRecoveryProbeIntervalMs = 60000' in config,
        "offline declaration/recovery policy is missing")
require('ConnectivityState::WaitingForTime' in connectivity and
        'OfflineReason::TimeUnavailable' in connectivity and
        'kOfflineFailureWindowMs' in connectivity,
        "boot-without-time offline handling is missing")
require('recordPublishFailure' in connectivity and
        'OfflineReason::PublishFailures' in connectivity,
        "four-failure offline transition is missing")
require('onlineEstablished_' in connectivity and
        'normal boot' in connectivity,
        "boot association/DHCP grace is missing")

for token in ('OfflineSummaryManager', 'recordOfflineBell',
              'processOfflineRecovery', 'enterOffline',
              'collapseUnreported', 'occasionCount'):
    require(token in controller_h + controller + delivery + offline_h + offline,
            f"missing offline summary feature: {token}")
require('changedDuringPublish' in controller and
        'markInitialConfirmed' in controller + offline,
        "bell events during recovery publication could be lost")
require('recoveryPublishAwaitingInputSettle_' in controller_h + controller and
        'sent_deferred=yes' in controller and
        'sent_finalized_after_input_settled' in controller and
        'kRecoveryInputSettlePollMs' in config and
        'RecoveryCompletionPolicy::afterSuccessfulPublish' in controller,
        "in-flight bell completion guard is missing")
require('activityInFlight' in bell_detector_h + bell_detector and
        'pendingRawEdges' in bell_detector and
        'currentLevelHigh_' in bell_detector,
        "bell input activity probe is missing")
require('highTimingReliable_' in bell_detector_h + bell_detector and
        'discardUnreliableHigh' in bell_detector and
        'processEdge(syntheticEdge, nowMs, false)' in bell_detector,
        "overflow/resynchronization timing hardening is missing")
require('WaitForInput' in recovery_policy and
        'ScheduleUpdate' in recovery_policy and
        'Finalize' in recovery_policy,
        "recovery completion policy is incomplete")
require('confirmedBaseline' in delivery and
        'unreportedPresses' in delivery and
        'desiredCount - job.confirmedCount' in delivery and
        'confirmedThrough' in controller,
        "already-delivered presses are not excluded from offline summary")
require('currentOccasionOpen' in offline and
        'kOfflineOccasionSilenceMs' in offline,
        "offline occasion grouping is missing")
require('UnknownAfterRestart' in offline and
        'lastBellBootNonce' in offline and
        'RelativeUptime' in offline,
        "relative time/restart uncertainty handling is missing")

for phrase in (
    'Portero nuevamente en línea',
    'ocasiones distintas',
    'en una sola ocasión',
    'se reinició sin conexión',
    'ayer a las',
):
    require(phrase in formatter, f"missing recovery message phrase: {phrase}")
require('sesiones' not in formatter.lower(),
        "user-facing recovery message must not mention sessions")

require('PersistentStateRecordV3' in app_types and
        'sizeof(PersistentStateRecordV3) == 900U' in app_types and
        'sizeof(PersistentStateRecord) == 996U' in app_types,
        "persistent v3 migration/v4 layout is missing")
require('kLegacyStatePath' in config and
        'validateLegacy' in persistent and
        'removeLegacyFiles' in persistent,
        "v3-to-v4 persistence migration is incomplete")
require('saveRecord_.offline = offlineSummary_.persistentState()' in controller,
        "offline summary is not persisted")

require('#include <ESP8266HTTPClient.h>' in ntfy_h and
        'http_.setReuse(true)' in ntfy,
        "keep-alive text transport was not preserved")
require('setInsecure' not in text('SmartDoorbell/TlsManager.cpp') and
        'kAllowInsecureTlsForCommissioning' not in config,
        "insecure TLS fallback must not exist")
require('kWifiRecycleAfterConnectFailures = 255' in config,
        "service failures should not repeatedly recycle healthy Wi-Fi")
require('ALL_NETWORK_LAYERS_OK' in network,
        "successful layered diagnostic classification is not fixed")
require('configTime(Secrets::kTimeZone' in timekeeper,
        "POSIX timezone/NTP configuration changed")
require('Tocaron el timbre.' in formatter,
        "unknown-time first notification fallback is missing")
require('formatNaturalMoment' in formatter and 'eventLocal.tm_yday' in formatter,
        "last-bell local date formatting is missing")

for token in ('onStationModeConnected', 'onStationModeDisconnected',
              'onStationModeGotIP', 'wifi_station_dhcpc_status',
              'netif_dhcp_data', 'recoverInvalidIpv4'):
    require(token in wifi + wifi_h, f"missing Wi-Fi instrumentation: {token}")
for token in ('dns_ntfy', 'dns_alternate', 'tcp_ntfy', 'tls_ntfy',
              'NET_DIAG_RESULT'):
    require(token in network, f"missing layered diagnostic probe: {token}")
require('lwip/priv/tcp_priv.h' in tcp_diag,
        "TCP PCB diagnostics are missing")

for token in ('status', 'sim <1..20>', 'diag', 'wifihealth',
              'wifievents', 'wificonfig', 'wifireconnect'):
    require(token in ino, f"missing serial command/help: {token}")
require('offline_presses' in ino and 'offline_occasions' in ino,
        "status does not expose offline summary counts")
require('serialCommand.reserve(80)' in ino and
        'serialCommand.remove(0)' in ino,
        "serial command buffer reuse is missing")

require('kWifiOutageSummaryIntervalMs = 60000' in config and
        'kVerboseOperationalLogs = false' in config,
        "production log policy is missing")
require('recordNoAp' in wifi_outage and 'periodicDue' in wifi_outage and
        'finish' in wifi_outage,
        "NO_AP_FOUND aggregation policy is missing")
require('WifiOutageAggregator noApOutage_' in wifi_h and
        'NO_AP_FOUND_SUMMARY' in wifi and
        'WIFI_RECOVERED_SUMMARY' in wifi and
        'WIFI_OUTAGE | state=' in wifi,
        "Wi-Fi outage summaries are incomplete")
require('!noApOutage_.recordNoAp(millis())' in wifi and
        'if (!noApOutage_.active())' in wifi,
        "repeated callback/history events are not suppressed")
require('post_begin | path=' not in ntfy and
        'post_begin | sequence=' in ntfy,
        "private ntfy path is still printed")
for token in ('enableStaticFromCurrent', 'enableStaticConfiguration',
              'staticConfigurationEnabled_', 'STATIC_CONFIGURATION_ENABLED'):
    require(token not in wifi + wifi_h + controller + controller_h,
            f"unused static-IP support remains: {token}")
require('mode=dhcp' in wifi and 'reservation=router_managed' in wifi,
        "DHCP-only configuration report is missing")
require('first_ntfy_connect_failure' in controller and
        'connectivity_entered_offline' in controller and
        'publishFailureNumber == 1U' in controller,
        "bounded failure diagnostics are missing")
require('AppConfig::kVerboseOperationalLogs' in controller and
        'AppConfig::kVerboseOperationalLogs' in ntfy,
        "routine persistence/keep-alive logs are not gated")

for test in ('DoorbellLogicTests.cpp', 'MessageFormatterTests.cpp',
             'check_ntfy_idle.py', 'run_tests.ps1', 'run_tests.sh'):
    require((ROOT / 'tests/host' / test).exists(),
            f"current host test entry point is missing: {test}")
require((SRC / 'RecoveryCompletionPolicy.h').exists(),
        "recovery completion policy is missing")

public_docs = ('README.md', 'CHANGELOG.md', 'CONTRIBUTING.md', 'SECURITY.md',
               'docs/ARCHITECTURE.md', 'docs/HARDWARE.md', 'docs/ROADMAP.md',
               'docs/TESTING.md', 'docs/VALIDATION.md', 'tests/host/README.md')
for name in public_docs:
    document = ROOT / name
    require(document.is_file(), f"public documentation is missing: {name}")
    if not document.is_file():
        continue
    for link in re.findall(r'\[[^\]]*\]\(([^)]+)\)', document.read_text(encoding='utf-8')):
        target = link.strip().strip('<>')
        if target.startswith(('https://', 'http://', 'mailto:', '#')):
            continue
        target_path = (document.parent / unquote(target.split('#', 1)[0])).resolve()
        target_inside = target_path.is_relative_to(ROOT.resolve())
        require(target_inside and target_path.is_file(),
                f"broken public documentation link in {name}: {target}")
        if target_inside:
            require(not any(part in ('local-guides', 'build')
                            for part in target_path.relative_to(ROOT.resolve()).parts),
                    f"public documentation links to local-only material in {name}: {target}")
require(not (SRC / 'Secrets.h').exists(), "private Secrets.h is present")

for forbidden in ('ImouClient', 'SnapshotManager', 'validateJpeg',
                  'snap.jpg', 'snap.part'):
    hits = []
    for path in SRC.glob('*.*'):
        if path.name in ('Secrets.example.h', 'Secrets.h'):
            continue
        try:
            data = path.read_text(encoding='utf-8')
        except UnicodeDecodeError:
            continue
        if forbidden in data:
            hits.append(path.name)
    require(not hits, f"forbidden image reference {forbidden}: {hits}")

if failures:
    for failure in failures:
        print(f"FAIL: {failure}")
    sys.exit(1)
print("Release checks passed.")
