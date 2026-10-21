# Validation Report — Documents vs. the Research Paper

| | |
|---|---|
| **Date** | 2026-10-20 |
| **Paper** | G. Lagos and S. Ávila, *Per-State Signatures and Mean Time-to-Failure: An Algorithm for Systems With Simultaneous Failures*, arXiv:2610.22181v1 [cs.DS], 15 Oct 2026 (`paper3.pdf`, 15 pages) |
| **Reviewed** | `space_system_reliability_analyzer_project_guide.docx`, [requirements-spec.md](requirements-spec.md), [test-plan.md](test-plan.md), [user-stories.md](user-stories.md), [uml/](uml/) |
| **Method** | Read the full paper text. Re-computed every numeric oracle in exact rational arithmetic with an independent script (brute-force subset counting, the paper's Lemma 1 recursion, the paper's Eq. (6) linear system, and closed-form integrals). Reproduced the paper's own worked example (Fig. 1). |

**Result:** the core mathematics in the guide and my first-draft documents is **consistent with the paper**. The review also found **one incorrect test expectation, one outdated diagram, and several missing paper-specific requirements**. All have been corrected in the documents (v1.1). A later check of the typeset PDF (2026-10-21, §6) found **a typo in the paper's own Eq. (21)**. The remaining caveats are listed in §5.

## 1. What was confirmed

| # | Claim in the documents | Paper reference | Status |
|---|---|---|---|
| C1 | State = bit vector, x_i = 1 means working, \|x\| = number working, 2ⁿ states | §I-A | ✅ identical |
| C2 | Structure function is monotone | §I-A | ✅ (paper also requires non-triviality, see E4) |
| C3 | Counting vector W^x_k = number of sub-states of x that still work after shutting down exactly k working components; W^x_0 = Φ(x) | §IV | ✅ |
| C4 | Recursion W^x_k = (1/k) Σ_{x′∈Sons(x)} W^{x′}_{k−1} | Lemma 1, Eq. (8) | ✅ matches guide §4.2 exactly. Re-checked: recursion equals brute force on all 64 reference states. |
| C5 | Tail signature S̄_k = W_k / C(n,k), signature s_k = S̄_{k−1} − S̄_k, entries ≥ 0, sum 1 | Eqs. (1), (2) | ✅ |
| C6 | Per-state MTTF m(x) = Σ_{k=0}^{\|x\|−1} [W^x_k / C(\|x\|,k)] · (E T_{k+1:\|x\|} − E T_{k:\|x\|}), T_{0:n} = 0 | Eqs. (5), (7) | ✅ guide §4.3 matches. Summation range is k = 0 … \|x\|−1. Also valid when Φ(x) = 0 (then m = 0, W = 0). |
| C7 | The failure-time increments depend on the failure model and are not interchangeable | Assumption (C) | ✅ |
| C8 | i.i.d. exponential lifetimes fit the paper's assumptions (model [Exp]); increment = 1/(μ(w−k)) | §VII-A.1, §VI | ✅ — the guide's "only after confirming it fits" is now confirmed. |
| C9 | Minimal path sets come out of the same sweep | §IV (corollary) | ✅ — criterion: x is a minimal path set iff W^x_0 = 1 and W^x_1 = 0. |
| C10 | State space is 2ⁿ; this limits the method to a few dozen components | §VIII | ✅ |
| C11 | Repair policies are optional and need cost models, because MTTF alone is not a maintenance-cost model | §V | ✅ — paper uses renewal-reward long-term mean cost. |
| C12 | Experiment 3 premise: equal failure counts can differ in reliability | §I, intro | ✅ — this is the paper's motivating example. |

### Independently verified numbers

All values below were recomputed with exact fractions, assuming i.i.d. exponential lifetimes of rate λ = 1 (MTTF scales as 1/λ).

| Case | W vector | MTTF·λ | Verified by |
|---|---|---|---|
| Reference system, all working (mask 63) | [1,6,12,8,0,0,0] | 7/10 | recursion = brute force; Eq. (6) solved directly; integral 8/3 − 3 + 6/5 − 1/6 |
| P1 failed (mask 62) | [1,4,4,0,0,0] | 8/15 | same |
| P1 and C1 failed (mask 58) | [1,2,0,0,0] | 5/12 | same |
| Reference system: operational states / minimal path sets | 27 / 8 (each of size 3) | — | enumeration; W⁰=1 and W¹=0 criterion |
| Paper Fig. 1 system, Φ = max{x₃, min{x₁,x₂}} | W¹ = (1,3,1,0), s = (0, 2/3, 1/3) | m(1) = 7/6 | **reproduces the paper's own figure**; per-state m(x)/m(1) = 100 %, 86 %, 86 %, 43 %, 0 % as printed |
| Fig. 1 policy costs (c_comp = 1/30, c_sys = 1, c_set = 0) | — | LTMC = 1/10, 11/15, 97/105 | **reproduces the paper** by implementing Eqs. (9)–(12) |
| Reference system under the lethal-shock model [LSh], β = 0.5 | — | 32/35 | closed-form integral; Eq. (6) solved directly; Eq. (7) with increments from the reconstructed Eq. (21) |

## 2. Errors found (and fixed)

| # | Document | Problem | Correction |
|---|---|---|---|
| E1 | test-plan TC-D-04 (my draft) | Expected that raising the common-cause parameter never increases MTTF. **This is false under the paper's calibration.** The paper normalises every model to unit mean component lifetime (Ψ(1) = 1). Raising the beta factor then *increases* the reference system's absolute MTTF: lethal-shock model, β = 0 → 0.700, 0.05 → 0.718, 0.20 → 0.776, 0.50 → 0.914 (exact: 32/35). Failures become bunched, which helps a series-of-pairs system. | Replaced by a correct expectation: the **benefit of redundancy relative to a series baseline under the same model** falls with β (ratio 4.20 → 3.88 → 3.20 for β = 0, 0.2, 0.5). Experiment 2 and US-17 now require this normalised comparison. |
| E2 | uml/sequence.puml | Showed "enumerate all 64 states, then compute vectors". The paper's Algorithm 1 is a **level-by-level backward sweep** (from the all-failed state up to all-working) that evaluates Φ once per state while computing W and m(x), and discards the previous level. | Diagram redrawn. Component diagram and use cases updated accordingly. |
| E3 | requirements ALG-004 / TC-C-09 | Said "≤ 2ⁿ vector computations". The paper's Theorem 2 is stronger and testable: Φ is evaluated **exactly 2ⁿ times, once per state**, and only two consecutive levels are kept. | Requirement and test tightened. |
| E4 | requirements SYS-001, test E | Did not require the paper's preconditions: n > 1 and non-trivial Φ (Φ(**0**) = 0 and Φ(**1**) = 1). It also did not say that irrelevant components must be **accepted** (the paper covers semi-coherent systems). | Added to SYS-001, A-1, and tests TC-E-10 to TC-E-12. |
| E5 | requirements §1.4 | Defined T_{k:n} as "time of the k-th distinct component failure". Under simultaneous failures several components fail at one instant. T_{k:n} are the **order statistics** of the component failure times (ties allowed), with T_{0:n} = 0. | Definition corrected. |
| E6 | requirements open item 2 (common-cause model undecided) | Now decidable from the paper. | Resolved: use the Lévy-frailty Marshall-Olkin (LFMO) family; the minimum is the lethal-shock model [LSh]. See REL-002, REL-004. |

## 3. Gaps found in the guide (the .docx), not just my documents

| # | Guide statement | Paper says | Impact |
|---|---|---|---|
| G1 | §11: "Even when the algorithm reduces the number of structure-function evaluations…" | It does **not** reduce them. Φ is still evaluated 2ⁿ times, the same as for the signature of the whole system alone. The contribution is getting the signature and MTTF at **every** state for that same count (plus a polynomial overhead). | Do not claim fewer Φ evaluations in the report. Experiment 4 should compare against *per-state brute force*, and against the 2ⁿ-enumeration baseline. |
| G2 | §2/§4: simultaneous failures described only as "a mathematically compatible model" | The model class is precise: Assumptions (A) exchangeable lifetimes, (B) survivor count is a continuous-time Markov chain, (C) computable E T_{k:\|x\|}. Together they force a **pure-death chain**. They exclude Markovian arrival processes, semi-Markov models and load-sharing models with non-monotone MTTF. | REL-002 now states these assumptions. A "common-cause" model that is not exchangeable (e.g. only P1 and P2 share a shock) is **out of scope** for the algorithm as published. |
| G3 | §4.3: no method given to obtain E T_{k:w} for simultaneous failures | Eq. (21) gives them from the Laplace exponent Ψ(1…n); Eqs. (19)–(20) give the transition kernel. The guide did not mention that Eq. (20) is an **alternating sum** and numerically delicate. | New numerical requirement NFR-009 and test TC-F-07 (see §4). |
| G4 | §2 table: minimal path sets listed as a P1 "extension" | They are a **free by-product**: flag states with W⁰ = 1 and W¹ = 0 during the sweep (no extra Φ calls). | Reasonable to keep at P1, but the implementation cost is a few lines. |
| G5 | §8 Experiment 2: "Compare models only when their assumptions and parameterization are clearly stated" | The paper's way: calibrate every model to unit mean component lifetime and a common **pairwise beta factor** β = 2 − Ψ(2); β ∈ {1, 5, 10, 20, 50} %. | Experiment 2 now specifies this calibration. |
| G6 | §6: REL-001 acceptance "analytical/reference cases where available" | Paper offers a built-in cross-check: solve the MTTF linear system Eq. (6) directly and compare with Eq. (7). Checked exactly here for all 64 states under both [Exp] and [LSh]. | New requirement REL-005. |
| G7 | §8 optional repair policies: "threshold-based vs simple failure-count policy" | The paper defines π_γ (repair when m(x)/m(1) < γ) and the r-out-of-n policy, costs Eqs. (9)–(12), and warns the cost curve is **not unimodal**, so all breakpoints must be searched. | EXP-004 and US-20 updated with the paper's definitions and the Fig. 1 oracle. |

## 4. Numerical-precision finding (new)

Eq. (21) is an alternating sum. Evaluating it in naive `double` loses accuracy as n grows. Measured on the lethal-shock model with θ = 0.2:

| n | worst relative error of E T_{k+1:n} − E T_{k:n} in double |
|---|---|
| 6 (this project's system) | 7.6 × 10⁻¹⁵ |
| 12 | 3.3 × 10⁻¹² |
| 20 | 8.4 × 10⁻⁹ — exceeds a 1e-9 tolerance |
| 30 | 6.4 × 10⁻⁴ |

The paper computes Ψ-derived quantities and P_{w,w′} in 512-bit arithmetic and then uses non-negative recursions in double. For the six-component project, `double` is adequate; for the scaling experiment (Exp. 4), **the supported maximum n for simultaneous-failure models must either be ≤ 12 or use extended precision**. The [Exp] model is unaffected (increment = 1/(w−k)).

### Implemented precision strategy and measured limits (2026-10-21)

The Lévy-frailty models compute Eq. (20) and the increments in `long double` (64-bit mantissa) and refuse any model whose kernel is not a probability distribution. Measured for the parallel system under [LSh], β = 0.2, against exact rational arithmetic:

| n | relative error of the MTTF | kernel error vs closed form |
|---|---|---|
| 6 | 5 × 10⁻¹⁵ | 4 × 10⁻¹⁵ |
| 12 | 3 × 10⁻¹² | 1 × 10⁻¹² |
| 16 | 2 × 10⁻¹⁰ | 6 × 10⁻¹¹ |
| 17 | refused for some β (e.g. [BFR] β ≤ 0.01, [LSh] β = 0.9), accepted for others | — |
| ≥ 18 | refused (negative kernel entries) | — |

Because behaviour at n = 17 depends on the parameter, the code uses a **hard cap of n = 16** for all Lévy-frailty models. Both increment routes (recursion and Eq. (21)) agree with exact values to about 3 × 10⁻¹⁴ in absolute terms; the *relative* error is larger (up to about 5 × 10⁻¹⁰) only for the tiniest late-stage increments.

## 5. Remaining caveats

- ~~The PDF text extraction garbles Eq. (21).~~ **Resolved on 2026-10-21 — see §6.** The typeset equation was read directly from the rendered page. It has a typo in the lower summation limit; the corrected form is used in the code.
- I verified [Exp] and [LSh] only. [BFR] and the other LFMO models (Table I) are not independently checked.
- The paper's Appendix proofs were read but not re-proved. The empirical results (Section VII, Table II/III) are for networks up to 30 edges in Julia and are not reproduction targets of this project.
- The paper's repository (`github.com/guidolagos/Subsystems`) is referenced for code and computed signatures; I did not access it. It could serve as a third independent check.
- Paper assumptions limit the tool to **identical, exchangeable** components. Non-identical components are listed by the authors as future work, so the satellite's three differently-purposed pairs are modelled as if all six units had the same lifetime law — a modelling simplification the report must state.
- The paper's Algorithm 1 sets the all-failed state's result without calling Φ, while Theorem 2.1 says exactly 2ⁿ evaluations. The documents count Φ(**0**) as well, giving exactly 2ⁿ calls; confirm this reading when implementing the sweep.

## 6. Typeset-PDF check of Eqs. (19)–(22) (2026-10-21)

Page 8 of `paper3.pdf` was rendered to an image and the equations were read from it directly (the text extraction had garbled Eq. (21)).

| Equation | As typeset | Status |
|---|---|---|
| (19) | ν_w = Ψ(w), for 0 < w ≤ n | ✅ matches |
| (20) | P_{w₁,w₂} · 1/C(w₁, w₁−w₂) = Σ_{w=w₂}^{w₁} (−1)^{w−w₂+1} C(w₁−w₂, w₁−w) Ψ(w)/Ψ(w₁), for 0 ≤ w₂ < w₁ ≤ n | ✅ matches; rows of P sum to 1 (checked exactly) |
| [Exp] | P_{w,w′}/C(w,w−w′) = 1{w′=w−1}/w, ν_w = wλ, increment = 1/ν_{w−k} | ✅ matches |
| [LSh] | Ψ(x) = λ_ind·x + ω for x > 0 | ✅ matches the model used (λ_ind = 1−β, ω = β) |
| (21) | E T_{k+1:w} − E T_{k:w} = Σ_{**i=w−k+1**}^{w} C(w,k) C(k,w−i) (−1)^{i−w+k} / Ψ(i) | ❌ **lower limit is wrong as printed** |

**Why the printed Eq. (21) is wrong.** With the printed lower limit `i = w−k+1`:

- for k = 0 the sum is empty, so the increment is 0 — but E T_{1:w} = 1/Ψ(w) > 0;
- for the independent-exponential model (true increment 1/(w−k)) it returns 0, −1, −2 for w = 3, k = 0, 1, 2, and 0, −1, −5/2, −3 for w = 4.

With the lower limit `i = w−k` it returns exactly 1/(w−k) for [Exp], and it equals the paper's own non-negative recursion (f^{(w)}_{w−k}/ν_{w−k}, stated after Proposition 3, with P from Eq. (20)) for the lethal-shock model, for every w ≤ 8, in exact arithmetic. It also reproduces Eq. (6) solved directly on all 64 satellite states. So the correct form is

  δ_{w,k} = Σ_{i=w−k}^{w} C(w,k) · C(k, w−i) · (−1)^{i−w+k} / Ψ(i).

**Consequences.** The code (`LevyFrailty::increment_eq21`) uses `w−k`; the non-negative recursion remains the primary route and Eq. (21) is its cross-check. The authors should be told about the typo if this project is discussed with them. The other equations used by this project (1), (2), (5)–(8) were already confirmed numerically through the Fig. 1 reproduction; (22) was confirmed by the lethal-shock calibration (β = 2 − Ψ(2) = θ).
