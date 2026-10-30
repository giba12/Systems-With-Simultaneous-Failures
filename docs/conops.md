# Concept of Operations — Space System Reliability Analyzer

| | |
|---|---|
| **Version** | 1.0 |
| **Date** | 2026-10-29 |
| **Template** | NASA Systems Engineering Handbook, Appendix S, *Concept of Operations Annotated Outline* (sections 1.0 – 8.0 and appendices follow that outline; the NASA outline is the stronger general-purpose choice listed in the template catalog) |
| **Describes** | `analyze` 0.8.0, commit `b662153` |
| **Related** | [requirements-spec.md](requirements-spec.md) · [user-stories.md](user-stories.md) · [technical-report.md](technical-report.md) · [interface-design-description.md](interface-design-description.md) |

The satellite-support system analysed by this tool is **fictional**. This is an educational portfolio project, not a
certified safety analysis and not flight-qualified (NFR-008).

## 1.0 Introduction

### 1.1 Project description

#### 1.1.1 Background
Redundancy is only worth what the failure process lets it be worth. For a system of *n* components, the question *"given
which components have already failed, how much useful life is left?"* has one answer per state, and there are 2ⁿ states.
Lagos & Ávila (arXiv:2610.22181) give an algorithm that returns the signature and the mean time to failure (MTTF) of
**every** state from one level-by-level sweep that evaluates the structure function Φ once per state, and that also covers
failure models in which one event can fail several components at once (common cause).

This project implements that algorithm, verifies it against independent computations, and uses it to run seven
pre-registered experiments on a small, fictional satellite-support system (three redundant pairs: power, communications,
computers).

Goals:

1. reproduce the paper's worked example and complexity claims (not its 22-network empirical study);
2. answer new questions on the fictional system: how redundancy, common cause, equal failure counts, scale, repair
   policy and failure-model choice change the answer;
3. show the work honestly: what was verified, what was not, which predictions were rejected.

#### 1.1.2 Assumptions and constraints

| # | Assumption / constraint | Source |
|---|---|---|
| AC-1 | Components are binary and failures are permanent; the core analysis has no repair | requirements A-1 |
| AC-2 | Components are **identical in law** (exchangeable), so the three differently-purposed pairs share one lifetime model | requirements A-6; deviation A6 |
| AC-3 | Redundancy inside a pair is perfect: no switch-over failure, shared resource or thermal coupling | deviation log §E |
| AC-4 | The structure function is monotone with Φ(0) = 0 and Φ(1) = 1; 2 ≤ n ≤ 24 | `SystemModel::validate` |
| AC-5 | Common-cause models are limited to n ≤ 16 (numerical precision); repair-policy analysis to n ≤ 20; the brute-force reference to n ≤ 16 | NFR-009; deviation A4, A7 |
| AC-6 | Single user, offline, command line. Needs a C++20 compiler, CMake ≥ 3.20 and Python 3 | requirements §2.1 |
| AC-7 | The first configure downloads GoogleTest 1.14 and nlohmann/json 3.11.3 | `CMakeLists.txt` |

### 1.2 Overview of the envisioned system

#### 1.2.1 Overview
A C++20 library and a command-line tool, `analyze`, read a JSON description of a system and a failure model, and write a
JSON result: the signature and MTTF of every state, the minimal path sets, an optional independent verification, and an
optional comparison of repair policies. A thin Python layer runs the experiments by calling `analyze`, turns the saved
results into tables with a SUPPORTED / REJECTED verdict per prediction, and draws the figures from those tables.

```
 system + failure model (JSON)  ──►  analyze  ──►  result (JSON)  ──►  run_experiments.py ─► tables/verdicts ─► figures
        configs/*.json            (C++ engine)                          (Python, calls the CLI only)
                                       ▲
              hypotheses written BEFORE each run (experiments/hypotheses/*.md)
```

The component and sequence diagrams are in [uml/component.png](uml/component.png) and [uml/sequence.png](uml/sequence.png).

#### 1.2.2 System scope

| Inside the project | Outside the project |
|---|---|
| Level-sweep engine, failure models (independent exponential and six common-cause models), MTTF engine, repair-policy evaluation | Real spacecraft data, real failure rates, flight qualification |
| Independent brute-force reference and direct solution of Eq. (6) | Non-identical components; failure models beyond the paper's Table I set (A5) |
| `analyze` CLI, Python experiment layer, traceability checker, CI workflow | Any operating system other than Linux (not tried) |
| The documents in `docs/` | The paper's empirical results (22 networks, up to 30 components, Julia, a cluster) |

External and enabling systems: GitHub (repository and Actions CI), the GitHub release servers that FetchContent downloads
from, the paper `docs/paper3.pdf` (kept locally, not published), and the compiler / CMake / Python toolchain.

## 2.0 Documents

### 2.1 Applicable documents
- Lagos & Ávila, *Per-State Signatures and Mean Time-to-Failure: An Algorithm for Systems With Simultaneous Failures*, arXiv:2610.22181.
- [requirements-spec.md](requirements-spec.md) (39 requirements: SYS, ALG, REL, VER, EXP, DOC, INT, NFR).

### 2.2 Reference documents
[user-stories.md](user-stories.md) · [test-plan.md](test-plan.md) · [verification-report.md](verification-report.md) ·
[experiment-report.md](experiment-report.md) · [deviation-log.md](deviation-log.md) · [notation-glossary.md](notation-glossary.md) ·
[paper-validation-report.md](paper-validation-report.md) · [project-plan.md](project-plan.md) · [demo.md](demo.md)

## 3.0 Description of the envisioned system

### 3.1 Needs, goals and objectives

| Need | Goal | Objective (measurable) | Requirements | Stories |
|---|---|---|---|---|
| Know the remaining life of a partly-failed system | Per-state MTTF and signature | Reference system: MTTF(all working) = 0.7/λ; signature (0, 0.2, 0.4, 0.4, 0, 0) | REL-001, ALG-003 | US-04, US-05, US-07 |
| Know whether a result can be trusted | Independent verification | `--verify` passes; mismatch exits 3 and names the first bad state | ALG-005, VER-001, VER-002 | US-10 – US-12 |
| See what shared causes do to redundancy | Common-cause models calibrated to β | Redundancy benefit reported relative to a series baseline under the same model | REL-002, REL-004, EXP-002 | US-08, US-17 |
| Decide when to repair | Repair-policy comparison | Fig. 1 costs 97/105, 11/15, 1/10 reproduced; all threshold breakpoints searched | EXP-004 | US-20 |
| Trust the experiments | Pre-registration and reproducibility | Runner refuses without a hypothesis; result files byte-identical on repeat | EXP-001 | US-15 |
| Know the limits | Honest scope | Every deviation and limitation in one log | DOC-001, DOC-002 | US-21 |

### 3.2 Overview of the system and key elements

| Element | Role | Operated by |
|---|---|---|
| **System model** (`system_model`, `state`) | Holds components and Φ; validates monotonicity, ids, bit use, range; counts calls to Φ | engine |
| **Signature engine** (`signature`, `level_index.hpp`) | Paper's Algorithm 1: counting vectors, signatures, minimal path sets, two levels in memory | engine |
| **Failure model** (`failure_model`) | ν(w), P(w, w′) and increments for [Exp] and the six Lévy-frailty models | engine |
| **MTTF engine** (`mttf`) | Eq. (7) per state | engine |
| **Reference** (`reference`) | Brute-force counting vectors and path sets; direct solution of Eq. (6); never uses the recurrence | engine, `--verify` |
| **Policy module** (`policy`) | Corrective, r-out-of-n, every MTTF-threshold, and optimal state-based repair policies | engine, cost flags |
| **`analyze`** | Command line, JSON in and out, exit codes 0–3 | analyst |
| **Python layer** | `run_experiments.py`, `analyze_results.py`, `plot_results.py` | analyst |
| **`check_traceability.py`** | Requirements → test cases → test code audit | verifier, CI |
| **CI workflow** | g++ and clang++ (Release) and ASan/UBSan (Debug) on every push and pull request | CI server |

Users: **Reliability analyst** (primary), **Verifier / reviewer**, **Portfolio reader**, and the **CI server** as a
system actor (use-case diagram [uml/use-case.png](uml/use-case.png), UC1 – UC20).

### 3.3 Interfaces
Specified in [interface-design-description.md](interface-design-description.md): the system-definition file, the
command-line flags and exit codes, the result file, the C++ library API, the Python-to-CLI contract, the hypothesis and
experiment-result files, and the external interfaces (FetchContent downloads, GitHub Actions).

### 3.4 Modes of operation

| Mode | Purpose | Entered by | Notes |
|---|---|---|---|
| **Development** | Change code or documents | edit, `cmake --build` | warnings on (`-Wall -Wextra -Wpedantic`), no warnings tolerated |
| **Verification** | Establish the right answers | `ctest`, `analyze --verify` | 129 ctest entries; also run in CI |
| **Analysis** | Answer a question about one system | `analyze --config …` | seconds or less up to n = 20 |
| **Experiment** | Test a pre-registered prediction | `run_experiments.py` | refuses to start without a hypothesis file |
| **Reporting** | Tables, verdicts, figures | `analyze_results.py`, `plot_results.py` | reads saved results only |
| **Disposal** | End of use | delete the checkout | no installed files, services or persistent state beyond `build/`, `.venv/`, `results/` (all ignored by git) |

There is no training mode and no operations mode in the flight sense: the tool never runs on a spacecraft.

### 3.5 Proposed capabilities

| ID | Capability | Verified by |
|---|---|---|
| CAP-1 | Load and validate a system from JSON (`and_of_or_groups`, `path_sets`, `series`, `parallel`) | SYS-001, SYS-003 – SYS-005 |
| CAP-2 | Signature, counting vector and MTTF at every state with exactly 2ⁿ evaluations of Φ | ALG-001 – ALG-006, REL-001 |
| CAP-3 | Minimal path sets as a by-product | SYS-002 |
| CAP-4 | Independent and common-cause failure models, n ≤ 16 for the latter | REL-002 – REL-004 |
| CAP-5 | Self-verification against two independent routes | REL-005, VER-001 – VER-004 |
| CAP-6 | Repair-policy comparison, n ≤ 20 | EXP-004 |
| CAP-7 | Reproducible, pre-registered experiments with tables, verdicts and figures | EXP-001 – EXP-003 |
| CAP-8 | Machine-checked requirement-to-test traceability | VER-003 |

## 4.0 Physical environment

| Item | Value |
|---|---|
| Platform | Linux x86-64 (developed and CI-tested on Ubuntu 24.04 / WSL2); no other OS tried (NFR-005 partly met) |
| Compilers | g++ 13.3, clang++ 18.1; C++20 |
| Memory | about 35 MB at n = 20 (1 048 576 states); about 534 MB at n = 24 |
| Time | satellite analysis about 0.08 ms; n = 20 in about 2.2 s; policy comparison about 0.6 s at n = 16, 2.4 s at 18, 8 s at 20 |
| Network | needed only at first configure (dependency downloads) and for CI |

The 2ⁿ growth is the environment limit that matters. Figures are from one laptop and are orders of magnitude, not benchmarks (D2).

## 5.0 Support environment

- **Maintenance:** one maintainer; changes go to `main` and CI runs on every push. Dependencies are pinned (GoogleTest 1.14.0, nlohmann/json 3.11.3, `ubuntu-24.04`, `actions/checkout@v7`, `actions/setup-python@v7`). See [configuration-management-plan.md](configuration-management-plan.md).
- **Upgrades:** a compiler or runner change invalidates the versions quoted in the verification report, which must be re-recorded.
- **Documents:** the traceability matrix is generated (`check_traceability.py --write`); everything else is edited by hand and checked by review.
- **Support to users:** the README, [demo.md](demo.md) and the file guide. No help desk, no warranty, no licence yet (all rights reserved by default).

## 6.0 Operational scenarios

Each scenario is a single-thread timeline, labelled for traceability. Commands are run from the repository root.

### 6.1 Nominal conditions

**NS-1 — Analyse the satellite and verify the answer** (US-01, US-04, US-05, US-07, US-10; UC1 – UC5, UC8, UC11)
1. Analyst runs `project/build/analyze --config project/configs/satellite_support.json --verify`.
2. The tool loads and validates the six components and the AND-of-OR-pairs structure (UC1, UC2).
3. One sweep computes all 64 counting vectors, signatures and MTTFs; Φ is called 64 times.
4. The verification compares every counting vector with the brute-force reference, every MTTF with the direct solution of Eq. (6), and runs the numerical checks.
5. The JSON reports MTTF(all working) = 0.7, 27 operational states, 8 minimal path sets, signature (0, 0.2, 0.4, 0.4, 0, 0), `verification.passed = true`. Exit status 0.

**NS-2 — Same number of failures, different futures** (US-09; UC3, UC8)
1. Analyst adds `--per-state --out result.json`.
2. Analyst compares the states {P1, C1 failed} (MTTF 0.417) and {P1, P2 failed} (MTTF 0).
3. Conclusion: the count of failures does not determine the remaining life; which components failed does (Experiment 3).

**NS-3 — Common cause erodes the benefit of redundancy** (US-08, US-17; UC7, UC9)
1. Analyst runs the tool on the satellite and on a six-component series system with `--model lsh --beta 0.5` and `--model bfr --beta 0.5`.
2. The benefit of redundancy (satellite MTTF ÷ series MTTF) falls from 4.20 (independent) to 3.20 (lethal shock) and 1.50 (binomial failure rate).

**NS-4 — Decide when to repair** (US-20; UC10)
1. Analyst adds `--c-comp 0.1667 --c-sys 1 --c-set 0.2`.
2. The tool evaluates corrective maintenance, every r-out-of-n policy, every MTTF-threshold policy (all breakpoints, one pass over classes of equal counting vectors) and the optimal state-based policy, and reports each long-term mean cost.
3. Analyst reads the best policy and the excess of the best threshold policy over the optimum.

**NS-5 — Run a pre-registered experiment** (US-15, US-16, US-18, US-19; UC16 – UC18)
1. Analyst confirms `project/experiments/hypotheses/expN_*.md` exists, with predictions and rejection criteria.
2. `python3 python/run_experiments.py expN` calls `analyze`, writes a deterministic result file (settings, seed, tool version, hypothesis SHA-256, raw output) and a separate timing file.
3. `analyze_results.py` writes tables and a SUPPORTED / REJECTED verdict per prediction; `plot_results.py` draws the figure from those tables.
4. Repeating step 2 gives a byte-identical deterministic file.

**NS-6 — Reviewer verifies the project** (US-13, US-14, US-22; UC14, UC15, UC19)
1. Reviewer clones, configures, builds and runs `ctest` (129 tests) with the commands in the README.
2. `check_traceability.py` confirms every requirement maps to a test case and every test case to test code or a stated alternative.
3. Reviewer reads the verification report, including "what has not been verified".

### 6.2 Off-nominal conditions

| ID | Condition | Expected behaviour | Exit | Requirement |
|---|---|---|---|---|
| **OS-1** | Configuration is malformed, has a duplicate id or bit, a group is empty, or Φ is not monotone | Message names the field or state and the broken rule; no result | 1 | SYS-001, SYS-003, INT-004 |
| **OS-2** | Flag missing a value, unknown flag, only some of `--c-comp/--c-sys/--c-set`, unknown `--model` | Usage message | 2 | INT-004 |
| **OS-3** | Common-cause model without β, β outside its range, or n > 16 | Error naming the model, the range or the precision limit | 1 | REL-004, NFR-009 |
| **OS-4** | Repair-policy request with n > 20 | `corrective_policy needs between 2 and 20 components` | 1 | EXP-004 |
| **OS-5** | `--verify` finds a mismatch (or the labelled fault `ANALYZE_FAULT_INJECTION` is set, which exists only to test this path) | JSON is still written, `verification.passed = false`, first mismatching state named, `VERIFICATION FAILED` on stderr | 3 | VER-001, INT-004 |
| **OS-6** | `--verify` on n > 16 | Verification is skipped with a stated reason (`performed = false`, `skipped_reason` set, `passed` null); the rest of the output is unaffected | 0 | ALG-005 |
| **OS-7** | Experiment started without a hypothesis file | Runner refuses and names the file to write first | 1 | EXP-001 |
| **OS-8** | A prediction fails | Reported as REJECTED, not explained away; the hypothesis file is not edited afterwards (stable rejections occurred in Experiments 2, 4 and 6; counts in [deviation-log.md](deviation-log.md) D4) | — | DOC-001 |
| **OS-9** | A run-time prediction flips on a repeat run | Treated as timing noise (experiment report §6.1); timing claims are orders of magnitude only | — | NFR-002 |
| **OS-10** | Network unavailable at first configure | FetchContent fails; no workaround is provided (offline build not supported) | — | AC-7 |

## 7.0 Impact considerations

### 7.1 Environmental impacts
None beyond the electricity of a laptop and a CI runner. No physical hardware, no space environment.

### 7.2 Organizational impacts
None. One maintainer, no sign-off, no external stakeholders. The documents state that nothing here claims to use, or be
used by, any organisation's tools (NFR-008).

### 7.3 Scientific / technical impacts
- Shows that per-state signatures and MTTFs can be computed exactly for up to 2²⁰ states in seconds on a laptop.
- Documents a correction to the paper (the lower summation limit of Eq. (21)) and a reading of Algorithm 1 (Φ(0) is evaluated).
- Records pre-registered predictions that failed, which is a result in itself.
- Does **not** reduce the number of Φ evaluations below 2ⁿ; it provides every state's answer for that count.

## 8.0 Risks and potential issues

| # | Risk | Mitigation / status |
|---|---|---|
| K-1 | Reference repeats a bug of the main algorithm | Reference written without the recurrence; direct Eq. (6) solution as a second route; mutation record in the verification report |
| K-2 | Precision loss in the alternating sums of Eqs. (20) – (21) | `long double`, refusal when probabilities do not form a distribution, cap n ≤ 16 |
| K-3 | Overclaiming (fidelity, employer relevance, savings in Φ evaluations) | NFR-008 review; scope statements in the deviation log §E |
| K-4 | Results depend on one platform | Linux only; CI pinned to Ubuntu 24.04; other OSes not tried |
| K-5 | Paper changes between versions | Equations re-checked against the typeset PDF (validation report §6) |
| K-6 | Repair-policy results over-generalised | Experiments use systems of 3 – 10 components; limits stated in the experiment report |
| K-7 | No licence chosen | All rights reserved by default; decide before inviting reuse |
| K-8 | Third-party documents (paper, project guide) not published | Ignored by git; links in the validation report need local copies |

## Appendix A — Acronyms

| | |
|---|---|
| ASan / UBSan | AddressSanitizer / UndefinedBehaviorSanitizer |
| BFR | Binomial failure rate (common-cause model) |
| CI | Continuous integration |
| CLI | Command-line interface |
| LFMO | Lévy-frailty Marshall–Olkin (family of common-cause models) |
| LSh | Lethal shock (common-cause model) |
| MTTF | Mean time to failure |
| Φ (phi) | Structure function: 1 if the system works in a state, else 0 |
| ConOps | Concept of operations |

## Appendix B — Glossary

Terms (state, counting vector, signature, tail signature, Ψ, β, γ) are defined in [notation-glossary.md](notation-glossary.md)
and [requirements-spec.md](requirements-spec.md) §1.4.
