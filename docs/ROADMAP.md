# Roadmap

The project advances through hardware-tested stages. Keep `main` on the latest
validated stage and develop candidates on a dedicated branch.

## Released

- **Stage 1 — `v0.1.0-stage1`:** firmware foundation, reset/memory diagnostics and
  non-blocking status indication.
- **Stage 2 — `v0.2.0-stage2`:** isolated D5 bell input, pulse filtering, arming,
  rearming and detector diagnostics, tested with the real intercom.

## Current: Stage 3 connected firmware candidate

Implemented: text notifications, session grouping, retries, verified TLS, local
time, restart persistence, Wi-Fi recovery and undelivered-event summaries.
Automated tests and ESP8266 compilation passed. The corrected base has hardware
evidence; the exact local-time candidate still needs its physical check.

Before promoting the candidate:

1. Load the candidate while preserving its flash layout and certificate store.
2. Confirm a real bell notification and its local time, then a grouped update.
3. Confirm offline recovery when practical and observe normal installed operation.
4. Record the result in [Validation](VALIDATION.md), complete the PR and publish
   the validated stage.

## Future work

- Authenticated, signed OTA updates with a bounded maintenance window.
- Restricted, authenticated diagnostics over the local network.
- Additional persistence recovery and scheduling improvements only when justified
  by a reproduced problem or a documented requirement.

These features are proposals, not capabilities of the installed firmware. OTA
and remote diagnostics require an initial physical firmware upload and their
own resource, security and hardware validation. Camera/image integration remains
outside the current text-only release scope.
