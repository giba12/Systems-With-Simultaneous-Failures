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

**ORC-7 — paper Fig. 1 repair policies** (c_comp = 1/30, c_sys = 1, c_set = 0, [Exp]): the MTTF-threshold policy π_γ has LTMC = 1/10 for γ ∈ (86 %, 100 %], 11/15 for γ ∈ (43 %, 86 %], and 97/105 for γ ∈ (0, 43 %]. Reproduced exactly with Eqs. (9)–(12), first in a throw-away validation script and now by the delivered policy code (TC-J-01, TC-H-08, TC-G-11).

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
| TC-C-15 | Sweep on arbitrary Boolean Φ (random truth tables, n = 1..8, including non-monotone and trivial ones) | Sweep equals brute force on every state — the paper states Algorithm 1 makes no assumption on Φ (Lemma 1 is pure counting) | ALG-002, VER-001 |
| TC-C-17 | Classic 5-component bridge network | Signature (0, 1/5, 3/5, 1/5, 0) (hand-derived from the 2 size-2 cuts and 2 size-2 paths), W = [1,5,8,2,0,0], 4 minimal path sets of sizes 2,2,3,3 | ALG-003, SYS-002 |
| TC-C-18 | k-out-of-n:G systems, n ≤ 9, all k | Signature is a point mass at position n−k+1; W[j] = C(n,j) for j ≤ n−k, else 0 | ALG-002, ALG-003 |
| TC-C-19 | Relabelling invariance and series-of-parallel block | Permuting component bits leaves the signature unchanged; (3-parallel) in series with 1 has signature (1/4, 1/4, 1/2, 0) | ALG-003 |
| TC-C-20 | Satellite MTTF on **all 64 states** vs. an independent closed form | Integral formula Σⱼ C(p2,j) 2^(p2−j) (−1)^j / (λ(p1+p2+j)) for p2 healthy and p1 degraded pairs (0 if a pair is gone) agrees for several λ — no sweep and no Eq. (6) involved | REL-001 |
| TC-C-21 | k-out-of-n:G MTTF on every state, n ≤ 9, all k | MTTF = Σ_{j=k}^{w} 1/(jλ) for a state with w ≥ k working components, else 0 | REL-001, REL-003 |
| TC-C-22 | Operation counts (Theorem 2.3) | the sweep's **measured** integer additions equal n(n+1)2ⁿ⁻² and divisions n·2ⁿ⁻¹ exactly for n = 2…14, independent of the structure function | ALG-006 |
| TC-C-16 | Sweep bookkeeping | The sweep resets the Φ-call counter itself; the observer sees each state once, level by level in increasing mask order; vectors are kept only on request; the peak number of live vectors equals the largest sum of two adjacent binomials | ALG-004 |

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
| TC-D-09 | [LSh] kernel vs independent closed form | P(w,w−1) = (1−β)w/Ψ(w), P(w,0) = β/Ψ(w), all else 0, for w ≤ 16 | REL-002 |
| TC-D-10 | [BFR] kernel vs independent closed form (tests all of Eq. (20)) | P(w,w−j) = C(w,j)pʲ(1−p)^{w−j} / (1−(1−p)^w), j = 1…w, p = β | REL-002 |
| TC-D-11 | The paper's printed Eq. (21) is wrong | With lower limit w−k+1 the sum is 0 for k = 0 and wrong for k ≥ 1; the corrected limit w−k equals the recursion | REL-002 |
| TC-D-12 | Other Ψ families and edge cases | Stable subordinator Ψ(x) = x^α gives a valid kernel; a non-concave Ψ (x²), a decreasing or uncalibrated Ψ are rejected; β = 1 fails everything at once (all increments after the first are 0, every working state has MTTF 1) | REL-002, REL-004 |
| TC-D-13 | Satellite under [LSh]: all 64 states, plus series/parallel | Independent integral closed forms agree (satellite 1e-11, series 1e-12, parallel 1e-9); exact values 32/35 (β = 0.5), 6003/7735 (0.2), 147467/205436 (0.05) | REL-002 |
| TC-D-14 | Eq. (6) vs Eq. (7) under [LSh], [BFR], stable | Agree to 1e-10 on 28 systems × 5 models | REL-005, REL-002 |
| TC-D-15 | Table I parameters | Calibrating to β = 5 %, 20 %, 50 % gives the paper's parameters: [LFMO-C] p = 0.0256 / 0.111 / 0.333, [LFMO-G] η = 18.51 / 3.537 / 0.618, [LFMO-IG] η = 4.188 / 1.634 / 0.289, [LFMO-S] α = 0.963 / 0.848 / 0.585 | REL-004, REL-002 |
| TC-D-16 | Bundle-size ranges at n = 30 | n/Ψ(30) for [LSh] and [BFR] equals the paper's quoted ranges (1.051–1.935 and 1.910–15.000), [LSh] being the minimum and [BFR] the maximum of the six models | REL-002 |
| TC-D-17 | Six-model calibration, kernel and Remark 1 | For each new model: Ψ(1) = 1, β = 2 − Ψ(2), kernel rows sum to 1, no negative entry, and the kernel matches the closed-form mean bundle size | REL-002, REL-004 |
| TC-D-18 | Parameter ranges and degenerate cases | Out-of-range β is rejected per model ([LFMO-IG] ≤ 2−√2, [LFMO-G] in (0,1)); β = 0 gives [Exp]; β = 1 gives the all-at-once limit; the [LFMO-IG] boundary limit is Ψ(x) = √x | REL-004 |
| TC-D-19 | One redundant pair is model-independent | The MTTF of one pair is identical under all six models (same β), while the satellite's differs | REL-002 |
| TC-D-20 | Eq. (6) vs Eq. (7) under the four new models | Agree to 1e-10 on the same 28 systems | REL-005, REL-002 |

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
| TC-E-13 | Path-set structures (OR of ANDs) | the bridge network from its four minimal path sets, from code and JSON (`path_sets`), equals the direct definition; empty path sets, unknown ids, missing fields and an empty path-set list are rejected | SYS-001, SYS-003 |
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
| TC-F-07 | Precision of Ψ-derived quantities | Kernel and increments in `long double`; parallel-system MTTF under [LSh] (β = 0.2) vs exact rational arithmetic: relative error ≤ 1e-13 at n=6, ≤ 1e-10 at n=12, ≤ 1e-9 at n=16 (measured 5e-15, 3e-12, 2e-10); n = 17 is refused; the two increment routes agree to 1e-11 (n ≤ 12) / 1e-9 (n ≤ 16) of the scale E T_{w:w} | NFR-009 |
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
| TC-H-04 | CLI end-to-end | `analyze --config configs/satellite_support.json` writes results with exit code 0; JSON fields, per-state records, `--out`, determinism apart from `timing`, all three failure models, failure model from the config file with the command line winning | INT-001, INT-003 |
| TC-H-05 | CLI errors | Invalid input → exit 1 with a message naming the problem (missing file, malformed JSON, n = 1, duplicate id, missing/invalid β or λ, unwritable output, n > 16 for the simultaneous-failure models); usage errors → exit 2 | INT-004 |
| TC-H-09 | CLI: all six common-cause models | `--model lsh|bfr|lfmo-c|lfmo-g|lfmo-ig|lfmo-s` each run, pass `--verify` and compare policies on the satellite, with six different results; β ranges and the missing-β error are reported by the tool; the model can come from the configuration file | INT-001, REL-002 |
| TC-H-08 | CLI repair policies | `--c-comp/--c-sys/--c-set` reproduce the paper's Fig. 1 costs through the tool; no policies without costs; works for every failure model; giving only some of the three flags is a usage error (2); negative costs and n > 14 are input errors (1) | EXP-004, INT-001 |
| TC-H-07 | Traceability is machine-checked | `project/tools/check_traceability.py` finds no problem: every requirement has a test case, every matrix entry exists, every test case carries a tag in test code or is classified as verified another way; it runs under ctest | VER-003 |
| TC-H-06 | The verification can fail | With a labelled fault injected (miscounted vector or drifting MTTF) `--verify` exits 3, still writes the JSON, names the first mismatching state, and an impossible vector is a failed verification, not a crash; the injection does nothing without `--verify` | VER-001, INT-004 |
| TC-G-11 | Experiment 5 | covers 6 × 45 cost-grid analyses (satellite under three models, three asymmetric systems) plus the paper's Fig. 1 setting (97/105, 11/15, 1/10); c_sys = 1, c_comp = ρ/n, c_set = σ in every run; all pre-registered predictions are evaluated deterministically | EXP-002, EXP-004 |
| TC-G-10 | The Python layer never re-implements the algorithm | `run_experiments.py` imports only the standard library; the saved raw results are exactly what `analyze` printed for the same inputs | INT-002 |
| TC-G-08 | Experiment runner and results | Refuses to run without a (non-empty) hypothesis; copies it and records its SHA-256; results carry settings, seed, tool version; `hypothesis_written_before_run`; the deterministic file is byte-identical across runs for all four experiments; an explicit bad `--analyze` path is an error | EXP-001, EXP-002, NFR-006 |
| TC-G-09 | Analysis and plots | Verdict file with a SUPPORTED/REJECTED line per prediction; results without provenance are refused; the four figures are drawn from the copied tables alone (valid PNGs, no subprocess use) | EXP-003 |

### Test I — Properties proved in the paper

| ID | Setup | Expected | Req |
|---|---|---|---|
| TC-I-01 | Signature dominance (Theorem 5): all pairs x, y with \|x\| = \|y\| in the reference system and in random monotone systems (n ≤ 8, fixed seed), under [Exp] and [LSh] | If S̄ˣₖ ≥ S̄ʸₖ for all k then m(x) ≥ m(y); strict when the tail signatures differ | VER-004 |
| TC-I-02 | MTTF monotonicity (Lemma 4): x′ ≤ x | m(x′) ≤ m(x) under [Exp], [LSh] (β = 0.2, 0.5) and [BFR] (β = 0.3), on > 40 000 state pairs | VER-004 |
| TC-I-03 | Minimal path sets: states with W₀ = 1 and W₁ = 0 | Equals the set of minimal path sets from an independent enumeration; reference system → 8 sets of size 3 | VER-004, SYS-002 |
| TC-I-04 | The converse of Theorem 5 is false | Deterministic counterexample: 6-component system with minimal path sets {0,5}, {3,4}, {1,4}, {3}, {0,1,3,5}; states x = mask 15 (W = [1,3,3,1,0], MTTF 1) and y = mask 51 (W = [1,4,2,0,0], MTTF 3/4) have crossing tail signatures yet m(x) > m(y). Crossings do not occur in systems with fewer than 5 components | VER-004 |

### Test J — Repair policies (optional P2, EXP-004; implemented on Day 9+)

| ID | Setup | Expected | Req |
|---|---|---|---|
| TC-J-01 | Fig. 1 system, [Exp], c_comp = 1/30, c_sys = 1, c_set = 0 | exactly three threshold policies with γ-intervals (0, 3/7], (3/7, 6/7], (6/7, 1] and LTMC = 97/105, 11/15, 1/10 (ORC-7); repairing at the first failure is cheapest; cycle length 7/6 | EXP-004 |
| TC-J-02 | All breakpoints of γ ↦ LTMC_γ are evaluated | one policy per distinct operational MTTF ratio (4 for the satellite); the best is the minimum over all of them. A **non-unimodal** example (8 components, path sets 90, 203, 22, 41, ρ = 10, σ = 0): three local minima; a search that decreases γ from 1 and stops at the first rise ends at policy 8 and misses the optimum (policy 6) by 0.15 % | EXP-004 |
| TC-J-03 | γ → 0, γ = 1, r = n | γ → 0 is corrective maintenance; γ = 1 is the r = 1 policy on systems whose components are all relevant; r = n is corrective maintenance | EXP-004 |
| TC-J-04 | Closed forms from the **signature** (independent exponential lifetimes) | corrective: LTMC = (c_set + c_sys + c_comp Σ k·sₖ) / MTTF; r-out-of-n for every r: the cycle ends at failure min(K, r), so length = Σ sₖ E T_{min(k,r):n}, cost = c_set + c_comp Σ sₖ min(k,r) + c_sys Σ_{k≤r} sₖ; both agree with the policy evaluator to 1e-12 on the satellite, Fig. 1, the bridge and random systems | EXP-004 |
| TC-J-05 | Scaling laws, preconditions, simultaneous failures | costs × 2 → LTMC × 2; λ × 2 → length ÷ 2, LTMC × 2; zero costs → 0; corrective cycle length = MTTF under [Exp], [LSh], [BFR]; invalid policies (repair at the all-working state, no repair at a failed state, wrong size), negative or non-finite costs, r or γ out of range, n > 14 → ModelError | EXP-004 |
| TC-J-06 | The paper's **Proposition 3** as an independent route | for every threshold policy of the satellite, Fig. 1, the bridge and 10 random systems, under [Exp], [LSh] and [BFR] and three cost sets (> 300 policies): cycle length and cost equal T, c_set + c_comp N + c_sys P computed from state-occupation times without any policy recursion (1e-9) | EXP-004 |

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
| SYS-001 | TC-C-11, TC-E-01, TC-E-04, TC-E-10, TC-E-11, TC-E-12, TC-E-13 |
| SYS-002 | TC-C-10, TC-E-12, TC-I-03 |
| SYS-003 | TC-C-12, TC-E-02, TC-E-03, TC-E-13 |
| SYS-004 | TC-A-01, TC-B-01, TC-C-02 |
| SYS-005 | TC-E-05, TC-E-06 |
| ALG-001 | TC-A-01, TC-C-01 |
| ALG-002 | TC-A-02, TC-B-02, TC-C-03, TC-C-04, TC-C-05, TC-C-14, TC-C-15, TC-F-01, TC-F-06 |
| ALG-003 | TC-A-03, TC-B-03, TC-C-06, TC-C-14, TC-C-17, TC-C-18, TC-C-19, TC-F-02 |
| ALG-004 | TC-C-09, TC-C-16 |
| ALG-005 | TC-C-03 (plus code inspection) |
| ALG-006 | TC-G-06, TC-C-22 |
| REL-001 | TC-A-04, TC-B-04, TC-B-05, TC-C-07, TC-C-08, TC-C-14, TC-C-20, TC-C-21, TC-F-03, TC-F-08 |
| REL-002 | TC-D-02, TC-D-03, TC-D-04, TC-D-05, TC-D-06, TC-D-07, TC-D-09 … TC-D-20, TC-H-09 |
| REL-003 | TC-A-04, TC-E-07, TC-F-04 |
| REL-004 | TC-D-01, TC-D-04, TC-D-08, TC-D-15, TC-D-17, TC-D-18, TC-E-08 |
| REL-005 | TC-C-13, TC-D-06 |
| VER-001 | TC-C-03, TC-F-05 |
| VER-002 | TC-F-01, TC-F-03 |
| VER-003 | TC-H-07 (the generated matrix in [traceability-matrix.md](traceability-matrix.md)) |
| VER-004 | TC-I-01, TC-I-02, TC-I-03, TC-I-04 |
| EXP-001 | TC-F-05, TC-G-01 |
| EXP-002 | TC-G-02, TC-G-04, TC-G-05, TC-G-06, TC-G-07, TC-G-11 |
| EXP-003 | TC-G-03 |
| EXP-004 (P2) | TC-J-01 … TC-J-06, TC-G-11, TC-H-08 |
| DOC-001 | TC-H-03 |
| DOC-002 | TC-H-03 |
| INT-001, INT-003 | TC-H-04, TC-H-08, TC-H-09 |
| INT-002 | TC-G-10 |
| INT-004 | TC-E-09 |
| NFR-001 | TC-H-03 (tolerances documented in [verification-report.md](verification-report.md)) |
| NFR-002 | TC-H-01 |
| NFR-003 | TC-E-06, TC-G-06 |
| NFR-004, NFR-005, NFR-007 | TC-H-02 |
| NFR-006 | TC-G-02 |
| NFR-008 | TC-H-03 |
| NFR-009 | TC-F-07 |

## 7. Defect handling

A failing recursive-vs-reference comparison is first checked against the analytical oracles, to decide which implementation is wrong before fixing either. Each defect is logged in the verification report with the failing test ID.
