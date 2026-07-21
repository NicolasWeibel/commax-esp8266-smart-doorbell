# Contributing

Contributions are welcome when they preserve electrical safety, privacy, and the staged validation process.

## Development principles

- Keep `main` hardware-tested and stable.
- Develop each stage in a dedicated branch.
- Keep commits focused and descriptive.
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
