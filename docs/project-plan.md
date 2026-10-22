# Project Plan — Space System Reliability Analyzer

| | |
|---|---|
| **Version** | 1.0 |
| **Date** | 2026-10-20 |
| **Basis** | Project guide §9 (ten-day schedule), [requirements-spec.md](requirements-spec.md) v1.1, [test-plan.md](test-plan.md) v1.1, [paper-validation-report.md](paper-validation-report.md) |
| **Paper** | Lagos & Ávila, arXiv:2610.22181 (`paper3.pdf`) |

## 1. Goal and scope

Build a small, auditable C++20 tool that implements the paper's per-state signature algorithm (Algorithm 1) and per-state MTTF for a fictional six-component satellite-support system. Verify it independently, run four reproducible experiments from Python, and deliver a report that separates **reproduced paper results** from **new experiments**.

| Priority | In scope | Out of scope |
|---|---|---|
| **P0** | System model and state encoding · counting vectors and signatures at every state (level sweep) · baseline [Exp] MTTF · brute-force reference · analytical and paper oracles · property tests · Python experiments 1, 3, 4 · report, README, CI | Certified or flight-qualified analysis · non-identical components · n beyond a few dozen · BDD extensions |
| **P1** | Simultaneous failures ([LSh] minimum) · minimal path sets · Experiment 2 · complexity comparison with the paper's bounds · precision handling | |
| **P2** | Repair-policy comparison (threshold policy vs r-out-of-n) with Fig. 1 oracle | Optimal policy by Dinkelbach's method (paper §V-B) |

**Guiding rule (guide §9):** a small correct implementation beats a broad unfinished one. If time runs short, protect P0 and cut from the bottom of the table.

## 2. Timeline

Ten working days. The calendar column is a **proposal** starting Wednesday 2026-10-21; shift it to the actual start date.

| Day | Date | Focus | Gate at end of day |
|---|---|---|---|
| 1 | Wed 10-21 | Paper, requirements, glossary | G1 |
| 2 | Thu 10-22 | State model and structure function | — |
| 3 | Fri 10-23 | Brute-force reference | G2 |
| 4 | Mon 10-26 | Paper's recursive algorithm (level sweep) | — |
| 5 | Tue 10-27 | Signature verification, path sets | G3 |
| 6 | Wed 10-28 | Baseline MTTF | G4 |
| 7 | Thu 10-29 | Simultaneous-failure model (P1) | G5 |
| 8 | Fri 10-30 | Python experiments | — |
| 9 | Mon 11-02 | Verification, CI, cleanup | G6 |
| 10 | Tue 11-03 | Portfolio delivery | G7 |

```
Day            1    2    3    4    5    6    7    8    9    10
Paper/reqs     ████
State model         ████
Reference                ████
Sweep (Alg 1)                 ████
Signatures/MPS                     ████
Baseline MTTF                           ████
Simultaneous (P1)                            ░░░░
Experiments                                       ████ ░░░░
Verify/CI                                              ████
Deliver                                                     ████
(░ = may slip or be dropped; P2 policies only if everything above is green)
```

**Critical path:** state model → reference → recursive sweep → signature tests → baseline MTTF → experiments → report. Everything else hangs off this chain.

## Progress

| Day | Status | Notes |
|---|---|---|
| 1 | **Done (2026-10-21)** | Requirements, test plan, plan and validation report written. Notation glossary written ([notation-glossary.md](notation-glossary.md)). Eq. (21) checked against the typeset PDF: the paper has a typo in the lower limit (correct: w−k). **Gate G1 met.** |
| 2 | **Done (2026-10-21)** | `state` and `system_model` implemented; 19 tests pass, including sanitizer runs. Decisions: GoogleTest, nlohmann/json 3.11.3, max n = 24. |
| 3 | **Done (2026-10-21)** | `reference` implemented (brute-force counting vectors, minimal path sets by definition); 14 tests, sanitizer-clean, mutation-checked. `solve_mttf_direct` waits for Day 6 as planned. **Gate G2 met:** the reference reproduces ORC-1…5, the paper's Fig. 1 system and closed-form series/parallel/repeated-pair vectors. |
| 4 | **Done (2026-10-21)** | Level sweep (Algorithm 1) implemented with dense per-level matrices, combinatorial-number-system ranks for son lookup, two levels in memory, exact Lemma 1 divisibility check. Φ is evaluated exactly 2ⁿ times. 13 tests; sweep = brute force on every state of the satellite, series/parallel up to n = 16, 60 random monotone systems **and 60 arbitrary (non-monotone) Boolean functions** (the paper claims no assumption on Φ). Sanitizer-clean; 4 deliberate bugs all caught. Measured: n = 20 → 2.9 s / 25 MB; n = 24 → 58 s / 402 MB (≈ ×4.2 time and ×4 memory per +2 components, as O(n²2ⁿ) and O(√n·2ⁿ) predict). Pulled forward from Day 5: minimal-path-set recording and random-system tests (TC-C-10, TC-E-12, TC-F-05, TC-I-03). |
| 5 | **Done (2026-10-21)** | `tail_signature()` and `signature()` implemented; each signature entry comes from an exact integer numerator, so it is rounded once and the entries sum to 1 within 1e-14. 9 new tests: oracles ORC-1…5 and the paper's Fig. 1 (0, 2/3, 1/3); the classic 5-component **bridge network (0, 1/5, 3/5, 1/5, 0)**; k-out-of-n point-mass signatures for all n ≤ 9; series-of-parallel block by hand count; invariance under relabelling; non-negativity, sum-to-1 and monotone tail on > 1000 operational states of satellite, bridge, series/parallel and 40 random monotone systems. Sanitizer-clean; 3 deliberate bugs caught. **Gate G3 met:** sweep = reference on all states of all test systems, Φ evaluated exactly 2ⁿ times. |
| 6 | **Done (2026-10-21)** | `IndependentExponential`, `mttf_from_counting_vector` (Eq. (7)), `compute_all_mttf` (inside the sweep) and `solve_mttf_direct` (Eq. (6), a separate route that never touches counting vectors). 21 new tests: oracles 1/(nλ), 1.5/λ, 0.7, 8/15, 5/12 and the paper's Fig. 1 (m(**1**) = 7/6 and the 86 % / 43 % ratios); **all 64 satellite states against an independent integral closed form**; k-out-of-n closed form on every state for n ≤ 9; Eq. (7) = Eq. (6) on 80+ systems × 3 rates (relative 1e-12); 1/λ scaling from 1e-6 to 1e3; parallel(n) = harmonic number. Paper properties on the code: **Lemma 4** (monotone MTTF) on > 10⁴ state pairs, **Theorem 5** (dominance ⇒ MTTF order, strict) on > 1000 pairs, and a deterministic counterexample to its converse (x: W = [1,3,3,1,0], MTTF 1.0; y: W = [1,4,2,0,0], MTTF 0.75). Sanitizer-clean; 4 deliberate bugs caught (incl. an off-by-one in the increment). Satellite analysis: 0.1 ms. **Gate G4 met** (all P0 core tests green; local fresh build has no warnings and ctest passes; the CI workflow itself has not run because the repository has no remote). **Go for Day 7.** |
| 7 | **Done (2026-10-21)** | `LevyFrailty` (generic Laplace exponent Ψ), `make_lethal_shock` and, as a bonus, `make_binomial_failure_rate`. Kernel from Eq. (20) and increments by the non-negative recursion, both in `long double`; Eq. (21) with the corrected lower limit kept as the cross-check. The constructor refuses models whose kernel is not a distribution; **hard cap n = 16** from measured accuracy (≈ 2e-10 at n = 16). 22 new tests: kernel vs independent closed forms for [LSh] and for [BFR] (the latter tests *all* of Eq. (20)); ORC-8 values (32/35, 6003/7735, 147467/205436); **all 64 satellite states** vs an independent integral under [LSh]; series/parallel closed forms; β = 0 reproduces [Exp]; β = 1 (everything fails at once); Eq. (6) = Eq. (7) on 28 systems × 5 models; Lemma 4 and Theorem 5 now also checked under [LSh] and [BFR]; the paper's printed Eq. (21) typo is pinned by a test. The corrected TC-D-04 holds: MTTF 0.700 → 0.914 as β rises, redundancy ratio 4.20 → 3.20. Sanitizer-clean; 5 deliberate bugs caught (incl. reintroducing the typo). **Gate G5 met.** |
| 8 | **Done (2026-10-21)** | `analyze` CLI (JSON in/out; `--model exp/lsh/bfr`, `--per-state`, `--verify`; exit codes 0/1/2/3) and the Python suite: `run_experiments.py` (refuses to run without a hypothesis; deterministic result file kept separate from timing, byte-identical across runs), `analyze_results.py` (tables + a PASS/REJECTED verdict per pre-registered prediction), `plot_results.py` (four figures from saved tables only, validated palette, no dual axes). **Hypotheses were committed (`0a3e51c`) before the runner existed.** Results: Exp 1 15/15, Exp 3 9/9 predictions supported; **Exp 2 6/8 and Exp 4 6/8 — four predictions rejected and reported as such** ([experiment-report.md](experiment-report.md)). Bugs found by the tests/inspection and fixed: peak-memory artefact (`ru_maxrss` inherited across exec → now `VmHWM`), config path made results non-reproducible, silent fallback for a bad `--analyze` path, an impossible vector during verification crashed instead of failing verification. 32 Python end-to-end tests (run by ctest), sanitizer-clean, mutation-checked; verification-failure path tested through a labelled fault-injection hook. |
| 9–10 | Not started | Next: Day 9 — full verification report (compiler, build type, tolerances, results), complete the traceability matrix and limitations, CI sanity; Day 10 — README, report assembly, demo. |

## 3. Day-by-day work plan

Requirement and test IDs refer to the requirements spec and test plan.

### Day 1 — Paper and requirements
- Read paper §I-A, §III, §IV; write a one-page **notation glossary** (x, |x|, Φ, Sons(x), W, S̄, s, m(x), T_{k:w}, δ_{w,k}, Ψ, β).
- Write pseudocode for Lemma 1 and Algorithm 1 (levels from the all-failed state up).
- Confirm the open items: tolerance, maximum n, test framework (GoogleTest or Catch2); **check Eq. (21) against the typeset PDF**.
- Create the repository skeleton from guide §5; copy `docs/` in.
- **Deliverable:** glossary, pseudocode, decisions recorded. **Gate G1:** glossary reviewed against the paper; open items 1, 3, 4 closed.

### Day 2 — State model
- `state.hpp`: n-bit mask, bit 1 = working, |x|, sons, sub-state iteration.
- `system_model`: components, labels, JSON loader (`satellite_support.json`), structure function Φ for the reference system.
- Validation: n > 1, non-trivial Φ, monotone, IDs unique, n limit; irrelevant components accepted.
- **Tests:** TC-C-01, TC-C-02, TC-C-11, TC-C-12, TC-E-01…E-06, TC-E-10…E-12, TC-A-01, TC-B-01.
- **Requirements:** SYS-001, SYS-003, SYS-004, SYS-005, ALG-001.

### Day 3 — Reference implementation
- Brute-force counting vectors by direct subset enumeration (must **not** use the recurrence).
- Brute-force minimal path sets by definition.
- Validate the reference alone against hand oracles ORC-1…ORC-5.
- **Requirements:** ALG-005. **Gate G2:** the reference reproduces ORC-1…5 (this is what makes it trustworthy as a check).

### Day 4 — Recursive algorithm
- Implement Lemma 1 as the level sweep of Algorithm 1: Φ evaluated once per state, two levels in memory.
- Compare **every state** against the reference.
- Instrument Φ call count.
- **Tests:** TC-C-03, TC-C-04, TC-C-05, TC-C-09, TC-F-01, TC-F-06.
- **Requirements:** ALG-002, ALG-004, VER-001.

### Day 5 — Signatures and path sets
- Tail signature and signature at every state.
- Series, parallel, mixed-topology and irrelevant-component systems; random monotone systems (n ≤ 8, fixed seed).
- Minimal path sets from W₀ = 1 and W₁ = 0.
- Reproduce **paper Fig. 1** counting vectors and signature (ORC-6, structural part).
- **Tests:** TC-A-02/03, TC-B-02/03, TC-C-06, TC-C-10, TC-C-14, TC-F-02, TC-F-05, TC-I-03.
- **Requirements:** ALG-003, SYS-002. **Gate G3:** all P0 algorithm tests green; no unexplained mismatch with the reference.

### Day 6 — Baseline MTTF
- `failure_model`: [Exp] with parameter validation; increments δ = 1/(λ(w−k)).
- `mttf`: Eq. (7) per state.
- **Eq. (6) cross-check:** a separate linear solve; compare on all 64 states.
- Property tests: Lemma 4 (monotone MTTF) and Theorem 5 (dominance).
- Reproduce Fig. 1: m(**1**) = 7/6 and the per-state ratios.
- **Tests:** TC-A-04, TC-B-04/05, TC-C-07, TC-C-08, TC-C-13, TC-C-14, TC-E-07, TC-F-03, TC-F-04, TC-F-08, TC-I-01, TC-I-02.
- **Requirements:** REL-001, REL-003, REL-005, VER-004. **Gate G4 (go/no-go for P1):** all P0 core tests green and CI builds. If red, **do not start Day 7** — use the day to fix.

### Day 7 — Simultaneous failures (P1; skip if G4 not met)
- Implement the Lévy-frailty Marshall-Olkin interface (Ψ → ν_w, P_{w,w′}, δ) and the lethal-shock model [LSh], with β calibration (Ψ(1) = 1).
- Handle precision: `double` for n ≤ 12 (error ≈ 3e-12), extended precision or a hard limit beyond (NFR-009).
- Verify: rows of P sum to 1; β = 0 equals [Exp]; closed form ORC-8; Eq. (6) vs Eq. (7) under [LSh].
- **Tests:** TC-D-01…D-08, TC-E-08, TC-F-07.
- **Requirements:** REL-002, REL-004, NFR-009. **Gate G5:** ORC-8 values match for β = 0.05, 0.2, 0.5. If this is not done by end of day, record it as a known gap and proceed; do not let it consume Day 8.

### Day 8 — Python experiments
- `run_experiments.py`, `analyze_results.py`, `plot_results.py`; each run saves raw output, configuration, seed, and a hypothesis written **before** the run.
- Experiment 1: redundancy vs reliability (series, pair, six-component; [Exp]).
- Experiment 3: equal failure counts (ORC-5: P1+C1 failed → 5/12; P1+P2 failed → 0).
- Experiment 4: scaling (runtime, memory, Φ calls, operation counts) against the paper's O(n²2ⁿ) / O(√n·2ⁿ) bounds; brute force only where practical.
- Experiment 2 (if Day 7 done): β ∈ {1, 5, 10, 20, 50} %; report redundancy ratio vs a series baseline, **not only absolute MTTF**.
- **Tests:** TC-G-01…G-07, TC-F-05. **Requirements:** EXP-001, EXP-002, EXP-003, ALG-006.

### Day 9 — Verification and cleanup
- Full test run; record compiler version, build type, test command, tolerances, results (verification report).
- CI workflow (build + unit tests on each push); fix flaky or tolerance-dependent tests.
- Complete the traceability matrix; write the deviation log and limitations (DOC-001).
- Optional P2: policy comparison with Fig. 1 oracle (TC-J-01…J-03) **only if everything is green**.
- **Gate G6:** all P0 tests pass in CI; matrix complete; no unexplained NaN/negative values.

### Day 10 — Portfolio delivery
- README (build, run, example output), architecture diagram, report, short demonstration.
- Report structure: (1) question; (2) system; (3) reproduced paper results (Fig. 1, oracles); (4) new experiments; (5) assumptions and limitations; (6) deviation log.
- Verify wording against NFR-008: fictional system, not flight-qualified, no overclaim about employers, no claim that Φ evaluations are reduced.
- **Gate G7:** a fresh clone builds and reproduces one experiment from the README alone.

## 4. Quality gates

| Gate | Criterion | If it fails |
|---|---|---|
| G1 | Glossary matches paper; Eq. (21) confirmed; decisions made | Do not start coding the failure model |
| G2 | Reference reproduces ORC-1…5 | Fix the reference first; do not compare against it |
| G3 | Recursive = reference on all states and all test systems; Φ called exactly 2ⁿ times | Debug against analytical oracles to decide which implementation is wrong |
| G4 | Baseline MTTF passes oracles and Eq. (6) cross-check; Fig. 1 reproduced | Skip Day 7; fix |
| G5 | [LSh] matches ORC-8 | Mark REL-002 as a known gap |
| G6 | P0 tests green in CI; traceability complete | Delay delivery; cut P2 and Experiment 2 |
| G7 | Fresh-clone reproduction works | Fix README / build |

## 5. Deliverables and owners

Single developer (Dana), using AI as a pair programmer: ask for one equation or function at a time, generate candidate edge cases, then **independently inspect, run, and record** what was verified (guide §12).

| Deliverable | Day | Source requirements |
|---|---|---|
| Notation glossary and pseudocode | 1 | A-4, DOC-001 |
| C++ library: state, system model, signature (sweep), failure model, MTTF | 2–7 | SYS-*, ALG-*, REL-* |
| Brute-force reference | 3 | ALG-005 |
| Test suite and traceability matrix | 2–9 | VER-001…004 |
| Python experiment suite, raw results, plots | 8 | EXP-001…003 |
| CI workflow | by 9 (build skeleton on Day 2) | NFR-007 |
| Verification report | 9 | DOC-001, NFR-005 |
| Technical report, README, demo | 10 | DOC-001, DOC-002, NFR-008 |

## 6. Risks and mitigations

| # | Risk | Likelihood / impact | Mitigation | Trigger |
|---|---|---|---|---|
| R1 | Notation or indexing misread | Med / High | Glossary on Day 1; checks against paper oracles (Fig. 1) | Day 5 mismatch with Fig. 1 |
| R2 | Reference repeats the same bug | Low / High | Reference written without the recurrence; validated against hand oracles first (G2) | G2 failure |
| R3 | Eq. (21) mis-transcribed from the garbled PDF text | ~~Med / High~~ Resolved | Checked against the typeset PDF: the paper itself has a typo (lower limit should be w−k); cross-checked via Eq. (6) and the recursion | TC-D-06 failure |
| R4 | Precision loss in Ψ-derived terms | Med / Med | Limit n ≤ 12 in double or extended precision; TC-F-07 | Error > TOL at larger n |
| R5 | Simultaneous model outside the paper's class (non-exchangeable common cause) | Med / Med | Use only [LSh]/LFMO; state the identical-components simplification | Scope drift on Day 7 |
| R6 | Wrong expectation about common-cause effects (as found in the first draft) | Med / Med | Compare redundancy ratio against a series baseline; write hypotheses before running | Surprising Experiment 2 result |
| R7 | Exponential growth hits memory/time in Experiment 4 | Med / Low | Benchmark small n first; report the practical limit | Run > a few minutes |
| R8 | Time overrun | High / Med | Gates G4 and G6; P1/P2 are cuttable | Day 6 not complete by end of Day 6 |
| R9 | Overclaiming (fidelity, employer relevance, Φ-evaluation savings) | Low / Med | NFR-008 review on Day 10 | — |
| R10 | Over-reliance on AI-generated code | Med / Med | Independent inspection; analytical oracles; verification log | Tests pass but behaviour unexplained |

## 7. Dependencies and assumptions

- Toolchain: C++20 compiler, CMake, GoogleTest or Catch2, Python 3 with NumPy, pandas, Matplotlib, Git, a CI runner. Linux/WSL.
- The authors' repository (`github.com/guidolagos/Subsystems`) is optional as a third independent check; verify its availability before relying on it.
- Paper content may change between versions; re-check formulas when implementing.
- The project is for learning and portfolio use; no external stakeholders or sign-off.

## 8. Definition of done

1. All P0 requirements have passing tests (traceability matrix complete); P1/P2 items are either done or listed as known gaps.
2. Recursive algorithm equals the brute-force reference on every state of every test system; Φ is called exactly 2ⁿ times.
3. Oracles ORC-1…ORC-6 pass; ORC-7 and ORC-8 pass if the corresponding features are implemented.
4. CI builds and runs the tests; the verification report records compiler, build type, tolerances, and results.
5. Experiments are reproducible from saved configuration and seed; hypotheses were recorded beforehand.
6. The report separates reproduced results from new experiments and states every limitation.
7. A fresh clone can build and reproduce an experiment using only the README.

## 9. Decisions needed from the project owner

| # | Decision | Recommendation |
|---|---|---|
| 1 | Test framework | GoogleTest (widely used; good CMake support) |
| 2 | Numerical tolerance | 1e-9, with `double` for n ≤ 12 |
| 3 | Maximum n for simultaneous-failure models | 12 in `double`; extended precision beyond |
| 4 | Include [BFR] in addition to [LSh]? | Only if Day 7 finishes early |
| 5 | Attempt P2 repair policies? | Only after G6 |
| 6 | Real start date | Replace the proposed calendar |
