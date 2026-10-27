# Deviation Log and Limitations

| | |
|---|---|
| **Date** | 2026-10-22 |
| **Purpose** | Requirement DOC-001: every place where the implementation, the experiments or the documents depart from the paper, the project guide or the plan, and every limitation that a reader must know. |

Status of each entry: **Deviation** (the project does something different from its source), **Correction** (the
source is wrong and the project fixes it), **Decision** (a choice the sources leave open), or **Limitation**.

## A. Relative to the research paper (Lagos & Ávila, arXiv:2610.22181)

| # | Kind | Entry | Why / effect | Where |
|---|---|---|---|---|
| A1 | Correction | **Eq. (21) is used with lower summation limit `i = w−k`**, not the printed `w−k+1` | the printed limit gives 0 for k = 0 and wrong values for k ≥ 1 even for exponential lifetimes; the corrected form matches [Exp], the paper's own recursion and Eq. (6) | validation report §6; `failure_model.cpp`; test `PrintedEquation21LowerLimitIsATypo` |
| A2 | Decision | **Φ(0) is evaluated**, so Algorithm 1 performs exactly 2ⁿ evaluations | the paper's pseudo-code sets the all-failed state's result without calling Φ, while Theorem 2.1 says 2ⁿ evaluations | `signature.hpp`; test `SweepEvaluatesPhiExactlyOncePerState` |
| A3 | Deviation | **Increments computed by the non-negative recursion** (after Proposition 3), with Eq. (21) as a cross-check, in `long double` | the paper computes Ψ-derived quantities in 512-bit arithmetic; ours is cheaper and is refused beyond its accuracy | `failure_model.cpp`; validation report §4 |
| A4 | Limitation | **Lévy-frailty models are limited to n ≤ 16** | measured: relative error ≈ 2e-10 at n = 16; at n = 17 some β are already refused. The paper reaches n = 30 with 512-bit arithmetic | `kMaxLevyFrailtyComponents`; NFR-009 |
| A5 | Limitation | **Only [Exp] and the six common-cause models of the paper's Table I are implemented**, out of its 31 failure models: [LSh], [BFR], [LFMO-C], [LFMO-G], [LFMO-IG], [LFMO-S] (all calibrated to β; Table I parameters reproduced). The paper's other models (non-exchangeable, other lifetime laws, other Ψ) are not | scope; the Table I models share one code path (a Ψ function) | requirements open item 2; `failure_model.cpp` |
| A6 | Limitation | **Exchangeable (identical) components**: the satellite's three differently-purposed pairs share one lifetime law | an assumption of the paper; non-identical components are listed there as future work | requirements A-6 |
| A7 | Deviation | **Repair policies (paper §V)**: policy costs are evaluated with the paper's level sums (O(n²2ⁿ) per policy), every threshold policy comes from one pass over classes of equal counting vectors (Proposition 3, which **assumes a monotone MTTF**: true for every model here, Lemma 4), and the optimal state-based policy is Dinkelbach's method (§V-B), all for n ≤ 20. Differences from the paper: a state-by-state hash map of counting vectors is used for the classes; ties are broken towards not repairing; the r-out-of-n policies and corrective maintenance come from the same class table (their continue sets are closed under "more components working", all that Proposition 3 needs), so a full comparison is one sweep plus a few level-sum passes of the optimal policy: about 0.6 s at n = 16, 2.4 s at 18 and 8 s at 20. The earlier enumeration code (about 3ⁿ, n ≤ 14) is **kept** as `*_by_enumeration` functions, which do not need monotonicity, and serves as the independent check; Proposition 3 and closed forms from the signature are further independent checks in the tests. | closes the earlier simplification; two implementations of every policy computation | `policy.hpp`; experiment report §7 |
| A8 | Limitation | The paper's **empirical results are not reproduced** (22 networks, 31 failure models, Julia, up to 30 components, cluster) | not a goal; only the paper's Fig. 1 example and its complexity claims are reproduced | experiment report §1 |
| A9 | Decision | **Calibration**: every model has Ψ(1) = 1 and is parameterised by the pairwise beta factor β = 2 − Ψ(2), as in the paper §VII-B | makes models comparable | `failure_model.hpp` |
| A10 | Decision | **Binomial failure rate** is calibrated driftless: Ψ(x) = (1 − (1−β)ˣ)/β | the paper's Table I gives the same form for this model | `make_binomial_failure_rate` |

## B. Relative to the project guide (`space_system_reliability_analyzer_project_guide.docx`)

| # | Kind | Entry | Why / effect | Where |
|---|---|---|---|---|
| B1 | Correction | The guide says the algorithm "reduces the number of structure-function evaluations". **It does not**: Φ is still evaluated 2ⁿ times, once per state; what is gained is the signature and MTTF at *every* state for that count | the paper (Theorem 2) | validation report G1; experiment report |
| B2 | Decision | **Time-stamped definition**: Tₖ:w are order statistics (ties possible), not "the k-th distinct failure" | simultaneous failures put several components at one instant | requirements §1.4 |
| B3 | Decision | Minimal path sets are produced **by the sweep** (W₀ = 1 ∧ W₁ = 0) and checked against the definition; they are tested, not kept as a separate extension | free by-product (paper §IV) | SYS-002 |
| B4 | Decision | Experiment 2 compares the benefit of redundancy **relative to a series baseline under the same model**, with the paper's calibration | absolute MTTF alone misleads: it rises with β | experiment report §4 |
| B5 | Deviation | The guide's **"state-space engine" module is folded into the signature engine** (the level sweep) | the paper's algorithm interleaves enumeration, counting and MTTF | component diagram |
| B6 | Deviation | **Repository layout** differs from guide §5: added `reference`, `errors`, `main`, `tools/`, `experiments/hypotheses/`; documents live in a sibling `docs/` folder; the CI file is at the repository root | needs of the plan; GitHub reads workflows only from the root | README files |
| B7 | Decision | Open decisions closed: GoogleTest 1.14, nlohmann/json 3.11.3, maximum system size 24, tolerance 1e-9 | recorded on Day 2 and in the verification report | requirements open items |

## C. Relative to the plan and the documents

| # | Kind | Entry | Where |
|---|---|---|---|
| C1 | Deviation | The `docs/` folder was **rebuilt on 2026-10-20** after it disappeared from disk; the UML sources and PNGs were restored from render copies, the five documents were reconstructed from earlier work | `docs/README.md` |
| C2 | Deviation | Work was **pulled forward**: minimal-path-set recording and random-system tests (Day 5 → Day 4); [BFR] (optional) was added on Day 7 | plan progress table |
| C3 | Decision | Hypotheses live in `project/experiments/hypotheses/` (tracked), results in `project/results/` (not committed, regenerated by the scripts) | project README |
| C4 | Decision | Figures use a validated **light** palette only; a dark variant would need its own validated steps | experiment report §11 |

## D. Measurement and process notes

| # | Kind | Entry |
|---|---|---|
| D1 | Correction | Peak memory is read from `VmHWM` (`/proc/self/status`) because `ru_maxrss` is inherited across fork and exec and reported the Python parent's memory. The early runs' constant 19 MB was an artefact. |
| D2 | Limitation | Run times are medians of 1 to 5 repeats on one laptop under WSL2 and are orders of magnitude and growth rates, not benchmarks. |
| D3 | Limitation | In Experiment 4 verification stops at n = 14 (the brute-force reference does about 3ⁿ work); the analysis stops at n = 20. |
| D4 | Process | **Pre-registered predictions were rejected** — two in Experiment 2, one in Experiment 4 and one in Experiment 6 in every run, plus one timing-sensitive Experiment 4 criterion that was rejected in the first run and supported on a repeat — and are reported as found; the hypothesis files were not edited afterwards. |
| D5 | Limitation | CI runs on GitHub Actions on Ubuntu 24.04 only (pinned, so that the compiler versions quoted in the verification report stay valid) (first green run 2026-10-22, run 39864780885). No other operating system has been tried. The workflow uses `actions/checkout@v7` and `actions/setup-python@v7` (Node.js 24 runtimes), which replaced v4 and v5 after the first runs warned about the deprecated Node.js 20 runtime. |
| D6 | Process | A fault-injection environment variable (`ANALYZE_FAULT_INJECTION`) exists in the CLI solely to test the verification itself; it acts only together with `--verify`. |

## E. Scope statements (NFR-008)

* The satellite-support system is **fictional**. This is an educational portfolio project, not a certified safety
  analysis, not flight-qualified, and not a statement about any real spacecraft.
* Nothing here claims the project uses, or is used by, any employer's tools.
* Model fidelity is limited to what §A6 and the experiment report §11 list: identical components, perfect redundancy in a
  pair, no switch-over failure, shared resources, thermal coupling or software common-mode faults beyond what the two
  common-cause models express.
