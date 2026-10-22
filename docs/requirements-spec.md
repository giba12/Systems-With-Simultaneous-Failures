# Requirements Specification — Space System Reliability Analyzer

| | |
|---|---|
| **Version** | 1.1 (validated against the paper) |
| **Date** | 2026-10-20 |
| **Source** | `space_system_reliability_analyzer_project_guide.docx` (v1.0) |
| **Research basis** | Lagos & Ávila, *Per-State Signatures and Mean Time-to-Failure: An Algorithm for Systems With Simultaneous Failures* (arXiv:2610.22181v1, `paper3.pdf`) |
| **Validation** | See [paper-validation-report.md](paper-validation-report.md). Equation and section numbers below (Eq. (n), Lemma, Theorem) refer to the paper. |

## 1. Introduction

### 1.1 Purpose
This document specifies what the Space System Reliability Analyzer shall do. It is the basis for the test plan ([test-plan.md](test-plan.md)), the user stories ([user-stories.md](user-stories.md)), the project plan ([project-plan.md](project-plan.md)) and the UML models in [uml/](uml/).

### 1.2 Scope
The product is a small, auditable reliability-analysis tool for a **fictional** satellite-support architecture. It:

- models binary component states and a monotone structure function;
- computes per-state counting vectors and signatures with the paper's recursive algorithm;
- computes mean time-to-failure (MTTF) under an explicitly supported failure model;
- verifies the results against an independent brute-force reference;
- runs reproducible experiments and reports results.

### 1.3 Out of scope
- Certified safety analysis, flight qualification, or any claim about a specific employer's tools.
- Switch-over failure, shared resources, thermal coupling, software common-mode faults, and mission dynamics (guide §3 caveat).
- Systems too large for exhaustive 2ⁿ enumeration (the guide targets 6 components as a teaching case).
- Non-identical components (the paper's assumptions require exchangeable components; the authors list non-identical components as future work).
- Repair-policy optimisation, except as the optional P2 extension.

### 1.4 Definitions

| Term | Meaning |
|---|---|
| Structure function Φ(x) | 1 if the system works in state x, 0 otherwise. |
| Monotone system | Turning a working component off never turns a failed system on: Φ(x₁) ≤ Φ(x₂) whenever x₁ ≤ x₂. The paper additionally assumes **n > 1** and a **non-trivial** Φ (Φ(**0**) = 0 and Φ(**1**) = 1). Irrelevant components are allowed (semi-coherent systems). |
| State mask | Integer whose bit *i* is 1 if component *i* works, 0 if failed. 6 components → 2⁶ = 64 states. |
| Counting vector Wₖˣ | Number of operational sub-states of x reached by shutting down exactly *k* currently working components (k = 0 … \|x\|). W₀ˣ = Φ(x); Wₖˣ = (1/k) Σ Wₖ₋₁ˣ′ over the \|x\| sons x′ of x (Lemma 1, Eq. (8)). |
| Tail signature S̄ˣₖ | Wₖˣ / C(\|x\|, k): the proportion of k-failure sub-states of x that still work (Eq. (2)). |
| Signature sˣₖ | S̄ˣₖ₋₁ − S̄ˣₖ for k = 1 … \|x\|; entries ≥ 0 and sum to 1 (Eq. (1)). The paper computes one per state; the "system signature" is the one at the all-working state **1**. |
| MTTF | Mean time to failure from state x. Per-state m(x) = Σₖ₌₀^{\|x\|−1} [Wₖˣ / C(\|x\|,k)] · [E(Tₖ₊₁:\|x\|) − E(Tₖ:\|x\|)] (Eq. (7)); m(x) = 0 if Φ(x) = 0. Tₖ:w are the **order statistics** of the component failure times (ties are possible under simultaneous failures), with T₀:w = 0. |
| Minimal path set | A set of components that, if all work, operate the system, and no proper subset does. State x defines one iff W₀ˣ = 1 and W₁ˣ = 0 (working, and every son failed). |
| Assumptions (A)–(C) | (A) component lifetimes are exchangeable; (B) the number of working components is a continuous-time Markov chain (hence a pure-death chain); (C) E[Tₖ:\|x\|] can be computed for every state. These define the failure models the algorithm supports. |
| Pairwise beta factor β | β = 2 − Ψ(2) when Ψ(1) = 1: the fraction of a component's failure rate due to events that also fail a given other component (Eq. (22)). β = 0 is the independent-exponential model. |
| Common-cause / simultaneous failure | A single event that fails more than one component. |
| Reference implementation | Deliberately simple brute-force code that does **not** reuse the recurrence under test. |

### 1.5 Priorities
Priorities follow the guide: **P0** required, **P1** required if time allows (extension), **P2** optional.

## 2. Overall description

### 2.1 Context
A single-user, offline command-line / library tool. A C++20 engine does the numerical work; Python scripts drive experiments and plots. There is no network, database, or multi-user element.

### 2.2 Users

| User | Description |
|---|---|
| Reliability analyst (primary) | Defines a system, runs analyses, reads signatures and MTTF. |
| Verifier / reviewer | Runs the test suite and reads the verification report and traceability. |
| Portfolio reader / hiring reviewer | Reads the README, report and demo; needs clear assumptions and limits. |

### 2.3 Modules (from guide §5)
System model · State-space engine (the level sweep, inside the signature engine) · Signature engine · Failure model · MTTF engine · Python analysis · Verification/reporting.

### 2.4 Reference system
Six components in three redundant pairs: P1/P2 (power), C1/C2 (communications), K1/K2 (computers).
Φ(x) = (xP1 ∨ xP2) ∧ (xC1 ∨ xC2) ∧ (xK1 ∨ xK2).
Bit assignment used by this specification: bit0 = P1, bit1 = P2, bit2 = C1, bit3 = C2, bit4 = K1, bit5 = K2.

### 2.5 Assumptions and constraints
- A-1 Components are binary (working / failed) and failures are permanent (no repair in the core). The system has n > 1 components, a monotone and non-trivial Φ, and may contain irrelevant components.
- A-2 Redundancy within a pair is perfect.
- A-3 Failure-time expectations E(Tₖ:n) depend on the chosen failure model and shall not be assumed interchangeable across models.
- A-4 The equations, indexing and assumptions in this document were checked against the paper (see the validation report). Any further deviation is logged (DOC-001). Eq. (21) was confirmed against the typeset PDF on 2026-10-21: **the paper prints its lower summation limit as w−k+1, which is a typo; the correct limit is w−k** (see the validation report §6).
- A-5 Stack: C++20, CMake, GoogleTest or Catch2, Python with NumPy, pandas and Matplotlib, Git, and a CI workflow.
- A-6 Failure models must satisfy the paper's Assumptions (A)–(C). In particular, **components are exchangeable (identical in law)**, so the three differently-purposed pairs are treated as having the same lifetime model; this is a simplification to be stated in the report. Models whose MTTF is not monotone (e.g. sequential order statistics with a decreasing per-component rate) are outside the experiments' scope.

## 3. Functional requirements

Requirement IDs marked **(guide)** come directly from guide §6. Others are derived and marked **(derived)**.

### 3.1 System model

| ID | Priority | Requirement | Acceptance criterion |
|---|---|---|---|
| SYS-001 (guide) | P0 | The tool shall model a monotone binary system with n > 1 components and a non-trivial structure function (Φ(**0**) = 0, Φ(**1**) = 1). Irrelevant components shall be accepted. | Invalid configurations are rejected (n ≤ 1, constant Φ, non-monotone Φ); monotonicity is tested on supported model definitions; a system with an irrelevant component loads successfully. |
| SYS-003 (derived) | P0 | The tool shall load a system definition (components, labels, structure) from a JSON configuration, e.g. `configs/satellite_support.json`. | The reference file loads to 6 components and the expected Φ; missing/duplicate/unknown component IDs produce a descriptive error. |
| SYS-004 (derived) | P0 | The tool shall evaluate the structure function Φ for any valid state mask. | For the reference system, Φ is 1 for exactly 27 of 64 states (3 working choices per pair: 3³). |
| SYS-005 (derived) | P0 | The tool shall reject masks outside the range [0, 2ⁿ − 1] and component counts outside the supported maximum. | Out-of-range input raises a documented error; the supported maximum n is documented. |
| SYS-002 (guide) | P1 | The tool shall identify minimal path sets, as a by-product of the signature sweep, by flagging states with W₀ˣ = 1 and W₁ˣ = 0. No extra Φ evaluations are allowed. | Each returned set operates the system; removing any member makes it fail. For the reference system there are 8 minimal path sets (one per pair, 2³), each with 3 components. |

### 3.2 State-space and signature engines

| ID | Priority | Requirement | Acceptance criterion |
|---|---|---|---|
| ALG-001 (guide) | P0 | The tool shall enumerate states. | Six components produce 64 unique states. |
| ALG-002 (guide) | P0 | The tool shall compute per-state counting vectors with the paper's recursive algorithm. | Values match an independent brute-force implementation on small systems, for every state. |
| ALG-003 (guide) | P0 | The tool shall compute the tail signature and signature **at every state** (and report the system signature at the all-working state). | Entries are nonnegative and sum to 1 within a documented tolerance, for every operational state. Reference system signature at **1** is (0, 0.2, 0.4, 0.4, 0, 0). |
| ALG-004 (derived) | P0 | The signature engine shall sweep the states level by level (Algorithm 1: from the all-failed state up to the all-working state), evaluating Φ **exactly once per state** and keeping the counting vectors of only two consecutive levels. | Instrumented run shows exactly 2ⁿ calls to Φ (64 for the reference system) and no stored level older than the previous one (Theorem 2). |
| ALG-005 (derived) | P0 | The tool shall provide a brute-force reference counting-vector routine that does not call the recurrence. | Code review/inspection confirms no shared recurrence code; used by VER-001 tests. |
| ALG-006 (derived) | P1 | Complexity shall be measured and compared with the paper's bounds: O(n²2ⁿ) integer additions, O(n2ⁿ) other operations, 2ⁿ floats for the MTTFs plus O(√n·2ⁿ) integers for counting vectors (Theorem 2). | Experiment 4 reports operation counts and memory against these bounds. |

### 3.3 Failure model and MTTF engine

| ID | Priority | Requirement | Acceptance criterion |
|---|---|---|---|
| REL-003 (derived) | P0 | The tool shall provide a baseline failure model of independent exponential component lifetimes (the paper's [Exp] model, which satisfies Assumptions (A)–(C)). | E(Tₖ₊₁:w) − E(Tₖ:w) = 1/(λ(w−k)); invalid λ (≤ 0, NaN) is rejected. |
| REL-001 (guide) | P0 | The tool shall compute per-state MTTF with Eq. (7). | Results are nonnegative and match analytical/reference cases. Oracles: series of n → 1/(nλ); 1-of-2 → 1.5/λ; reference six-component system, all working → 0.7/λ; the paper's Fig. 1 system → m(**1**) = 7/6 (ORC-6). |
| REL-005 (derived) | P0 | The MTTF engine shall be cross-checked against a direct solution of the MTTF system, Eq. (6), for every state. | Eq. (6) and Eq. (7) agree within tolerance on all 64 states under [Exp] and (if REL-002 is implemented) [LSh]. |
| REL-002 (guide) | P1 | The tool shall support simultaneous failures within the paper's model class: exchangeable lifetimes whose survivor count is a Markov chain, specified by a Lévy-frailty Marshall-Olkin Laplace exponent Ψ (rates νw = Ψ(w), Eq. (19); kernel Pw,w′, Eq. (20); increments Eq. (21)). The minimum model is the lethal-shock model [LSh], Ψ(x) = (1−θ)x + θ for x > 0, with θ = β. | Transition probabilities Pw,w′ are nonnegative and each row sums to 1; the model is documented mathematically; a single event can fail ≥ 2 components. Reference-system MTTF at **1** under [LSh]: 8/(θ+3r) − 12/(θ+4r) + 6/(θ+5r) − 1/(θ+6r), r = 1−θ (θ = 1/2 → 32/35). |
| REL-004 (derived) | P1 | The common-cause model shall be calibrated as in the paper (Ψ(1) = 1, i.e. unit mean component lifetime) and parameterised by the pairwise beta factor β = 2 − Ψ(2) ∈ [0,1]; invalid parameters shall be rejected. | β outside [0,1] (and, for [LFMO-IG], above 2−√2 ≈ 0.586) is rejected; with β = 0 the results equal the [Exp] model. |

### 3.4 Verification, experiments and documentation

| ID | Priority | Requirement | Acceptance criterion |
|---|---|---|---|
| VER-001 (guide) | P0 | The project shall verify the algorithm. | Automated tests pass, including edge cases (tests A–F in [test-plan.md](test-plan.md)). |
| VER-002 (derived) | P0 | The numeric outputs shall be free of unexplained negative values, NaNs and infinities. | Test F passes across all test cases. |
| VER-003 (derived) | P0 | The project shall maintain a requirements-to-test traceability matrix. | Every requirement above maps to ≥ 1 test (see test plan §6). |
| VER-004 (derived) | P0 | The tests shall check the paper's structural properties. | (a) Signature dominance (Theorem 5): for states x, y with \|x\| = \|y\| and S̄ˣ ≥ S̄ʸ pointwise, m(x) ≥ m(y), strictly if the signatures differ and all increments are positive. (b) MTTF monotonicity (Lemma 4): m(x′) ≤ m(x) for x′ ≤ x under [Exp] and [LSh]. (c) Minimal path sets satisfy W₀ = 1 and W₁ = 0. |
| EXP-001 (guide) | P0 | Experiments shall be reproducible. | Configurations and random seeds reproduce the same outputs where randomness is used. |
| EXP-002 (derived) | P0 | The Python layer shall run Experiments 1–4 of the guide, saving raw output and configuration for each run. Experiment 2 shall use the paper's calibration (Ψ(1) = 1, β ∈ {1, 5, 10, 20, 50} %) and report the redundancy benefit **relative to a series baseline under the same model**, not only absolute MTTF. | Each run writes a result file plus the configuration used into `results/`. A hypothesis is recorded before the run. |
| EXP-003 (derived) | P0 | The Python layer shall produce plots and summary tables from saved results. | `plot_results.py` regenerates plots from `results/` alone, without re-running the engine. |
| EXP-004 (derived) | P2 | The tool may compare the MTTF-threshold policy π_γ (repair all failed components when m(x)/m(1) < γ, Eq. (13)) with the r-out-of-n failure-count policy, using the long-term mean cost LTMC = c_rep(**1**)/m_rep(**1**) (Eqs. (9)–(12)). All breakpoints of γ shall be searched, because the cost curve is not unimodal. | Decision rule, transition assumptions and cost parameters are documented; results are reproducible. Oracle: Fig. 1 system with c_comp = 1/30, c_sys = 1, c_set = 0 gives LTMC = 1/10, 11/15, 97/105 for the three distinct thresholds (ORC-7). |
| DOC-001 (guide) | P0 | The project shall document limitations. | Assumptions, complexity, and deviations from the paper are explicit in the report. The report states that the algorithm does **not** reduce the 2ⁿ evaluations of Φ; it provides all per-state signatures and MTTFs for the same count. |
| DOC-002 (derived) | P0 | Deliverables shall separate reproduced paper results from new experiments. | Report has distinct sections; the system is labelled fictional. |

## 4. Interface requirements

| ID | Requirement |
|---|---|
| INT-001 | The C++ engine shall expose a library API (`system_model`, `state`, `signature`, `failure_model`, `mttf`) and a command-line executable that reads a config file and writes JSON/CSV results. |
| INT-002 | The Python layer shall call the engine through the command-line executable (or bindings) and shall not reimplement the algorithm. |
| INT-003 | Configuration format is JSON; results are CSV or JSON with the configuration and seed recorded alongside. |
| INT-004 | Errors are reported with a message identifying the field/value and the violated rule, and a non-zero exit status from the CLI. |

## 5. Non-functional requirements

| ID | Category | Requirement |
|---|---|---|
| NFR-001 | Correctness | A small correct implementation takes precedence over a broad one. Numerical tolerance is documented (default relative tolerance 1e-9, to be confirmed in the verification report). |
| NFR-002 | Performance | The 6-component analysis completes in under 1 s on a typical laptop. Runtime, memory, structure-function calls and counting-vector operations are measurable (Experiment 4). |
| NFR-003 | Scalability | Exponential 2ⁿ growth is acknowledged; the practical n limit is benchmarked and documented. |
| NFR-004 | Maintainability | C++20, CMake build, one test file per module as in the guide's layout; the code is readable and mirrors the paper's notation. |
| NFR-005 | Portability | Builds on Linux with a standard compiler; compiler version and build type are recorded. |
| NFR-006 | Auditability | Every result can be traced to a configuration, code version and (where used) seed. |
| NFR-007 | CI | A CI workflow builds the code and runs the automated test suite on each push. |
| NFR-008 | Honesty | Documentation does not claim flight qualification, full reproduction of the paper before it is checked, or employer relevance beyond transferable skills. |
| NFR-009 | Numerical precision | Eq. (20) and Eq. (21) are alternating sums whose cancellation grows like 2ⁿ. In naive `double` the error of the increments is ≈ 8e-15 at n=6, 8e-9 at n=20, 6e-4 at n=30. **Implemented (Day 7):** the Lévy-frailty models compute the kernel and increments in `long double`, refuse any model whose transition probabilities do not form a distribution, and accept at most **n = 16** (`kMaxLevyFrailtyComponents`). Measured accuracy of a parallel-system MTTF against exact rational arithmetic: ≈ 5e-15 relative at n=6, 3e-12 at n=12, 2e-10 at n=16; at n=17 some parameter values are already refused. Counting vectors are integers (exact); the [Exp] model has no such issue. |

## 6. Data requirements

- **System configuration:** component ID, name, role, group (pair); structure definition (here: AND of OR-pairs).
- **State:** n-bit mask, working-count \|x\|, Φ(x).
- **Counting vector:** W₀…W_|x| for each state.
- **Signature:** vector of probabilities, one per failure order.
- **Failure model parameters:** type (`independent_exponential` / lethal shock), rate λ, beta factor β.
- **Result record:** configuration, seed, tool version, outputs, timestamp.

## 7. Deliverables (guide §12)
C++ implementation of the recursive algorithm · baseline MTTF and, if completed, simultaneous-failure model · brute-force reference · tests, build instructions and CI · architecture diagram and traceability matrix · Python experiments, plots and raw results · technical report · short demonstration.

## 8. Risks

| Risk | Mitigation (requirement) |
|---|---|
| Paper notation or indexing misread | Notation glossary; check equations against the paper (A-4, DOC-001) |
| Reference repeats the same bug | Keep it structurally independent; analytical oracles (ALG-005, REL-001) |
| Simultaneous-failure model incompatible with paper | Use only a model the paper supports (REL-002) |
| Exponential state-space growth | Benchmark and document limits (NFR-003) |
| Precision loss in alternating sums | Limit n or use extended precision (NFR-009) |
| Overclaiming fidelity or employer relevance | Label fictional; list omitted effects (NFR-008, DOC-002) |

## 9. Open items
1. ~~Confirm exact equations against the paper~~ — done for Eqs. (1)–(2), (5)–(8), (19)–(22). Eq. (21) has a typo in the paper's lower limit (should be w−k); see the validation report §6.
2. ~~Select the common-cause model~~ — resolved: Lévy-frailty Marshall-Olkin family, minimum [LSh] (REL-002). [BFR] was added on Day 7 (its transition kernel has an independent closed form that tests all of Eq. (20)); the stable subordinator [LFMO-S] is exercised in the tests; [LFMO-C], [LFMO-G] and [LFMO-IG] are not implemented.
3. Fix the numerical tolerance (suggest 1e-9 with `double` for n ≤ 12). **Decided on Day 2:** the maximum system size is n = 24 (`kMaxComponents`; validation of a 24-component system takes about 0.3 s). **Decided on Day 7:** the Lévy-frailty models apply their own limit of n = 16, set from measurements (see NFR-009).
4. ~~Choose GoogleTest or Catch2~~ — **decided: GoogleTest 1.14**. JSON parsing: **nlohmann/json 3.11.3** (MIT licence), also decided on Day 2.
5. Decide whether to use the authors' repository (`github.com/guidolagos/Subsystems`) as a third independent check.
7. **EXP-004 (optional P2) was implemented on 2026-10-22** as a simpler direct evaluation (n ≤ 14), with every threshold breakpoint searched; the paper's optimal state-based policy (§V-B) and single-pass evaluation (Proposition 3 over classes) are not implemented. See the deviation log (A7).
6. Confirm the reading of Algorithm 1 regarding the all-failed state: the paper sets its result without calling Φ, while Theorem 2.1 says exactly 2ⁿ evaluations. This specification counts Φ(**0**) too (exactly 2ⁿ calls).
