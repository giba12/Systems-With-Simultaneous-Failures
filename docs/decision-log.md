# Decision Log — Space System Reliability Analyzer

| | |
|---|---|
| **Version** | 1.0 |
| **Date** | 2026-10-29 |
| **Template** | MADR 4.0.0 (Markdown Architectural Decision Records, [adr.github.io/adr-templates](https://adr.github.io/adr-templates)): front-matter fields and the headings *Context and Problem Statement, Decision Drivers, Considered Options, Decision Outcome (Consequences, Confirmation), Pros and Cons of the Options, More Information*. As the template catalog suggests, one file holds the whole collection, so the log doubles as the project's decision log |
| **Covers** | commits `ba4a60c` … `b662153` (2026-10-21 – 2026-10-27) |
| **Related** | [deviation-log.md](deviation-log.md) (departures from the paper and the guide) · [software-design-description.md](software-design-description.md) (D-1 … D-12 cite these records) |

**How this log was written.** It was written on 2026-10-29, *after* the decisions, from the repository history, the commit
messages and the existing documents. Each decision's date is the date of the commit that made it. Options marked **(recorded)**
are alternatives that earlier project documents or commit messages name; options marked **†** are the obvious alternatives,
reconstructed here for context and **not** evidence that they were weighed at the time. Only decisions that are not already
just a deviation from the paper are recorded here; deviations stay in the deviation log.

**Index**

| ADR | Decision | Status |
|---|---|---|
| [0001](#adr-0001--state-as-bit-mask-one-level-sweep-exact-integer-counting-vectors) | State as bit mask; one level sweep; exact integer counting vectors | accepted |
| [0002](#adr-0002--an-independent-brute-force-reference-and-a-direct-solution-of-eq-6) | Independent brute-force reference and direct solution of Eq. (6) | accepted |
| [0003](#adr-0003--common-cause-models-in-long-double-with-a-size-cap) | Common-cause models in `long double`, refused beyond n = 16 | accepted |
| [0004](#adr-0004--exchangeable-components-calibrated-by-the-pairwise-beta) | Exchangeable components, calibrated by the pairwise β | accepted |
| [0005](#adr-0005--the-python-layer-only-calls-the-command-line-tool) | The Python layer only calls the command-line tool | accepted |
| [0006](#adr-0006--hypotheses-are-committed-before-experiments-run) | Hypotheses are committed before experiments run | accepted |
| [0007](#adr-0007--deterministic-results-and-timing-in-separate-files) | Deterministic results and timing in separate files | accepted |
| [0008](#adr-0008--repair-policies-from-one-class-table-the-enumeration-code-kept-as-the-check) | Repair policies from one class table; enumeration code kept as the check | accepted |
| [0009](#adr-0009--toolchain-googletest-nlohmannjson-via-fetchcontent-and-a-pinned-ci-runner) | GoogleTest, nlohmann/json via FetchContent, pinned CI runner | accepted |

---

## ADR-0001 — State as bit mask, one level sweep, exact integer counting vectors

```yaml
status: accepted
date: 2026-10-21
decision-makers: project owner (git user giba12)
consulted: AI coding assistant used as pair programmer (technical report §10)
informed: none (single-maintainer project)
```

### Context and Problem Statement
The algorithm must produce the counting vector, signature and MTTF of every one of the 2ⁿ states, evaluate the structure
function Φ once per state, and stay usable up to about n = 20. How should states and counting vectors be represented and
generated?

### Decision Drivers
* the paper's Theorem 2 (Φ evaluated 2ⁿ times; O(n²2ⁿ) integer additions; memory dominated by two levels);
* the claim must be **measurable** in the code (ALG-004, ALG-006);
* no rounding in the combinatorial part.

### Considered Options
* a `uint32_t` mask per state, levels by popcount, counting vectors as exact `uint64_t`, two levels held at a time;
* keep the counting vectors of all 2ⁿ states **†**;
* floating-point counting vectors or probabilities directly **†**.

### Decision Outcome
Chosen: the first option. Each level is a flat matrix with one row of w + 1 integers per state, rows in increasing mask
order; a state's sons are found by their rank in the previous level (combinatorial number system, `level_index.hpp`, later
O(w) per state for all sons). Φ(**0**) is evaluated too, so the count is exactly 2ⁿ (deviation A2).

#### Consequences
* Good: exact integers; each division by k is exact (Lemma 1), so a remainder reveals a bug; at n = 20 the sweep holds 352 716 vectors instead of 1 048 576; the sweep counts its own additions and divisions and they equal the paper's formulas.
* Bad: n is capped at 24 by the mask size and by time (about 48 s and 534 MB at n = 24); the algorithm gives every state's answer for 2ⁿ Φ evaluations but does **not** reduce that number (the guide's claim to the contrary is corrected, deviation B1).

#### Confirmation
`SweepEvaluatesPhiExactlyOncePerState`; TC-C-22 (exact operation counts for n = 2 … 14); sweep = brute-force reference on every state; Experiment 4.

### Pros and Cons of the Options
* **All vectors kept † —** simpler indexing, but memory grows with 2ⁿ vectors and the paper's memory claim could not be demonstrated.
* **Floating point † —** loses the exact-division check and the exact signature numerators.

### More Information
Paper Algorithm 1, Lemma 1, Theorem 2. Code: `signature.cpp`, `level_index.hpp`.

---

## ADR-0002 — An independent brute-force reference and a direct solution of Eq. (6)

```yaml
status: accepted
date: 2026-10-21
decision-makers: project owner
consulted: AI coding assistant
informed: none
```

### Context and Problem Statement
Risk R2 of the project plan: a reference that shares the algorithm's bug proves nothing. How should the output of the sweep
and of the MTTF formula be checked?

### Decision Drivers
* a wrong expectation in a test is as likely as a wrong implementation (several of the assistant's own expectations were wrong);
* the check must be runnable by users (`--verify`), not only in the tests.

### Considered Options
* brute-force counting vectors by enumeration of subsets, written **without** the recurrence **(recorded: "keep it structurally independent", ALG-005)**;
* plus a direct solution of the MTTF linear system, Eq. (6), against the sweep's Eq. (7) **(recorded: REL-005)**;
* hand-computed analytical oracles only **(recorded as a supplement, not a replacement: ORC-1 … ORC-8)**.

### Decision Outcome
All three. `reference.cpp` must not use `sweep()`, `sons()`, `masks_with_working()` or `binomial()`; `solve_mttf_direct`
never looks at counting vectors. `analyze --verify` runs both and exits 3 on any mismatch, still writing the result.

#### Consequences
* Good: two independent routes plus analytical oracles; the verification found real defects and near-misses (verification report §5); the failure path itself is tested through a labelled fault-injection hook.
* Bad: the reference costs about 3ⁿ and is limited to n ≤ 16; `--verify` is skipped beyond that.

#### Confirmation
TC-H-06 (fault injection makes `--verify` exit 3); mutation record (verification report §3.1); Eq. (6) = Eq. (7) on all states under all seven failure models.

### Pros and Cons of the Options
* **Oracles only —** cheap but covers only systems with known closed forms.
* **Reference + Eq. (6) + oracles —** three independent kinds of evidence, at the price of one slow module.

### More Information
`reference.hpp`, requirements ALG-005, REL-005, VER-001.

---

## ADR-0003 — Common-cause models in `long double` with a size cap

```yaml
status: accepted
date: 2026-10-21
decision-makers: project owner
consulted: AI coding assistant
informed: none
```

### Context and Problem Statement
The transition kernel (Eq. (20)) and the increments (Eq. (21)) of the Lévy-frailty models are alternating sums whose
cancellation grows like 2ⁿ. In `double` the measured error of the increments is about 8e-15 at n = 6, 8e-9 at n = 20 and
6e-4 at n = 30. The paper uses 512-bit arithmetic. Separately, the printed lower limit of Eq. (21) is a typo.

### Decision Drivers
* never return a number that cannot be trusted (NFR-001, NFR-009);
* keep the tool dependency-free and fast;
* the experiments need n up to about 10 for common-cause models, not 30.

### Considered Options
* `double` with n ≤ 12 **(recorded: plan decision 3)**;
* 512-bit arithmetic as in the paper **(recorded: deviation A3)**;
* `long double`, the non-negative recursion for the increments, Eq. (21) with lower limit w − k as a cross-check, refusal when the kernel rows are not distributions, and a cap at n = 16.

### Decision Outcome
The third. Measured accuracy of a parallel-system MTTF against exact rational arithmetic: about 5e-15 relative at n = 6,
3e-12 at n = 12 and 2e-10 at n = 16; at n = 17 some β are already refused, so the cap is 16
(`kMaxLevyFrailtyComponents`).

#### Consequences
* Good: no new dependency; errors measured and documented; unsafe requests fail loudly with an explanation.
* Bad: n ≤ 16 for common-cause models (the paper reaches 30); the verification tolerance for these models is 1e-9, not 1e-12.

#### Confirmation
Precision tests; test `PrintedEquation21LowerLimitIsATypo`; paper-validation report §4 and §6.

### Pros and Cons of the Options
* **`double`, n ≤ 12 —** simplest but wastes the headroom that `long double` gives.
* **512-bit —** matches the paper but needs a multiprecision library and slows the tool.
* **`long double` + cap —** measured, cheap, honest about the limit.

### More Information
Deviation log A1, A3, A4; requirements NFR-009.

---

## ADR-0004 — Exchangeable components, calibrated by the pairwise β

```yaml
status: accepted
date: 2026-10-21
decision-makers: project owner
consulted: AI coding assistant
informed: none
```

### Context and Problem Statement
The satellite has three pairs with different purposes, but the paper's models need exchangeable (identical in law)
components. How should failure models be parameterised so that different families can be compared fairly?

### Decision Drivers
* the paper's Assumptions (A) – (C);
* a first draft's wrong expectation about common cause (risk R6) showed that absolute MTTF misleads;
* honesty about fidelity (NFR-008).

### Considered Options
* identical components, every model calibrated to Ψ(1) = 1 and parameterised by β = 2 − Ψ(2) **(recorded: A9, REL-004)**;
* each family with its own natural parameters **†**;
* non-identical components **(recorded as out of scope: A6; the paper lists it as future work)**.

### Decision Outcome
The first. Experiments report the benefit of redundancy **relative to a series baseline under the same model**
(deviation B4), because absolute MTTF can rise with β while the benefit of redundancy falls.

#### Consequences
* Good: six families comparable under one number; Experiment 6 can rank them at equal β.
* Bad: the three pairs are modelled as having one lifetime law; stated in every report.

#### Confirmation
β = 0 reproduces [Exp]; invalid β rejected (TC-E-08); Experiments 2 and 6.

### Pros and Cons of the Options
* **Own parameters † —** each family easy to read, but no fair comparison.
* **Non-identical —** more realistic, but outside the paper's theorems.

### More Information
Deviation log A5, A6, A9, A10, B4; notation glossary §4.

---

## ADR-0005 — The Python layer only calls the command-line tool

```yaml
status: accepted
date: 2026-10-22
decision-makers: project owner
consulted: AI coding assistant
informed: none
```

### Context and Problem Statement
Experiments need scripting, tables and plots; the maths is in C++. How should Python reach the engine?

### Decision Drivers
* one implementation of the maths to verify;
* every number in a table traceable to a configuration, a seed and a tool version (NFR-006);
* standard-library-only operation for the runner.

### Considered Options
* subprocess calls of `analyze` with JSON in and out **(recorded: INT-002)**;
* Python bindings to the library **(recorded: "or bindings", INT-002)**;
* re-implementing the algorithm in Python for the experiments **†**.

### Decision Outcome
Subprocess calls. `run_experiments.py` imports only the standard library; pandas and matplotlib are needed only to analyse and plot.

#### Consequences
* Good: no binding code or build coupling; results contain exactly what the tool returned; the same contract serves users and experiments.
* Bad: process start-up cost per run (negligible against the analysis times); the CLI JSON becomes a de-facto API (see the interface description §5).

#### Confirmation
TC-G-10 (the Python layer does not re-implement the algorithm); Python end-to-end tests under `ctest`.

### Pros and Cons of the Options
* **Bindings —** faster calls, but a second build system and a wider surface to verify.
* **Re-implementation † —** would defeat independent verification.

### More Information
`python/run_experiments.py`; [interface-design-description.md](interface-design-description.md) IF-5.

---

## ADR-0006 — Hypotheses are committed before experiments run

```yaml
status: accepted
date: 2026-10-21
decision-makers: project owner
consulted: AI coding assistant
informed: none
```

### Context and Problem Statement
An AI-assisted project can easily "predict" what it has already seen. How can the experiments be shown to test something?

### Decision Drivers
* a first-draft expectation about common cause was wrong (risk R6);
* the history must make the order visible;
* a rejected prediction must stay rejected (honesty, DOC-001).

### Considered Options
* a hypothesis file per experiment, committed first; the runner refuses to start without it and records its SHA-256 and whether it predates the run **(recorded: EXP-002)**;
* write the expected results after the run **†**.

### Decision Outcome
The first. Hypothesis commits precede the code that runs them (`257394a` before the runner; `ee43a39`, `40d272e`, `081aa56` before Experiments 5, 6, 7). `analyze_results.py` gives each prediction a SUPPORTED or REJECTED verdict.

#### Consequences
* Good: predictions failed and are reported as found (Experiments 2, 4 and 6; see deviation log D4); the experiments are credible as tests.
* Bad: the check that a hypothesis predates a run relies on a file's modification time and on commit order, not on a signature.

#### Confirmation
TC-G-07, TC-G-08; `hypothesis_written_before_run` in every timing file; git history.

### Pros and Cons of the Options
* **Post-hoc †** — nothing could be rejected.

### More Information
`project/experiments/hypotheses/`; experiment report §2.

---

## ADR-0007 — Deterministic results and timing in separate files

```yaml
status: accepted
date: 2026-10-22
decision-makers: project owner
consulted: AI coding assistant
informed: none
```

### Context and Problem Statement
Run times, memory and timestamps vary between runs; results must be reproducible byte for byte (EXP-001). Peak memory was
also wrong at first: `ru_maxrss` is inherited across fork and exec and reported the Python parent's memory (constant
19 MB, deviation D1).

### Decision Drivers
* byte-identical comparison of repeated runs;
* correct memory figures.

### Considered Options
* the CLI writes `timing` as a separate top-level key; the runner moves it to `<exp>.timing.json`; peak memory is read from `VmHWM` in `/proc/self/status`;
* one result file with volatile fields masked when comparing **†**;
* `ru_maxrss` **(recorded as wrong: D1)**.

### Decision Outcome
The first.

#### Consequences
* Good: the deterministic file is identical on repeat; timing claims are kept visibly apart and labelled as orders of magnitude.
* Bad: the CLI is Linux-specific (`/proc`, `<sys/resource.h>`); a timing-based prediction may flip on a repeat run (Experiment 4, experiment report §6.1).

#### Confirmation
TC-G-01; tests that compare two runs byte for byte.

### Pros and Cons of the Options
* **Masking †** — error-prone; any new volatile field breaks reproducibility silently.

### More Information
Deviation log D1, D2.

---

## ADR-0008 — Repair policies from one class table; the enumeration code kept as the check

```yaml
status: accepted
date: 2026-10-26
decision-makers: project owner
consulted: AI coding assistant
informed: none
```

### Context and Problem Statement
The optional repair-policy extension (paper §V) first used a direct evaluation over states (about 3ⁿ, n ≤ 14). The paper's
level sums and Proposition 3 allow n ≤ 20. Which implementation should the tool use, and what happens to the old one?

### Decision Drivers
* a complete policy comparison at 16, 18 and 20 components took 4.2, 20 and 92 s with the first fast version;
* every policy result needs an independent check, as in ADR-0002;
* Proposition 3 assumes a monotone MTTF.

### Considered Options
* keep the enumeration code only (n ≤ 14);
* level-sum evaluation and a single-pass threshold search over classes of equal counting vectors, with the enumeration code **kept** as `*_by_enumeration` **(recorded: A7)**;
* additionally read r-out-of-n and corrective policies from the same class table, since their continue sets are closed under "more components working" **(recorded: commit `88d62f2`)**.

### Decision Outcome
The third. Comparison at 16 / 18 / 20 components: 0.57 / 2.4 / 7.9 s. Ties break towards not repairing.

#### Consequences
* Good: n ≤ 20; two implementations of every policy computation that agree to about 3e-14 and with closed forms from the signature; the optimal state-based policy (Dinkelbach) checked against an exhaustive search over all policies on small systems.
* Bad: the class table assumes a monotone MTTF (true for every model here, Lemma 4) — a failure model outside that class would need the enumeration code; two code paths to maintain.

#### Confirmation
TC-J-01 … TC-J-14; ten deliberate bugs in the two implementations all caught (verification report §6); Fig. 1 costs 97/105, 11/15, 1/10.

### Pros and Cons of the Options
* **Enumeration only —** simple, independent, but unusable above 14.
* **Class table + enumeration as check —** fast and independently verified at the cost of duplicate code.

### More Information
Commits `750bc03`, `88d62f2`; deviation log A7; experiment report §7 and §9.

---

## ADR-0009 — Toolchain: GoogleTest, nlohmann/json via FetchContent, and a pinned CI runner

```yaml
status: accepted
date: 2026-10-22
decision-makers: project owner
consulted: AI coding assistant
informed: none
```

### Context and Problem Statement
Open decisions in the guide: test framework, JSON library, how to obtain them, and how CI should select its runner and
actions (the GitHub-hosted `ubuntu-latest` was migrating to a newer Ubuntu).

### Decision Drivers
* a fresh clone builds from the README alone (definition of done 7);
* compiler versions quoted in the verification report must remain valid (NFR-005);
* no deprecated runtimes in CI.

### Considered Options
* GoogleTest 1.14.0 **or** Catch2 **(recorded: open item 4)**; nlohmann/json 3.11.3 (MIT, header-only) **(recorded: Day 2)**;
* dependencies fetched by CMake `FetchContent` at configure time, versus vendored copies or a system package **†**;
* CI on `ubuntu-latest` versus `ubuntu-24.04` **(recorded: commit `4739136`)**; `actions/checkout@v7` and `actions/setup-python@v7` versus v4 / v5 **(recorded: commit `3d60426`)**.

### Decision Outcome
GoogleTest 1.14.0, nlohmann/json 3.11.3 via FetchContent, CI pinned to `ubuntu-24.04` with v7 actions, matrix g++ and
clang++ (Release) plus an ASan/UBSan job (Debug). The workflow lives at the repository root, because GitHub reads workflows
only there (it was first placed under `project/.github` and never ran, commit `b5c31ad`).

#### Consequences
* Good: no vendored code; the same commands locally and in CI; the first green run is recorded (run 39864780885) and the latest recorded run is 40394591079 at `88d62f2`.
* Bad: the first configure needs the network and does not pin a checksum for either download; Linux only; the pin must be revisited when `ubuntu-24.04` is retired. Commit `b662153` (comments and documents only) has no CI run recorded.

#### Confirmation
`.github/workflows/ci.yml` runs on every push and pull request; `ctest` 129 / 129 in every job (verification report §2).

### Pros and Cons of the Options
* **FetchContent —** simplest build, depends on the network and on the upstream URLs.
* **Vendoring †** — offline builds, at the cost of third-party code in the repository.
* **Pinned runner —** reproducible compiler versions; manual upkeep.

### More Information
Requirements open items 4; deviation log B7, D5.
