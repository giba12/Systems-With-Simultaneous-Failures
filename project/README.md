# Space System Reliability Analyzer

A small C++20 tool that implements the per-state signature / MTTF algorithm of Lagos & Ávila
(*Per-State Signatures and Mean Time-to-Failure: An Algorithm for Systems With Simultaneous
Failures*, arXiv:2610.22181) for a **fictional** six-component satellite-support system.
Educational portfolio project: not a certified or flight-qualified analysis.

**Status: Days 1–8 of the plan are complete** (documents and glossary, state model and structure function, brute-force reference, level sweep, signatures, baseline MTTF, simultaneous failures, command-line tool and experiments). Gates G1–G5 are met. What remains is Day 9 (verification report, traceability, cleanup) and Day 10 (delivery). Results: [../docs/experiment-report.md](../docs/experiment-report.md). Everything
below that line is still a skeleton: those functions throw `NotImplementedError` and their
tests report *skipped*. Work through [../docs/project-plan.md](../docs/project-plan.md) day by day.

| Module | State |
|---|---|
| `state` (bitmask helpers, sons, level enumeration, binomials) | done, tested |
| `system_model` (components, Φ, JSON loader, validation, call counter) | done, tested |
| `reference` (brute-force counting vectors, minimal path sets, Eq. (6) MTTF solver) | done, tested |
| `signature`: `sweep()` (Algorithm 1: counting vectors, minimal path sets) | done, tested |
| `signature`: `tail_signature()`, `signature()` | done, tested |
| `failure_model`: `IndependentExponential`; `mttf` | done, tested |
| `failure_model`: Lévy-frailty family (lethal shock, binomial failure rate, any Laplace exponent; n ≤ 16) | done, tested |
| `main.cpp` (`analyze` CLI) | done, tested |
| `python/` (experiment runner, analysis, plots) and `experiments/hypotheses/` | done, tested |

Test status: 99 C++ tests and 32 Python end-to-end tests (run by ctest as `python_end_to_end`), all passing; 7 Python tests skip themselves when pandas / matplotlib are missing.

Measured sweep cost (parallel system, Release build): n = 16 → 0.17 s; n = 20 → 2.9 s, 25 MB; n = 24 → 58 s, 402 MB.

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

## Running the analysis and the experiments

```bash
build/analyze --config configs/satellite_support.json --verify          # one analysis, JSON on stdout
build/analyze --config configs/satellite_support.json --model lsh --beta 0.5 --per-state --out result.json

python3 python/run_experiments.py all                                    # needs experiments/hypotheses/ first
python3 -m venv .venv && .venv/bin/pip install -r python/requirements.txt  # once, for the next two commands
.venv/bin/python python/analyze_results.py
.venv/bin/python python/plot_results.py
```

`analyze` exit codes: 0 success, 1 invalid input or model, 2 usage error, 3 verification failed.
Generated results go to `results/` (not committed; regenerate with the commands above).
