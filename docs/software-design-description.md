# Software Design Description — Space System Reliability Analyzer

| | |
|---|---|
| **Version** | 1.0 |
| **Date** | 2026-10-29 |
| **Template** | DI-IPSC-81435A, *Software Design Description* (sections 1 – 7 of the DID, tailored to one CSCI with no database and no real-time constraints; DID headings such as "Execution control" and "Data" are kept where they say something). Written from the DID's content structure, not from a fresh copy: confirm the current revision in the DLA ASSIST database before any contractual use |
| **Describes** | `analyze` 0.8.0 and library `reliability`, commit `b662153` |
| **Related** | [interface-design-description.md](interface-design-description.md) · [decision-log.md](decision-log.md) · [requirements-spec.md](requirements-spec.md) · [notation-glossary.md](notation-glossary.md) · [uml/component.png](uml/component.png) |

Equation numbers refer to Lagos & Ávila, arXiv:2610.22181. The satellite-support system is fictional.

## 1. Scope

### 1.1 Identification
One CSCI, **Space System Reliability Analyzer**: the static library `reliability` (7 translation units), the executable
`analyze`, the Python experiment layer, and the traceability tool.

### 1.2 System overview
Given a monotone binary system of n components, the software computes for **every** state x (a subset of working
components) the counting vector, the signature and the mean time to failure m(x), under a failure model that may fail
several components at once. It verifies its own answers, compares repair policies, and runs reproducible experiments.
See [conops.md](conops.md).

### 1.3 Document overview
Section 3 gives the design decisions that cut across modules, section 4 the architecture, section 5 each module in detail,
section 6 the traceability to requirements.

## 2. Referenced documents

[requirements-spec.md](requirements-spec.md) · [interface-design-description.md](interface-design-description.md) ·
[notation-glossary.md](notation-glossary.md) · [paper-validation-report.md](paper-validation-report.md) ·
[deviation-log.md](deviation-log.md) · [decision-log.md](decision-log.md) · UML diagrams in [uml/](uml/).

## 3. CSCI-wide design decisions

| # | Decision | Rationale | Record |
|---|---|---|---|
| D-1 | **A state is a bit mask** (`uint32_t`, bit *i* = 1 ⇔ component *i* works); n ≤ 24 | one word per state, popcount gives the level, no allocation per state | `state.hpp`; ADR-0001 |
| D-2 | **Φ is called exactly once per state**, 2ⁿ calls in all, and counted | the paper's complexity claim is checkable; Φ(**0**) is also evaluated | `SystemModel::phi_calls`; deviation A2 |
| D-3 | **Only two levels of counting vectors live at once**; vectors are exact integers (`uint64_t`), each division is exact (Lemma 1) and a remainder is a bug | memory O(√n · 2ⁿ) integers instead of 2ⁿ vectors; no rounding in the combinatorial part | ADR-0001 |
| D-4 | **Failure model behind an abstract interface** (ν(w), P(w, w′), increment(w, k)); the MTTF engine uses only `increment` | any model of the paper's class plugs in; ν and P exist for the cross-check Eq. (6) | ADR-0003 |
| D-5 | **Every result has an independent second route**: brute-force counting vectors, direct solution of Eq. (6), and `*_by_enumeration` policy code | a bug repeated in both routes is the main risk | ADR-0002, ADR-0008 |
| D-6 | **Precision is handled where it is needed**: Lévy-frailty kernels and increments in `long double` with the non-negative recursion, refusal if the numbers do not form a distribution, cap n ≤ 16 | Eqs. (20) – (21) are alternating sums with cancellation growing like 2ⁿ | ADR-0003; NFR-009 |
| D-7 | **Errors are exceptions of one type** (`ModelError`) carrying the field or value and the broken rule; the CLI maps them to exit status 1 | INT-004 | `errors.hpp` |
| D-8 | **Components are exchangeable** and calibrated by one parameter, the pairwise β = 2 − Ψ(2), Ψ(1) = 1 | the paper's assumption; makes models comparable | ADR-0004 |
| D-9 | **The Python layer only calls the CLI** and never re-implements the mathematics | one implementation to verify | ADR-0005 |
| D-10 | **Volatile output (time, memory) is kept in a separate `timing` section/file** | the rest of every result is byte-reproducible | ADR-0007 |
| D-11 | **No global mutable state** except the Φ-call counter of a `SystemModel` (declared `mutable`); not thread-safe | single-user, single-threaded tool | `system_model.hpp` |
| D-12 | **Zero compiler warnings** with `-Wall -Wextra -Wpedantic`, sanitizer-clean | NFR-004, verification report §2 | `CMakeLists.txt`, CI |

## 4. CSCI architectural design

### 4.1 Concept of execution
One `analyze` run is a single pass:

1. parse arguments (`UsageError` → exit 2); read the JSON file; build the `SystemModel` and `validate()` it (`ModelError` → exit 1);
2. choose the failure model (command line over file over default) and build it, which may refuse (`ModelError`);
3. `sweep()` the levels from w = 0 to w = n; for each state the observer computes m(x) from its counting vector;
4. if `--verify`: recompute everything by the independent routes and compare;
5. if all three cost flags: build the class table once and evaluate corrective, r-out-of-n, every threshold policy and the optimal policy;
6. write the JSON (`timing` last); exit 0, or 3 if verification failed.

The Python layer invokes step 1 – 6 many times as subprocesses; see the sequence diagram [uml/sequence.png](uml/sequence.png).
There are no threads, no signals and no persistent state; all memory is released at exit.

### 4.2 Module structure (component diagram: [uml/component.png](uml/component.png))

```
main.cpp ──► system_model ◄── state
   │              ▲   ▲
   │              │   └──────────── reference   (phi only: never sweep(), sons(), binomial())
   ├──► signature (sweep, level_index.hpp) ──► mttf ──► failure_model
   ├──► policy ───────────────────────────────────────► mttf, failure_model, level_index.hpp
   └──► errors (used by all)
```

| Module | Files | Depends on |
|---|---|---|
| state | `state.hpp/.cpp` | – |
| system model | `system_model.hpp/.cpp` | state, errors, nlohmann/json |
| signature engine (sweep) | `signature.hpp/.cpp`, `level_index.hpp` | system model, state |
| failure model | `failure_model.hpp/.cpp` | errors |
| MTTF engine | `mttf.hpp/.cpp` | signature (types), failure model |
| reference | `reference.hpp/.cpp` | system model, failure model (for Eq. (6)) |
| policy | `policy.hpp/.cpp` | system model, failure model, MTTF, level_index |
| CLI | `main.cpp` | all of the above, nlohmann/json |
| experiment layer | `python/run_experiments.py`, `analyze_results.py`, `plot_results.py` | the CLI only (IF-5) |
| audit tool | `tools/check_traceability.py` | docs and tests |

### 4.3 Interface design
See [interface-design-description.md](interface-design-description.md).

## 5. CSCI detailed design

### 5.1 `state` — representation of a condition
**Purpose.** Encode and enumerate states. `working_count` (popcount), `all_working(n)`, `is_working`, `without(x, i)`,
`sons(x)` (the states one component fewer, lowest failed bit first), `masks_with_working(n, w)` (a level, in increasing
order), `binomial(n, k)` (exact, `uint64_t`).
**Limit.** `kMaxComponents = 24`.

### 5.2 `system_model` — components and Φ
**Purpose.** Hold the components and the structure function; load from JSON; validate.
**Design.** `Φ` is a `std::function<bool(StateMask)>` built by one of `series`, `parallel`, `and_of_or_groups`,
`or_of_and_groups` (the JSON `path_sets`) or `from_function` (tests, arbitrary Boolean functions). `phi(x)` range-checks the
mask and increments a counter. `validate()` evaluates Φ at every state once, then checks 2 ≤ n ≤ 24, Φ(0) = 0, Φ(1) = 1 and monotonicity
(a failed state may not have a working son, which suffices because any chain is a sequence of single steps), naming the first
offending state. It resets the call counter on exit (also on error), and `sweep()` resets it again before starting, so the
sweep's count is exactly 2ⁿ.
**Reference system.** `P1, P2, C1, C2, K1, K2` on bits 0 – 5; groups {P1, P2}, {C1, C2}, {K1, K2}; Φ = 1 for 27 of 64 states.

### 5.3 `signature` — the level sweep (paper Algorithm 1, Lemma 1, Theorem 2)
**Definitions.** For state x with w working components, the counting vector W^x has entries W^x[k], k = 0 … w: the number
of ways to fail k of them while the system still works. W^x[0] = Φ(x).
**Algorithm.**

```
for w = 0 .. n:                                   // levels: all states with w working components
    current = matrix with one row of w+1 integers per state of the level, rows in increasing mask order
    for each state x of the level:
        W[0] = Φ(x)                               // the only call of Φ, once per state
        ranks = son_ranks(x)                      // position of each son in the previous level, O(w)
        for k = 1 .. w:  W[k] = (Σ over sons of previous[son][k-1]) / k      // exact integer division
        observer(x, W)                            // the MTTF engine, the CLI, the tests
        if W[0] == 1 and W[1] == 0: record x as a minimal path set
    previous = current                            // the older level is released
```

**Outputs.** `SweepResult`: minimal path sets, `phi_calls` (= 2ⁿ), `peak_vectors_held` (two levels; 352 716 vectors at
n = 20 instead of 1 048 576), and the measured number of integer additions and divisions, which equal the paper's
n(n+1)2ⁿ⁻² and n·2ⁿ⁻¹ exactly.
**Derived values.** Tail signature S̄_k = W[k] / C(w, k); signature s_k = S̄_{k−1} − S̄_k, k = 1 … w. For the reference system
at the all-working state: (0, 0.2, 0.4, 0.4, 0, 0).
**Rank of a son.** `level_index.hpp` ranks a state within its level by the combinatorial number system,
rank = Σ C(c_m, m) over the working positions c₁ < … < c_w, so no hash map is needed; `son_ranks` obtains all w sons' ranks in
one O(w) sweep with prefix and suffix sums. The header is shared with the policy module.
**Option.** `keep_all_vectors` (tests only) stores every vector.
**Property.** The recurrence is pure counting: it is correct for any Boolean Φ, which the tests exploit; monotonicity matters only for the meaning of "minimal path set" and for the theorems on top.

### 5.4 `failure_model` — how fast components fail
**Interface.** `FailureModel`: `name()`, `exit_rate(w)` = ν(w), `transition_prob(w₁, w₂)` = P(w₁, w₂), `increment(w, k)` =
E[T_{k+1:w}] − E[T_{k:w}].

| Class | Models | Design |
|---|---|---|
| `IndependentExponential(λ)` | [Exp] | ν(w) = wλ; one component fails per event; increment = 1/(λ(w−k)); λ ≤ 0 or NaN rejected |
| `LevyFrailty(label, Ψ, max_n)` | [LSh], [BFR], [LFMO-C], [LFMO-G], [LFMO-IG], [LFMO-S] | ν(w) = Ψ(w) (Eq. (19)); kernel P from Eq. (20) and increments from the non-negative recursion (after Proposition 3), both in `long double` and tabulated for w ≤ `max_n`; `increment_eq21` is the cross-check with the lower summation limit **w − k** (the printed w − k + 1 is a typo); the constructor refuses a kernel whose rows are not a distribution; `kMaxLevyFrailtyComponents = 16` |

**Ψ functions** (all with Ψ(1) = 1, calibrated by β = 2 − Ψ(2)): lethal shock (1−β)x + β; binomial failure rate (1 − (1−β)ˣ)/β;
compound Poisson x/(1 + p(x−1)), p = β/(2−β); gamma process log(1 + x/η)/log(1 + 1/η), η solving Ψ(2) = 2 − β; inverse Gaussian
(√(2x+η²) − η)/(√(2+η²) − η); stable subordinator xᵅ, α = log₂(2−β). The parameter solvers are exposed (`gamma_process_eta`, …).
**Factories.** `make_*` return `std::unique_ptr<FailureModel>`; invalid parameters throw `ModelError`.

### 5.5 `mttf` — Eq. (7)
m(x) = 0 if W[0] = 0; otherwise m(x) = Σ_{k=0}^{w−1} (W[k] / C(w, k)) · increment(w, k). Every term is non-negative, so nothing
cancels. `compute_all_mttf` runs one sweep and fills a vector of 2ⁿ doubles from the observer.
Tested properties: m ≥ 0 and finite; m(x) = 0 where Φ(x) = 0; m monotone in the state (Lemma 4).

### 5.6 `reference` — the independent second opinion
Never calls `sweep()`, `sons()`, `masks_with_working()` or `binomial()`.
- `brute_force_counting_vectors`: for each state, every subset of its working bits is failed in turn and Φ of the result is added to `W[k]` (about 3ⁿ work; n ≤ 16).
- `brute_force_minimal_path_sets`: the definition (operates, and no single removal operates).
- `solve_mttf_direct`: solves the first-step equations (Eq. (6)) over the model's ν and P, state by state in increasing mask order (every proper sub-state has a smaller mask, so it is already solved): m[x] = 0 if Φ(x) = 0, else 1/ν_w + Σ over proper sub-states y of P(w, |y|)/C(w, w−|y|) · m[y]; agrees with Eq. (7) to about 1e-12 for [Exp] and 2e-10 for n = 16 under common cause. This is why `FailureModel` exposes ν and P.

### 5.7 `policy` — when to repair, and at what cost
**Cost model** (paper §V, Eqs. (9) – (12)). `RepairCosts{component, system, setup}`; a policy is a `RepairSet`
(one flag per state: repair here or continue). `PolicyEvaluation` = mean cycle length, mean cycle cost, long-term mean cost
= cycle cost / cycle length.
**Policy families.** corrective (repair only on system failure); r-out-of-n (repair when r components have failed, 1 ≤ r ≤ n);
threshold π_γ (repair when m(x)/m(**1**) < γ); optimal state-based (Dinkelbach iteration, §V-B).
**Design for n ≤ 20.**
1. `build_policy_classes` sweeps once and groups states with equal counting vectors into classes (count, MTTF ratio, and three level sums: time spent, failures, corrective contribution).
2. `evaluate_policy` and the optimal-policy passes use level sums, O(n²2ⁿ) per policy.
3. `search_threshold_policies(table, costs)` gets **every** breakpoint of γ from that table in one pass (Proposition 3, which assumes a monotone MTTF: true for all models here); ties break towards not repairing; operational ratios closer than a relative tolerance count as equal.
4. `evaluate_r_out_of_n_policies(table, costs)` and corrective maintenance read the same table (their "continue" sets are closed under "more components working", all that Proposition 3 needs).
5. `optimal_state_policy` iterates policy evaluation until the cost rate stops falling (finite; an iteration guard protects against rounding loops).

**Independent check.** `*_by_enumeration` functions evaluate policies by direct recursion over states (about 3ⁿ, n ≤ 14,
`kMaxEnumeratedPolicyComponents`), need no monotonicity, and are compared with the fast code in the tests together with closed forms from the signature and the
paper's Fig. 1 costs 97/105, 11/15, 1/10.

### 5.8 `main.cpp` — the `analyze` tool
Parses options (`Options`, `UsageError`), maps `--model` names to model types, merges command line over file over default,
runs the pipeline of §4.1, builds the result JSON (IF-3), reads peak memory from `VmHWM`, and applies the verification
tolerances (MTTF 1e-9 relative, signature sum 1e-12). Exit codes: `kExitOk = 0`, `kExitInvalid = 1`, `kExitUsage = 2`,
`kExitVerificationFailed = 3`. The labelled fault-injection hook is active only with `--verify`.

### 5.9 Python layer
- `run_experiments.py`: refuses to run an experiment without its hypothesis file; calls the CLI; writes the deterministic result and the timing file; generated systems are written as IF-1 files; random systems for Experiment 7 come from `random.Random(seed)`.
- `analyze_results.py`: loads results (refuses those without provenance), builds pandas tables, and evaluates each pre-registered prediction to SUPPORTED or REJECTED with evidence.
- `plot_results.py`: draws the seven figures from the CSV tables only.
- `tools/check_traceability.py`: IF-8.

### 5.10 Data
There is no database. Data structures that matter: the state mask (`uint32_t`), a counting vector (`std::vector<uint64_t>`),
a level (flat matrix of rows), the MTTF vector (2ⁿ doubles), the Lévy-frailty tables (`(max_n+1)²` `long double` kernel
entries and `(max_n+1)²` doubles of increments), the class table of the policy module. Persistent data are the JSON files of IF-1, IF-3 and IF-7.

### 5.11 Resource use

| n | Operation | Time | Memory |
|---|---|---|---|
| 6 (satellite) | analysis | ≈ 0.08 ms | negligible |
| 20 | analysis | ≈ 2.2 s | ≈ 35 MB |
| 24 | analysis | ≈ 48 s | ≈ 534 MB |
| 16 / 18 / 20 | full policy comparison | ≈ 0.6 / 2.4 / 8 s | – |

One laptop, one core, WSL2; orders of magnitude only (D2).

## 6. Requirements traceability

| Requirement | Realised in | Verified by (test-plan ids) |
|---|---|---|
| SYS-001, SYS-003 – SYS-005 | §5.1, §5.2 | TC-E-01 – TC-E-06, TC-E-10 – TC-E-13 |
| SYS-002 | §5.3 (path-set detection) | `Reference`/`Signature` tests, ORC path-set counts |
| ALG-001 – ALG-004 | §5.1, §5.3 | sweep = reference on every state; Φ called 2ⁿ times |
| ALG-005 | §5.6 | structural independence by inspection; used by VER-001 tests |
| ALG-006 | §5.3 (`integer_additions`, `integer_divisions`) | TC-C-22; Experiment 4 |
| REL-001, REL-003 | §5.4 `IndependentExponential`, §5.5 | oracles ORC-1 – ORC-6 |
| REL-002, REL-004 | §5.4 `LevyFrailty`, Ψ functions | TC-D-*, TC-E-08 |
| REL-005 | §5.6 `solve_mttf_direct` | Eq. (6) = Eq. (7) on all states |
| VER-001 – VER-004 | §5.6, §5.8 `--verify`, tests | test plan A – F, I; TC-H-06 |
| EXP-001 – EXP-003 | §5.9 | TC-G-01 – TC-G-09 |
| EXP-004 | §5.7 | TC-J-01 – TC-J-14, TC-G-11, TC-G-13 |
| INT-001 – INT-004 | §5.8, IF-1 – IF-5 | TC-H-05, TC-H-08, TC-H-09, TC-G-10 |
| NFR-009 | D-6, §5.4 | TC-F-07 and precision tests |

The generated [traceability-matrix.md](traceability-matrix.md) is the authority for requirement → test-case → test-code links;
this table maps requirements to design elements.

## 7. Notes

- **Not designed:** non-identical components, repair inside the core model, failure models outside the paper's Assumptions (A) – (C), parallel execution, operating systems other than Linux (`main.cpp` reads `/proc/self/status` and includes `<sys/resource.h>`).
- **Known design debts:** `SystemModel` is not thread-safe because of the mutable call counter; `kMaxComponents = 24` is a mask-size decision, but the Pascal table in `level_index.hpp` is sized for 31; two independent size caps (16 and 20) are enforced in different layers (`LevyFrailty`, `policy`) rather than in one place.
- **Open question carried from the requirements** (open item 5): a third independent implementation (the authors' repository) has not been consulted.
