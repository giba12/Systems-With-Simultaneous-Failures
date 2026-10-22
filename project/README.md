# Space System Reliability Analyzer

A small C++20 tool that implements the per-state signature / MTTF algorithm of Lagos & Ávila
(*Per-State Signatures and Mean Time-to-Failure: An Algorithm for Systems With Simultaneous
Failures*, arXiv:2610.22181) for a **fictional** six-component satellite-support system.
Educational portfolio project: not a certified or flight-qualified analysis.

**Status: Days 1–10 and the optional repair-policy extension are complete** (documents and glossary, state model and structure function, brute-force reference, level sweep, signatures, baseline MTTF, simultaneous failures, command-line tool and experiments). Gates G1–G5 are met. Results: [../docs/experiment-report.md](../docs/experiment-report.md); verification: [../docs/verification-report.md](../docs/verification-report.md); deviations and limits: [../docs/deviation-log.md](../docs/deviation-log.md). Everything
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
| `policy` (repair policies: corrective, r-out-of-n, every MTTF-threshold policy; long-term cost) | done, tested (optional P2) |
| `main.cpp` (`analyze` CLI) | done, tested |
| `python/` (experiment runner, analysis, plots) and `experiments/hypotheses/` | done, tested |

Test status: 125 C++ tests, 48 Python end-to-end tests (`python_end_to_end` under ctest) and the traceability check (`traceability`), 127 ctest tests in all, all passing; 8 Python tests skip themselves when pandas / matplotlib are missing. Builds warning-free with g++ 13 and clang++ 18, and passes under AddressSanitizer + UndefinedBehaviorSanitizer.

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
| `include/reliability/failure_model.hpp`, `src/failure_model.cpp` | Independent exponential and Lévy-frailty (lethal shock, binomial failure rate) failure models | 6–7 |
| `include/reliability/mttf.hpp`, `src/mttf.cpp` | Per-state MTTF | 6 |
| `include/reliability/policy.hpp`, `src/policy.cpp` | Repair policies and long-term cost (paper §V) | optional |
| `src/main.cpp` | Command-line tool `analyze` | 6–8 |
| `tests/` | One test file per module; tests are tagged with their test-plan ID | 2–9 |
| `python/` | Experiment runner, result summaries, plots, and the end-to-end tests (`python/tests/`) | 8 |
| `tools/check_traceability.py` | Machine check of requirements → test cases → test code | 9 |
| `experiments/hypotheses/` | The pre-registered hypotheses of the four experiments | 8 |
| `configs/satellite_support.json` | The reference system | 2 |
| `../docs/` | Requirements, test plan, user stories, UML, plan, validation, verification and experiment reports | — |

## Working on the code

* **Tests are tagged.** A comment `// Traceability: TC-C-03` (or `# Traceability: ...` in Python) ties a test to its
  test case in `../docs/test-plan.md`; `tools/check_traceability.py` (run by `ctest`) fails if a requirement, a test
  case or a tag is orphaned. After adding tests, regenerate the matrix:
  `python3 tools/check_traceability.py --write ../docs/traceability-matrix.md`.
* **Check more than "it passes".** The history of this project shows the useful habit: after a change, break the code on
  purpose (a mutation) and confirm a test fails; run the sanitizer build
  (`-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all"`); compare against an independent route
  (`--verify`, the brute-force reference, a closed form).
* **Continuous integration** lives at the repository root, `../.github/workflows/ci.yml` (GitHub reads workflows only
  from the root): g++ and clang++ in Release plus an ASan/UBSan job.

## Running the analysis and the experiments

```bash
build/analyze --config configs/satellite_support.json --verify          # one analysis, JSON on stdout
build/analyze --config configs/satellite_support.json --model lsh --beta 0.5 --per-state --out result.json
build/analyze --config configs/satellite_support.json --c-comp 0.1 --c-sys 1 --c-set 0.05   # also compare repair policies

python3 python/run_experiments.py all                                    # needs experiments/hypotheses/ first
python3 -m venv .venv && .venv/bin/pip install -r python/requirements.txt  # once, for the next two commands
.venv/bin/python python/analyze_results.py
.venv/bin/python python/plot_results.py
```

`analyze` exit codes: 0 success, 1 invalid input or model, 2 usage error, 3 verification failed.
Generated results go to `results/` (not committed; regenerate with the commands above).
