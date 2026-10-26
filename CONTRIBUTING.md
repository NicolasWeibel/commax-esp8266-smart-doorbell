# Contributing

Contributions are welcome when they preserve electrical safety, privacy, and the staged validation process.

## Development principles

- Keep `main` hardware-tested and stable.
- Develop each stage in a dedicated branch.
- Keep commits focused and descriptive.
- Maintain public documentation in English; keep personal setup guides and audit
  reports in the ignored `local-guides/` directory.
- Keep one current test/release workflow. Archive superseded RC copies locally
  rather than publishing parallel historical checklists and runners.
- Do not mix unrelated refactoring with behavior changes.
- Preserve non-blocking input detection as network features are added.
- Document any new wiring requirement or electrical assumption.
- Add or update tests for every behavioral change.

## Suggested workflow

1. Create a branch from `main`.
2. Implement one focused change.
3. Verify compilation for `LOLIN(WEMOS) D1 R2 & mini`.
4. Run the relevant hardware tests.
5. Review the complete diff for secrets and private data.
6. Open a pull request with the test evidence and expected behavior.

## Release candidates

- Keep the current public validation summary in `docs/VALIDATION.md`, separating
  the exact candidate's results from evidence for an earlier build.
- Use `tests/host/run_tests.sh` or `run_tests.ps1`, the idle-clock regression and
  `tests/static/check_release.py` as the current automated entry points.
- Run the publication check on an export without `Secrets.h`. Keep private
  configuration, local binaries, build outputs and raw reports out of Git.
- Use the version in `SmartDoorbell/AppConfig.h` for the release tag, prefixed
  with `v`. Tag the commit that contains the reviewed sources.
- Publish unvalidated candidates as pre-releases from their development branch
  and leave their PR as draft. Promote only after the relevant hardware checks.
- GitHub's source archives should contain the public tracked tree. Do not attach
  a firmware binary compiled with an installation's private credentials.
- Summarize unpublished development iterations in the candidate's changelog entry;
  retain the history of actually published stages.

## Commit style

Use short imperative Conventional Commit messages when practical:

```text
feat: add D5 edge queue
fix: prevent false bell event during boot
refactor: isolate diagnostics formatting
docs: document optocoupler wiring
test: add long-high pulse scenario
chore: prepare stage 2 release
```

## Coding style

- C++11-compatible code for the ESP8266 Arduino Core.
- Two-space indentation.
- `constexpr` for compile-time constants.
- Fixed-size buffers and queues in timing-sensitive code.
- No network, filesystem, dynamic allocation, or serial output inside interrupts.
- Use overflow-safe elapsed-time comparisons with unsigned integers.
- Keep secrets out of source files and logs.

## Pull requests

A pull request should explain:

- what changed;
- why it changed;
- how it was tested;
- the hardware used;
- any memory or timing impact;
- any new privacy or security consideration.
