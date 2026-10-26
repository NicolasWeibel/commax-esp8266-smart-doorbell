# Validation

## Candidate

Firmware: `1.0.0-stage3-rc1`.
Development branch: `stage-3-complete-firmware`.

This is a Stage 3 candidate, not a claim of a new stable release. The natural-time
build has not yet been uploaded to the installed intercom. Keep its PR as draft
and any published candidate marked as a pre-release until its hardware check.

## Completed checks

| Area | Result and scope |
| --- | --- |
| Host logic | Sessions, delivery counts, connectivity, offline grouping, restore timing, queued input batches and recovery completion passed. |
| Message formatting | Last-event time, ART3 local dates, midnight/year boundaries, leap day, relative offline time and unknown-time fallback passed. |
| ntfy idle clock | Before, across and after `millis()` rollover, including exact zero; isolated timer test passed. |
| Publication check | Passed on a public source export without private configuration. |
| ESP8266 compilation | Complete sketch passed with Core 3.1.2, D1 mini, 160 MHz and 4M2M; no compiler warnings/errors in the recorded build. |
| Release identity | Firmware version, boot text and public documentation aligned with Stage 3 RC1; bell/delivery logic unchanged. |

The host environment substitutes Arduino APIs and the local-clock formatting
service. It does not establish real NTP, HTTP/TLS delivery or physical signal quality.

## Hardware evidence for the corrected connected base

The earlier corrected base, before the local-time message change, was tested on
a Wemos D1 mini/ESP8266EX with a 4 MB flash and PC817 input. Observations included:

- Wi-Fi/DHCP, NTP, LittleFS certificate loading and verified ntfy delivery.
- Initial notification and count update, with receipt confirmed on the phone.
- Session restoration after reset and correct transition to insistence.
- Direct D5 pulse acceptance and return to armed LOW.
- Offline count persistence across restart and a single recovery summary.
- A real access-point outage and recovery, with the expected summary received.
- A real COMMAX bell notification after soldering and installation.

These observations belong to the corrected base; they are not measurements of
the exact natural-time candidate. A longer earlier field run used an older
firmware and is not presented as a soak test of this build.

## Compiled resources

Measured with the local installation configuration; another private configuration
may change build size slightly.

| Resource | Corrected base | Natural-time candidate |
| --- | ---: | ---: |
| Static RAM | 40632 B | 40748 B |
| IRAM, including reserved instruction cache | 61239 B | 61239 B |
| Code in flash | 465204 B | 465016 B |

The candidate adds 116 bytes of static RAM relative to the corrected base,
including its current version/diagnostic strings, with unchanged IRAM. Runtime heap
and restart behavior of the exact candidate remain hardware observations to make;
the compilation figures alone do not establish them.

## Pending before stable promotion

- Upload the exact candidate while preserving its flash layout and LittleFS.
- Confirm a real bell and grouped update show the correct local time.
- Confirm a recovery message when practical and observe normal installed operation.
- Record the outcome and resolve any reproducible regression before merging.

The current firmware has no OTA or network diagnostic service. Software tests run
on a PC cannot install it on a mounted device without a physical connection.
Detailed personal reports and private artifacts are retained outside the public
repository; only this sanitized summary is intended for release review.
