# Verification Report — Space System Reliability Analyzer

| | |
|---|---|
| **Date** | 2026-10-22 |
| **Code version** | `analyze` 0.8.0, after Day 9 cleanup and the optional repair-policy extension |
| **Scope** | requirements in [requirements-spec.md](requirements-spec.md), tests in [test-plan.md](test-plan.md) |
| **Related** | [traceability-matrix.md](traceability-matrix.md) (generated) · [deviation-log.md](deviation-log.md) · [experiment-report.md](experiment-report.md) · [paper-validation-report.md](paper-validation-report.md) |

This report records how the software was verified, in which environment, with which tolerances, what was found
along the way, and what has **not** been verified. It covers the educational, fictional-system scope only; nothing
here is a certification.

## 1. Environment

| Item | Value |
|---|---|
| Platform | Linux 6.18 (WSL2), x86-64, Intel Core i7-9750H @ 2.6 GHz, 12 logical cores |
| Compilers | **g++ 13.3.0** (Ubuntu 13.3.0-6ubuntu2~24.04.1) and **clang++ 18.1.3**, both with `-std=c++20` |
| Build system | CMake 3.28.3, Ninja 1.11.1 |
| Build types | **Release** (`-O3 -DNDEBUG`) and **Debug + AddressSanitizer + UndefinedBehaviorSanitizer** (`-fno-sanitize-recover=all`) |
| Warnings | `-Wall -Wextra -Wpedantic` on the library and CLI; also checked once with `-Wshadow -Wconversion -Wsign-conversion -Wold-style-cast -Wuseless-cast -Wdouble-promotion` (one real finding fixed, one portability-motivated cast kept) |
| Test frameworks | GoogleTest 1.14.0 (C++), Python `unittest` (standard library) |
| Libraries | nlohmann/json 3.11.3 (MIT) |
| Python | 3.12.3, numpy 2.5.4, pandas 3.0.6, matplotlib 3.11.2 (in `project/.venv`) |

## 2. Test commands and results

```bash
cmake -S project -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel
ctest --test-dir build --output-on-failure           # C++ tests + python_end_to_end + traceability
```

| Configuration | Result |
|---|---|
| g++ 13.3.0, Release | **124 / 124 passed**, 0 compiler warnings |
| clang++ 18.1.3, Release | **120 / 120 passed**, 0 compiler warnings |
| g++ 13.3.0, Debug + ASan + UBSan | **120 / 120 passed**, no sanitizer report |
| Release, `ctest` repeated 5 times | 5 × 124 / 124 |
| C++ tests, 10 repetitions × 3 shuffle seeds (1, 77, 2026) | 30 × 122 / 122: no flakiness, no order dependence |
| Python tests with only the standard library (no pandas / matplotlib) | 43 run, 8 skip themselves cleanly |
| Fresh clone-style configure + build (no cached state) | clean, no CMake warning |

**Test inventory.** 122 C++ tests (`Signature` 23, `Mttf` 21, `FailureModels` 22, `Reference` 16, `StructureFunction` 13,
`Policy` 14, `State` 8, `Properties` 5) and 46 Python end-to-end tests (`test_cli` 23, `test_experiments` 18,
`test_traceability_tool` 5). `ctest` runs the C++ tests individually (122) plus the Python suite as one test and the
traceability check as one test: 124. About 7 400 lines of source, tests and scripts.

**Traceability (VER-003).** `project/tools/check_traceability.py` reads the requirements, the test plan and the tags in
the test code and fails on any orphan. Current state: **39 requirements, 107 test cases, 104 test cases tagged in
automated test code, 3 verified another way** (timing in Experiment 4, CI, and documentation review), **0 problems**; the
optional repair-policy cases are now implemented and tagged. The generated matrix is
[traceability-matrix.md](traceability-matrix.md); the tool runs under `ctest`.

## 3. How correctness is established

1. **Two independent implementations.** The level sweep (paper Algorithm 1) is checked against a brute-force reference
   that counts sub-states by direct subset enumeration and uses none of the recurrence's code; the MTTF of Eq. (7) is
   checked against a direct solution of Eq. (6) that never touches a counting vector. Agreement is tested on every
   state of every test system: the satellite, the paper's Fig. 1 system, the bridge network, series/parallel and
   k-out-of-n systems up to 16 components, 60 random monotone systems and 60 random **arbitrary** Boolean functions
   (the paper claims no assumption on Φ).
2. **Independent oracles that need neither implementation.** Hand-derived and closed-form values: series and parallel
   (1/n, harmonic numbers), k-out-of-n (Σ 1/(jλ) on every state), the satellite's MTTF on **all 64 states** from an
   integral formula (independent exponential and lethal-shock), the bridge network's signature (0, 1/5, 3/5, 1/5, 0),
   and the paper's own Fig. 1 numbers (counting vectors, 7/6, 86 %/43 %). The figure's policy costs (1/10, 11/15, 97/105) were reproduced once during validation by a separate script; they are not in the test suite because the repair-policy feature is not implemented.
3. **Closed forms for the transition kernel.** The lethal-shock kernel (one component or all) and the binomial-failure-rate
   kernel (a binomial conditioned on at least one failure) check Eq. (20) against numbers never produced by an
   alternating sum.
4. **The paper's theorems tested on the code.** Lemma 4 (MTTF never rises when more components fail) on more than 40 000
   state pairs under four models; Theorem 5 (signature dominance ⇒ MTTF order, strictly) on thousands of pairs; a concrete
   counterexample to its converse.
5. **Verification built into the tool.** `analyze --verify` re-checks any result against the reference, Eq. (6) and
   numerical sanity; a labelled fault-injection hook proves it can fail (exit code 3).
6. **Mutation checking.** After each phase the code was deliberately broken to confirm that the tests notice.

### 3.1 Mutation record

| Phase | Deliberate bugs | Caught |
|---|---|---|
| Day 2 (state, system model) | `sons()` skips bit 0; validation ignores monotonicity | 2 / 2 |
| Day 3 (reference) | wrong state evaluated; path-set check only one sub-state | 2 / 2 |
| Day 4 (sweep) | missing division by k; wrong son rank; Φ asked twice; wrong path-set condition | 4 / 4 |
| Day 5 (signatures) | sign flipped; wrong binomial; failed system not empty | 3 / 3 |
| Day 6 (MTTF) | off-by-one increment; wrong tail entry; Eq. (6) without 1/ν; without binomial weight | 4 / 4 |
| Day 7 (Lévy-frailty) | Eq. (20) sign; Eq. (21) typo reintroduced; wrong divisor; β swapped; recursion drops a term | 5 / 5 |
| Day 8 (CLI) | wrong exit code; `--lambda` ignored; wrong operational count; verification verdict forced to pass | 3 / 4 at first |
| Optional P2 (policies) | set-up cost forgotten in the repair cost (caught **only** by the signature closed forms, since the paper's Fig. 1 has no set-up cost); holding time 1/ν dropped; transition weight without the 1/C split; r-out-of-n repairing one failure late; threshold using `<` for `<=` | 5 / 5 |
| Table I models ([LFMO-C], [LFMO-G], [LFMO-IG], [LFMO-S]) | five deliberate bugs in the new Ψ functions and the calibration | 4 / 5 (see below) |

The survivor among the Table I mutations changed a constant factor of Ψ for [LFMO-IG]. It is an **equivalent mutant**, not
a test gap: Ψ is normalised by Ψ(1) = 1, so a constant factor cancels in every ratio Ψ(x)/Ψ(1) that the code and the
tests can observe.

The one survivor (verification verdict forced to pass) exposed that **no test made the verification fail**; fault
injection and three new tests closed the gap, after which the same mutation is caught.

## 4. Numerical tolerances

| Where | Tolerance | Basis |
|---|---|---|
| Counting vectors, minimal path sets | **exact** (integers) | no rounding |
| Signature entries: sum to 1 | ≤ 1e-12 in the tool, ≤ 1e-14 asserted in tests | each entry from an exact integer numerator, rounded once |
| MTTF, Eq. (7) vs Eq. (6) (`--verify`) | relative ≤ 1e-9 | measured ≈ 1e-16 for [Exp]; ≤ 2e-10 for the Lévy models at n = 16 |
| MTTF in unit tests vs closed forms | relative 1e-12 (independent exponential), 1e-11 / 1e-9 (lethal shock, larger n) | measured differences below these |
| Transition kernel validity | rows sum to 1 within 1e-9, entries ≥ −1e-9 (tiny negatives clamped to 0) | guard in the constructor |
| Ψ calibration | \|Ψ(1) − 1\| ≤ 1e-12 | unit mean lifetime |
| Limits | system n ≤ 24; brute-force reference n ≤ 16; Lévy-frailty models n ≤ 16 | validation cost; ≈ 3ⁿ work; measured precision (relative error ≈ 5e-15 at n = 6, 3e-12 at 12, 2e-10 at 16) |

## 5. Defects and near-misses found by the verification work

Most were found by the tests, the mutation checks or by looking at the numbers, **not** by luck; several were mistakes
in my own expectations rather than in the code, which is itself worth recording.

| Phase | What | How found | Resolution |
|---|---|---|---|
| Paper | Eq. (21) prints its lower summation limit as w−k+1 (gives 0 at k = 0, even for exponential lifetimes) | rendering the PDF page and testing against exact arithmetic | corrected to w−k; a test pins the typo |
| Day 1 | wrong test expectation: "more common-cause failure lowers absolute MTTF" | exact computation under the paper's calibration | corrected; the redundancy *ratio* falls, absolute MTTF rises |
| Day 1 | outdated sequence diagram (all states enumerated first) | reading Algorithm 1 | diagram redrawn |
| Day 2 | my XOR example for "not monotone" actually fails the endpoint check first | the test would not have exercised monotonicity | replaced by a function that passes the endpoints |
| Day 5 | two hand-derived expectations were wrong (series-of-parallel block; ordering of path sets) | tests failed on correct code | expectations re-derived and corrected |
| Day 6 | no pair with crossing signatures in my random systems, so the "converse is false" search found nothing | the test failed | concrete counterexample found by search and fixed in the test |
| Day 7 | a precision limit: Eq. (20)/(21) lose accuracy; behaviour at n = 17 differs by parameter | measuring against exact rational arithmetic | hard cap n = 16; guard in the constructor |
| Day 7 | increment-comparison tolerance too tight for tiny late-stage values | test failed | both routes checked against exact values; tolerance taken relative to the scale of the increments |
| Day 8 | peak memory constant at ≈ 19 MB (`ru_maxrss` inherited across exec) | looking at the numbers | read `VmHWM` instead |
| Day 8 | saved results contained a temporary-directory path, so were not reproducible | the byte-for-byte reproducibility test | file name only |
| Day 8 | a bad `--analyze` path silently fell back to the default binary | a test | an explicit path is used as given or is an error |
| Day 8 | an impossible counting vector crashed verification instead of failing it | fault-injection test | reported as a failed verification, exit code 3 |
| Day 8 | the "verification failed" path was untested | surviving mutant | fault-injection hook and tests |
| Day 8 | name collision in the plot script (matplotlib `Path` shadowed `pathlib.Path`) | running it | renamed |
| P2 | Experiment 5's deterministic result file contained the tool's volatile timing block, so two runs differed | the byte-for-byte reproducibility test | timing split out as in the other experiments |
| P2 | an unpinned claim: the cost curve over γ "can rise and fall" had no concrete example | needing a test for it | random search over the paper's cost grid found an 8-component example, now a test |
| Day 9 | **CI workflow was in `project/.github/`, where GitHub never reads it** (the repository root is the parent folder) | reviewing the workflow against how GitHub locates workflows | moved to the repository root |
| Day 9 | a unit test asserted a run-time prediction, failing under the sanitizer build | executing the CI jobs locally | structural predictions are asserted; timing predictions are not |
| Day 9 | traceability gaps: 22 test cases lacking tags, 3 requirements missing from the matrix, no test for INT-002 | the new traceability checker | tags added, rows added, test TC-G-10 written |
| Day 9 | checker false positives (digits in prose read as test numbers) | the checker's first run | grammar tightened, regression tests added |

## 6. Requirement status

| Requirement | Status | Evidence |
|---|---|---|
| SYS-001 … SYS-005 | Met | state/structure-function tests, validation tests, CLI error tests |
| ALG-001 … ALG-005 | Met | sweep = reference on every state; Φ evaluated exactly 2ⁿ times |
| ALG-006 | Met | time and memory growth measured (Experiment 4), and the sweep **measures** its integer additions and divisions: exactly n(n+1)2ⁿ⁻² and n·2ⁿ⁻¹ for n = 2…14 (TC-C-22) |
| REL-001, REL-003, REL-005 | Met | oracles, closed forms, Eq. (7) = Eq. (6) |
| REL-002, REL-004 | Met, with a limit | lethal-shock and binomial-failure-rate models, calibration by β; **n ≤ 16** |
| VER-001 … VER-004 | Met | see §3 |
| EXP-001 … EXP-003 | Met | byte-identical deterministic results; four experiments; tables and figures from saved results |
| EXP-004 (P2) | Met, **with limits** | policies evaluated by direct recursion (n ≤ 14), every threshold breakpoint searched; Fig. 1 costs reproduced; closed forms and the paper's Proposition 3 as independent checks (TC-J-01…06); Experiment 5; the optimal state-based policy (Dinkelbach) checked against an exhaustive search of all policies on 7 small systems × 4 failure models × 12 cost pairs (TC-J-07…10), and four deliberate bugs in it all caught. **Not implemented:** the paper's single-pass evaluation (Proposition 3 over classes of equal counting vectors) |
| DOC-001, DOC-002 | Met | [deviation-log.md](deviation-log.md), [experiment-report.md](experiment-report.md) |
| INT-001 … INT-004 | Met | CLI tests (output, errors, exit codes, determinism); the Python layer only calls the CLI (TC-G-10) |
| NFR-001, NFR-006, NFR-008, NFR-009 | Met | tolerances in §4; provenance in every result file; deviation log; precision strategy |
| NFR-002, NFR-003 | Met | satellite analysis 0.08 ms; practical limit measured (n = 20 in 3.2 s / 35 MB; n = 24 in 58 s / 402 MB) |
| NFR-004 | Met | one test file per module plus extras |
| NFR-005 | **Partly met** | g++ and clang++ on Linux/WSL; other operating systems not tried |
| NFR-007 | **Partly met** | the workflow is written, syntax-checked, and every job's commands were executed locally; it **has not run on a CI service** because the repository has no remote |

## 7. What has not been verified

* The CI workflow on an actual CI service (no remote), and any operating system other than Linux.
* Lévy-frailty models above n = 16, and models other than lethal shock and binomial failure rate (the stable
  subordinator is exercised in tests; the paper's other four are not implemented).
* The paper's empirical results (22 benchmark networks, up to 30 components, repair-policy cost tables) — not a goal of
  this project.
* The paper's single-pass evaluation of all thresholds (the optimal state-based policy exists but uses the same 3ⁿ enumeration); policy evaluation
  above n = 14; any repair-policy claim about realistic, larger systems (Experiment 5 uses systems of 3 to 8 components,
  one of them chosen because its cost curve is non-unimodal).
* Agreement with a third implementation (the authors' repository was not consulted).
* Behaviour with non-identical components, which the paper's assumptions exclude.

## 8. Reproducing this verification

```bash
cmake -S project -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel && ctest --test-dir build
python3 project/tools/check_traceability.py --write docs/traceability-matrix.md
# sanitizers
cmake -S project -B build-san -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer" \
  && cmake --build build-san --parallel && ctest --test-dir build-san
# shuffled, repeated C++ tests
build/reliability_tests --gtest_repeat=10 --gtest_shuffle --gtest_random_seed=77
```

## 9. Fresh-copy check (gate G7, 2026-10-22)

A copy of the repository was made the way a clone would see it (tracked and untracked files, minus everything git
ignores: no `build/`, `.venv/` or `results/`) into an empty folder, and the README's commands were followed literally:

| Step | Result |
|---|---|
| `cmake -S project -B project/build -DCMAKE_BUILD_TYPE=Release` | 0 warnings |
| `cmake --build project/build --parallel` | 0 warnings or errors (about 2 min with the dependency downloads) |
| `ctest --test-dir project/build` | **101 / 101 passed** |
| `project/demo.sh` | ran to the end; numbers identical to the documented transcript |
| `analyze --config … --verify`; `--model lsh --beta 0.5 --per-state --out …` | MTTF 0.7, verification passed; 0.914286 = 32/35, 64 state records |
| `python3 python/run_experiments.py all` (standard library only) | all four experiments completed |
| `python3 -m venv .venv && pip install -r python/requirements.txt`, then `analyze_results.py` and `plot_results.py` | tables, verdicts and all four figures produced |

**Observation.** The repeat run reproduced every structural result exactly but not the timing-based verdicts: one
Experiment 4 criterion that had been rejected passed (see [experiment-report.md](experiment-report.md) §6.1). Predictions
about run time should be treated as noise-sensitive.

### 9.1 Repeated after the repair-policy extension (2026-10-22)

The fresh-copy check was repeated on the final tree, including the optional repair-policy work (75 files, nothing from
`build/`, `.venv/` or `results/`):

| Step | Result |
|---|---|
| configure and build (Release) | 0 warnings, 0 errors (about 2 min 50 s with the dependency downloads) |
| `ctest` | **120 / 120 passed** |
| `project/demo.sh` | ran to the end, five steps; step 5 prints the paper's Fig. 1 costs 0.9238 / 0.7333 / 0.1000 = 97/105, 11/15, 1/10 |
| `analyze … --c-comp 0.1 --c-sys 1 --c-set 0.05` | 4 threshold policies, best r = 1 |
| `run_experiments.py all`, new virtual environment, `analyze_results.py`, `plot_results.py` | five experiments saved, five figures drawn |
| verdicts | Exp 1 15/15, Exp 2 6/8, Exp 3 9/9, Exp 4 7/8 (the same timing-sensitive criterion as in §9), **Exp 5 8/8** |
