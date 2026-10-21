# Space System Reliability Analyzer

A small C++20 tool that implements the per-state signature / MTTF algorithm of Lagos & Ávila
(*Per-State Signatures and Mean Time-to-Failure: An Algorithm for Systems With Simultaneous
Failures*, arXiv:2610.22181) for a **fictional** six-component satellite-support system.
Educational portfolio project: not a certified or flight-qualified analysis.

**Status: Day 2 of the plan is complete** (state model and structure function). Everything
below that line is still a skeleton: those functions throw `NotImplementedError` and their
tests report *skipped*. Work through [../docs/project-plan.md](../docs/project-plan.md) day by day.

| Module | State |
|---|---|
| `state` (bitmask helpers, sons, level enumeration, binomials) | done, tested |
| `system_model` (components, Φ, JSON loader, validation, call counter) | done, tested |
| `reference` (brute-force checker) | skeleton, next (Day 3) |
| `signature` (Algorithm 1 sweep) | skeleton (Days 4–5) |
| `failure_model`, `mttf` | skeleton (Days 6–7) |
| `main.cpp`, `python/` | skeleton |

Test status: 53 tests, 19 passing, 34 skipped (waiting for their feature).

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

(The first configure downloads GoogleTest and nlohmann/json.)

## Layout

| Path | What it holds | Plan day |
|---|---|---|
| `include/reliability/state.hpp`, `src/state.cpp` | State as a bitmask, sons, binomials | 2 |
| `include/reliability/system_model.hpp`, `src/system_model.cpp` | Components, structure function Φ, validation, JSON loading | 2 |
| `include/reliability/reference.hpp`, `src/reference.cpp` | Independent brute-force checker | 3 |
| `include/reliability/signature.hpp`, `src/signature.cpp` | Algorithm 1: level sweep, counting vectors, signatures, minimal path sets | 4–5 |
| `include/reliability/failure_model.hpp`, `src/failure_model.cpp` | Exponential and lethal-shock failure models | 6–7 |
| `include/reliability/mttf.hpp`, `src/mttf.cpp` | Per-state MTTF | 6 |
| `src/main.cpp` | Command-line tool `analyze` | 6–8 |
| `tests/` | One test file per module; each test is named after its test-plan ID | 2–9 |
| `python/` | Experiment runner, result summaries, plots | 8 |
| `configs/satellite_support.json` | The reference system | 2 |
| `../docs/` | Requirements, test plan, user stories, UML, plan, paper validation | — |

## How to "turn on" a piece

1. Read the pseudo-code comment above the function.
2. Replace the `not_implemented(...)` line with real code.
3. In the matching test, write the check from its pseudo-code and delete the `GTEST_SKIP()` line.
4. Run `ctest`. The test-plan ID in the test name (for example `TC-C-03`) says which requirement it covers.
