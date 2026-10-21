# Test Plan — Space System Reliability Analyzer

| | |
|---|---|
| **Version** | 1.1 (validated against the paper) |
| **Date** | 2026-10-20 |
| **Related** | [requirements-spec.md](requirements-spec.md) · [user-stories.md](user-stories.md) · [paper-validation-report.md](paper-validation-report.md) |

## 1. Purpose and approach

The test strategy rests on **two independent implementations** (guide §7):

1. a deliberately simple **brute-force reference** that counts operational substates directly, and
2. the paper's **recursive algorithm**.

The reference must not reuse the recurrence under test. It is itself validated against **hand-derived analytical values**, so a shared bug cannot go unnoticed.

Levels: unit tests (GoogleTest or Catch2, one file per module), integration tests (CLI → results files), and experiment-reproducibility checks (Python).

Record in the verification report: compiler version, build type, test command, results, and numerical tolerances.

**Numerical tolerance:** the default is relative/absolute 1e-9 (to be confirmed). It is written `TOL` below.

## 2. Analytical oracles

All values below follow the paper's Eqs. (1), (2), (7), (8) with i.i.d. exponential(λ) lifetimes (model [Exp]), where E(Tₖ₊₁:n) − E(Tₖ:n) = 1/(λ(n−k)). They were derived by hand and then **re-computed in exact rational arithmetic** with an independent script (brute-force subset counting, the Lemma 1 recursion, and a direct solution of the MTTF system Eq. (6)); all agree. ORC-6 and ORC-7 are reproduced from the paper's own Fig. 1.

The counting vector counts k-subsets of working components whose removal leaves the system operational. Define φₖ = Wₖ / C(n,k) and signature sₖ = φₖ₋₁ − φₖ.

| ID | System | n | W | Signature | MTTF·λ |
|---|---|---|---|---|---|
| ORC-1 | Series | 3 | [1,0,0,0] | (1,0,0) | 1/3 |
| ORC-2 | 1-out-of-2 | 2 | [1,2,0] | (0,1) | 3/2 |
| ORC-3 | Reference six-component, all working (mask 63) | 6 | [1,6,12,8,0,0,0] | (0, 0.2, 0.4, 0.4, 0, 0) | 0.7 |
| ORC-4 | Reference system, P1 failed (mask 62) | 5 | [1,4,4,0,0,0] | φ = [1, 0.8, 0.4, 0, 0, 0] → s = (0.2, 0.4, 0.4, 0, 0) | 8/15 ≈ 0.5333 |
| ORC-5 | Reference system, P1 and C1 failed (mask 0b111010 = 58) | 4 | [1,2,0,0,0] | φ = [1, 0.5, 0, 0, 0] → s = (0.5, 0.5, 0, 0) | 5/12 ≈ 0.4167 |

Derivations:

- ORC-3: k=2 removals avoiding a whole pair: C(6,2) − 3 = 12; k=3: one from each pair: 2³ = 8; k≥4: impossible. φ = [1, 1, 0.8, 0.4, 0, 0, 0]. MTTF·λ = 1/6 + 1/5 + 0.8/4 + 0.4/3 = 0.7. Cross-check by direct integration: ∫(2e⁻ᵗ − e⁻²ᵗ)³ dt = 8/3 − 3 + 6/5 − 1/6 = 0.7 ✓.
- ORC-4: P2 cannot be removed (its pair partner is gone), leaving 5 working components. W₁ = 4; W₂ = C(4,2) − 2 = 4; W₃ = 0. MTTF·λ = 1/5 + (4/5)/4 + (4/10)/3 = 8/15. Cross-check: ∫ e⁻ᵗ(2e⁻ᵗ − e⁻²ᵗ)² dt = 4/3 − 1 + 1/5 = 8/15 ✓.
- ORC-1: φ = [1,0,0,0]; MTTF·λ = 1/3 ✓ (min of 3 exponentials).
- ORC-2: MTTF = E[max of two exp(λ)] = 1.5/λ ✓.
- ORC-5 (two failures, still operational): with P1 and C1 failed, only K1 or K2 can still be removed, so W = [1,2,0,0,0] and MTTF·λ = 1/4 + (2/4)/3 = 5/12. With P1 and P2 failed the system is down (MTTF 0), so two states with the same failure count differ in reliability.

### Oracles taken from the paper

**ORC-6 — paper Fig. 1** (n = 3, Φ = max{x₃, min{x₁, x₂}}, [Exp] with unit mean; component order (x₁,x₂,x₃)):

| State | W | m(x)/m(**1**) |
|---|---|---|
| (1,1,1) | [1,3,1,0] (tail signature (1,1,1/3,0), signature (0,2/3,1/3)) | 1 (m(**1**) = 7/6) |
| (1,0,1), (0,1,1) | [1,1,0] | 6/7 ≈ 86 % |
| (0,0,1) | [1,0] | 6/7 ≈ 86 % |
| (1,1,0) | [1,0,0] | 3/7 ≈ 43 % |
| (1,0,0), (0,1,0), (0,0,0) | all zero | 0 |

**ORC-7 — paper Fig. 1 repair policies** (c_comp = 1/30, c_sys = 1, c_set = 0, [Exp]): the MTTF-threshold policy π_γ has LTMC = 1/10 for γ ∈ (86 %, 100 %], 11/15 for γ ∈ (43 %, 86 %], and 97/105 for γ ∈ (0, 43 %]. Reproduced exactly with Eqs. (9)–(12).

**ORC-8 — reference system under the lethal-shock model [LSh]** (Ψ(x) = (1−θ)x + θ, β = θ, Ψ(1) = 1). Closed form: MTTF(**1**) = 8/(θ+3r) − 12/(θ+4r) + 6/(θ+5r) − 1/(θ+6r) with r = 1−θ (a shock time Exp(θ) racing against the pair structure with independent rate r). Verified equal to Eq. (7) with increments from Eq. (21), and to Eq. (6) solved directly, on all 64 states.

| β = θ | MTTF(**1**) | MTTF(P1 failed) | MTTF(P1, C1 failed) | Series-of-6 baseline 1/Ψ(6) | Redundancy ratio |
|---|---|---|---|---|---|
| 0 ([Exp]) | 7/10 = 0.7000 | 8/15 | 5/12 | 1/6 | 4.20 |
| 0.05 | 147467/205436 ≈ 0.71782 | ≈ 0.54868 | ≈ 0.42991 | 1/5.75 | 4.13 |
| 0.2 | 6003/7735 ≈ 0.77608 | ≈ 0.60009 | ≈ 0.47511 | 1/5 | 3.88 |
| 0.5 | 32/35 ≈ 0.91429 | 11/15 | 3/5 | 1/3.5 | 3.20 |

> **Important (corrects an earlier draft of this plan).** Under the paper's calibration (unit mean component lifetime), absolute MTTF *increases* with β for this system, because failures are bunched. A test must **not** assert that more common-cause failure lowers absolute MTTF. What falls is the *redundancy ratio* MTTF(**1**) / (series-of-6 MTTF), where series-of-6 MTTF = 1/Ψ(6) = 1/(6(1−θ)+θ). Use the ratios in the table (4.20, 4.13, 3.88, 3.20), which decrease monotonically.

## 3. Test cases

Naming: **TC-x-nn**. "Req" lists the requirements covered.

### Test A — Series system

| ID | Setup | Expected | Req |
|---|---|---|---|
| TC-A-01 | n = 1..6 series systems (works only if all work) | Exactly one operational state (all-ones mask); Φ = 0 for all other masks | SYS-004, ALG-001 |
| TC-A-02 | Counting vectors of the all-working state | W = [1,0,…,0] for each n | ALG-002 |
| TC-A-03 | Signature of the series system | s = (1,0,…,0), sums to 1 | ALG-003 |
| TC-A-04 | MTTF with i.i.d. exp(λ), several λ | MTTF = 1/(nλ) within TOL | REL-001, REL-003 |

### Test B — One-out-of-two redundancy

| ID | Setup | Expected | Req |
|---|---|---|---|
| TC-B-01 | n = 2, Φ = x₁ ∨ x₂ | Φ fails only for mask 00; 3 operational states | SYS-004 |
| TC-B-02 | Counting vector, state 11 | W = [1,2,0] (ORC-2) | ALG-002 |
| TC-B-03 | Signature | s = (0,1) | ALG-003 |
| TC-B-04 | MTTF with exp(λ) | 1.5/λ | REL-001 |
| TC-B-05 | Per-state MTTF for states 01 and 10 | 1/λ (one exp component left) | REL-001 |

### Test C — Six-component architecture

| ID | Setup | Expected | Req |
|---|---|---|---|
| TC-C-01 | Enumerate all masks 0..63 | 64 unique states; each mask round-trips to a component set | ALG-001 |
| TC-C-02 | Count Φ = 1 states | 27 | SYS-004 |
| TC-C-03 | Recursive vs brute-force counting vectors, **all 64 states** | Identical W for every state (integer-valued within TOL) | ALG-002, VER-001, ALG-005 |
| TC-C-04 | State 63 | W = [1,6,12,8,0,0,0] (ORC-3) | ALG-002 |
| TC-C-05 | State 62 (P1 failed) | W = [1,4,4,0,0,0] (ORC-4) | ALG-002 |
| TC-C-06 | Signature of the full system | (0, 0.2, 0.4, 0.4, 0, 0); sums to 1 | ALG-003 |
| TC-C-07 | MTTF for state 63 with exp(λ) | 0.7/λ; state 62 → 8/(15λ) | REL-001 |
| TC-C-08 | Non-operational states (Φ = 0) | MTTF = 0 | REL-001 |
| TC-C-09 | Level sweep (Theorem 2) | Φ is called **exactly 64 times**, once per state; the sweep goes from the all-failed state to the all-working state; at most two consecutive levels of counting vectors are held in memory | ALG-004 |
| TC-C-10 | Minimal path sets | 8 sets, each of size 3 (one per pair); each operates the system; removing any member fails it; no duplicate or superset | SYS-002 |
| TC-C-11 | Monotonicity | For every state x and working component i, Φ(x without i) ≤ Φ(x) | SYS-001 |
| TC-C-12 | Config loading | `satellite_support.json` loads 6 components with IDs P1,P2,C1,C2,K1,K2 and bits 0–5 | SYS-003 |
| TC-C-13 | Eq. (6) vs Eq. (7) under [Exp] | MTTF from the sweep equals the MTTF from solving Eq. (6) directly, for all 64 states (within TOL) | REL-005 |
| TC-C-14 | Paper Fig. 1 system (ORC-6) | W vectors, signature (0, 2/3, 1/3), m(**1**) = 7/6 and the per-state ratios in ORC-6 | ALG-002, ALG-003, REL-001 |

### Test D — Simultaneous failures

*Applicable only if REL-002 is implemented (P1).*

| ID | Setup | Expected | Req |
|---|---|---|---|
| TC-D-01 | Common-cause parameter β = 0 | Results equal the baseline independent model (all of Test C) | REL-004 |
| TC-D-02 | Common-cause event enabled | A transition can fail ≥ 2 components in one event; the resulting state is a valid mask | REL-002 |
| TC-D-03 | Model constraints | Transition probabilities/rates are nonnegative and satisfy the documented normalisation | REL-002 |
| TC-D-04 | Redundancy benefit vs β | With Ψ(1) = 1, the ratio MTTF(**1**) / (1/Ψ(6)) for the reference system decreases as β grows (4.20 → 3.88 → 3.20 at β = 0, 0.2, 0.5). **Do not** assert that absolute MTTF decreases (it increases; see ORC-8). | REL-002, REL-004 |
| TC-D-05 | Closed-form check | [LSh] MTTF(**1**) equals the ORC-8 closed form for β = 0.05, 0.2, 0.5 (within TOL) | REL-002 |
| TC-D-06 | Eq. (6) vs Eq. (7) under [LSh] | Direct solution of the MTTF system using νw = Ψ(w) and Pw,w′ from Eq. (20) equals the sweep result on all 64 states | REL-005, REL-002 |
| TC-D-07 | Kernel validity | For w = 1 … 6, the Pw,w′ (w′ < w) are ≥ 0 (within TOL) and sum to 1; mean number failing per epoch equals wΨ(1)/Ψ(w) (Remark 1) | REL-002 |
| TC-D-08 | Calibration | β = 2 − Ψ(2); Ψ(1) = 1; β = 0 reproduces [Exp] exactly; β outside [0,1] is rejected | REL-004 |

### Test E — Invalid models and inputs

| ID | Input | Expected | Req |
|---|---|---|---|
| TC-E-01 | Empty component list; n = 0 | Rejected with descriptive error | SYS-001 |
| TC-E-02 | Duplicate component IDs | Rejected naming the duplicate | SYS-003 |
| TC-E-03 | Structure references an unknown component | Rejected naming the unknown ID | SYS-003 |
| TC-E-04 | Non-monotone structure function (e.g. XOR) | Rejected as non-monotone | SYS-001 |
| TC-E-05 | Mask < 0 or ≥ 2ⁿ | Rejected | SYS-005 |
| TC-E-06 | n above the supported maximum | Rejected with the documented limit | SYS-005, NFR-003 |
| TC-E-07 | λ ≤ 0, NaN, ∞ | Rejected | REL-003 |
| TC-E-08 | Probability < 0 or > 1 (β); unknown model type | Rejected | REL-004 |
| TC-E-09 | Malformed JSON; missing required fields | CLI exits non-zero with a message naming the field | INT-004 |
| TC-E-10 | n = 1 | Rejected (the paper requires n > 1) | SYS-001 |
| TC-E-11 | Constant Φ (always 0 or always 1) | Rejected as trivial (needs Φ(**0**) = 0 and Φ(**1**) = 1) | SYS-001 |
| TC-E-12 | Structure with an irrelevant component (e.g. Φ ignores x₃) | **Accepted**; the irrelevant component never appears in a minimal path set; recursive = brute force | SYS-001, SYS-002 |

### Test F — Numerical checks

| ID | Setup | Expected | Req |
|---|---|---|---|
| TC-F-01 | Every state of every test system | All Wₖ ≥ 0; no NaN/Inf | VER-002, ALG-002 |
| TC-F-02 | Every signature | Entries ≥ −TOL; sum = 1 ± TOL | ALG-003 |
| TC-F-03 | Every MTTF | ≥ 0 and finite | REL-001, VER-002 |
| TC-F-04 | Sweep λ from 1e-6 to 1e3 | MTTF scales as 1/λ (MTTF·λ constant within TOL) | REL-003 |
| TC-F-05 | Random monotone systems, n ≤ 8 (fixed seed) | Recursive = brute-force for all states; signatures valid | VER-001, EXP-001 |
| TC-F-06 | Boundary states | Mask 0 (all failed) and all-ones handled; W₀ = Φ | ALG-002 |
| TC-F-07 | Precision of Ψ-derived increments | Compare Eq. (21) in `double` with an exact/extended-precision reference for n = 6, 12, 20 ([LSh], θ = 0.2). Expect relative error ≈ 1e-14, 1e-11, 1e-8; any n for which error exceeds TOL must be rejected or run in extended precision | NFR-009 |
| TC-F-08 | Increments positive | δ_{w,k} = E(T_{k+1:w}) − E(T_{k:w}) > 0 for every model and w, k used | REL-001 |

### Test G — Experiment and reproducibility (Python)

| ID | Setup | Expected | Req |
|---|---|---|---|
| TC-G-01 | Run `run_experiments.py` twice with the same config and seed | Byte-identical (or tolerance-equal) result files | EXP-001 |
| TC-G-02 | Result file contents | Includes configuration, seed, tool version and raw outputs | EXP-002, NFR-006 |
| TC-G-03 | `plot_results.py` run on saved results only | Generates plots without invoking the engine | EXP-003 |
| TC-G-04 | Experiment 1 (series vs redundant pair vs six-component) | Output has initial-state MTTF, signature, operational-state count and runtime for each system | EXP-002 |
| TC-G-05 | Experiment 3 (equal failure counts) | At least one pair of states with equal failed counts but different MTTF (ORC-5: P1+C1 failed has MTTF·λ = 5/12; P1+P2 failed has MTTF 0) | EXP-002 |
| TC-G-06 | Experiment 4 (scaling) | Runtime/memory/call counts recorded for increasing n; brute-force compared only where practical | EXP-002, NFR-003, ALG-006 |
| TC-G-07 | Hypothesis file exists before each run | Present and dated before results | EXP-002 |

### Test H — Non-functional and documentation

| ID | Check | Expected | Req |
|---|---|---|---|
| TC-H-01 | Time the six-component analysis | < 1 s | NFR-002 |
| TC-H-02 | Clean build with CMake + CI | Builds with no errors; CI passes the unit tests | NFR-004, NFR-005, NFR-007 |
| TC-H-03 | Documentation review | Limitations, complexity, deviations, fictional-system label and the reproduced-vs-new separation are present | DOC-001, DOC-002, NFR-008 |
| TC-H-04 | CLI end-to-end | `analyze --config configs/satellite_support.json` writes results with exit code 0 | INT-001, INT-003 |

### Test I — Properties proved in the paper

| ID | Setup | Expected | Req |
|---|---|---|---|
| TC-I-01 | Signature dominance (Theorem 5): all pairs x, y with \|x\| = \|y\| in the reference system and in random monotone systems (n ≤ 8, fixed seed), under [Exp] and [LSh] | If S̄ˣₖ ≥ S̄ʸₖ for all k then m(x) ≥ m(y); strict when the tail signatures differ | VER-004 |
| TC-I-02 | MTTF monotonicity (Lemma 4): x′ ≤ x | m(x′) ≤ m(x) under [Exp] and [LSh] (verified exactly on the reference system) | VER-004 |
| TC-I-03 | Minimal path sets: states with W₀ = 1 and W₁ = 0 | Equals the set of minimal path sets from an independent enumeration; reference system → 8 sets of size 3 | VER-004, SYS-002 |
| TC-I-04 | The converse of Theorem 5 is false | Find (or document) a pair of states with the same \|x\| and crossing tail signatures; do not assert a total order | VER-004 |

### Test J — Repair policies (only if EXP-004 is implemented, P2)

| ID | Setup | Expected | Req |
|---|---|---|---|
| TC-J-01 | Fig. 1 system, [Exp], c_comp = 1/30, c_sys = 1, c_set = 0 | LTMC = 1/10, 11/15, 97/105 for the three distinct thresholds (ORC-7) | EXP-004 |
| TC-J-02 | All breakpoints of γ ↦ LTMC_γ are evaluated | The best threshold equals the minimum over all distinct values of m(x)/m(**1**); a search that stops at the first cost increase must not be used | EXP-004 |
| TC-J-03 | γ → 0 and γ = 1 | γ → 0 is corrective maintenance (repair only after failure); γ = 1 repairs at the first failure of a relevant component | EXP-004 |

## 4. Test data

- `configs/satellite_support.json` (reference system).
- Generated series, parallel and mixed topologies for n ≤ 8.
- Invalid-input fixtures for Test E.
- Fixed random seeds (stored with each result file).

## 5. Entry and exit criteria

**Entry:** requirements baselined; notation glossary written; reference implementation compiles.

**Exit:** all P0 tests pass; P1 tests pass for implemented P1 features, and any unimplemented P1 item is recorded as a known gap; no unexplained NaN/negative values; verification report completed; traceability complete.

## 6. Traceability matrix

| Requirement | Test cases |
|---|---|
| SYS-001 | TC-C-11, TC-E-01, TC-E-04, TC-E-10, TC-E-11, TC-E-12 |
| SYS-002 | TC-C-10, TC-E-12, TC-I-03 |
| SYS-003 | TC-C-12, TC-E-02, TC-E-03 |
| SYS-004 | TC-A-01, TC-B-01, TC-C-02 |
| SYS-005 | TC-E-05, TC-E-06 |
| ALG-001 | TC-A-01, TC-C-01 |
| ALG-002 | TC-A-02, TC-B-02, TC-C-03, TC-C-04, TC-C-05, TC-C-14, TC-F-01, TC-F-06 |
| ALG-003 | TC-A-03, TC-B-03, TC-C-06, TC-C-14, TC-F-02 |
| ALG-004 | TC-C-09 |
| ALG-005 | TC-C-03 (plus code inspection) |
| ALG-006 | TC-G-06 |
| REL-001 | TC-A-04, TC-B-04, TC-B-05, TC-C-07, TC-C-08, TC-C-14, TC-F-03, TC-F-08 |
| REL-002 | TC-D-02, TC-D-03, TC-D-04, TC-D-05, TC-D-06, TC-D-07 |
| REL-003 | TC-A-04, TC-E-07, TC-F-04 |
| REL-004 | TC-D-01, TC-D-04, TC-D-08, TC-E-08 |
| REL-005 | TC-C-13, TC-D-06 |
| VER-001 | TC-C-03, TC-F-05 |
| VER-002 | TC-F-01, TC-F-03 |
| VER-003 | This table (review) |
| VER-004 | TC-I-01, TC-I-02, TC-I-03, TC-I-04 |
| EXP-001 | TC-F-05, TC-G-01 |
| EXP-002 | TC-G-02, TC-G-04, TC-G-05, TC-G-06, TC-G-07 |
| EXP-003 | TC-G-03 |
| EXP-004 (P2) | TC-J-01, TC-J-02, TC-J-03 (only after the core is verified) |
| DOC-001 | TC-H-03 |
| DOC-002 | TC-H-03 |
| INT-001, INT-003 | TC-H-04 |
| INT-004 | TC-E-09 |
| NFR-002 | TC-H-01 |
| NFR-003 | TC-E-06, TC-G-06 |
| NFR-004, NFR-005, NFR-007 | TC-H-02 |
| NFR-006 | TC-G-02 |
| NFR-008 | TC-H-03 |
| NFR-009 | TC-F-07 |

## 7. Defect handling

A failing recursive-vs-reference comparison is first checked against the analytical oracles, to decide which implementation is wrong before fixing either. Each defect is logged in the verification report with the failing test ID.
