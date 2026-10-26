# Architecture

## Input and main loop

The PC817 output drives D5/GPIO14 with an external 10 kOhm pull-up. Idle is LOW;
a valid HIGH pulse is confirmed when it returns to LOW. Startup, pulse-duration
and rearm checks run in `BellDetector`, outside the ISR.

```text
D5 interrupt -> fixed raw-edge queue -> BellDetector -> valid bell event
                                                         |
                          +------------------------------+-----------------+
                          |                                                |
                   ONLINE / DEGRADED                                OFFLINE / RECOVERING
                          |                                                |
                   SessionManager                                 OfflineSummaryManager
                          |                                                |
                   DeliveryManager --------------------------------> NtfyClient
```

The ISR captures only the level and microsecond timestamp. It does not allocate
memory, log, access LittleFS or perform network work. The raw queue has 64 slots;
the high-level event queue has 128, so a raw batch can emit diagnostic events
without crowding valid presses out of that batch. Ring buffers reserve one slot.
The queues remain finite and expose overflow/resynchronization diagnostics.

`SmartDoorbell.ino` services the input before and after controller/network work.
HTTPS operations are sequential and use cooperative input pumping where supported.
The ISR continues to capture edges while the main loop is delayed; this is not
a guarantee against loss under an arbitrarily large input burst.

## Sessions and delivery

`SessionManager` maintains the initial 60-second window, cumulative count,
insistence phase and 120-second silence deadline. Restored events can precede the
current boot, so deadlines are reconstructed from their remaining time rather
than from an unsigned subtraction that underflows the new uptime.

`DeliveryManager` schedules initial notifications, quiet count updates and retries.
It tracks desired and confirmed counts separately. When connectivity is lost,
only unreported presses are transferred to the recovery summary. An insistence
notification inherits the confirmed baseline from the first notification.

## Connectivity and offline recovery

`ConnectivityManager` exposes WAITING_FOR_TIME, ONLINE, DEGRADED, OFFLINE and
RECOVERING. Offline declaration considers Wi-Fi loss, repeated publication
failures, failure duration and missing time, with startup grace for normal DHCP.
The current thresholds are centralized in `AppConfig.h`.

`OfflineSummaryManager` stores total undelivered presses, distinct occasions and
the best available last-event timestamp. Recovery publishes one summary. If input
is still active or the count changes during publication, completion is deferred
or the same recovery notification is updated. This is a best-effort input guard;
extreme bounce can still make a concurrent press appear as a separate notification.

`WifiOutageAggregator` combines repeated NO_AP_FOUND events into an initial event,
periodic summaries and a final recovery summary. It is diagnostic state and is not
part of persistent bell counts.

## Time and message formatting

`MonotonicClock` extends uptime beyond the 32-bit hardware timer rollover.
`TimeKeeper` configures the private POSIX timezone and maintains an NTP epoch anchor.
`MessageFormatter` uses the recorded last-event time, not the send time, and compares
local calendar dates to choose a time, yesterday, or an explicit date/year.

For offline events with a validated relative age from the same boot, the formatter
reconstructs the displayed epoch without modifying saved state. An invalidated
timestamp after restart produces an explicit unknown-time message. Normal jobs
without a reliable epoch also retain an unknown-time fallback.

## Storage and TLS

`PersistentStore` validates versioned LittleFS records and maintains temporary and
backup files. The current v4 record is 996 bytes, with migration from the 900-byte
v3 format. The record includes session, delivery jobs and offline summary state.

`TlsManager` requires a valid clock and the LittleFS certificate store.
`NtfyClient` uses ESP8266HTTPClient/BearSSL with connection reuse and certificate
verification. Its idle interval stays in the 32-bit `millis()` domain, avoiding
premature socket closure after rollover. Notification bodies remain text; optional
static click/action links do not implement a camera API.

Operational logs identify a notification sequence rather than printing its private
topic path. Serial commands provide network diagnostics; the current firmware
does not expose an inbound diagnostic server or OTA update service.
