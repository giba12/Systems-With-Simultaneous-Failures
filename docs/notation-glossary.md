# Notation Glossary

One page that maps the paper's symbols to plain language, to the numbers in this project, and to the code.
Paper: Lagos & Ávila, arXiv:2610.22181 (`paper3.pdf`). Equation numbers are the paper's.

## 1. States and the structure function (paper §I-A)

| Symbol | Meaning | Example (satellite) | In the code |
|---|---|---|---|
| n | number of components | 6 | `SystemModel::n()` |
| x ∈ {0,1}ⁿ | a state; xᵢ = 1 means component i works, 0 means failed | x = 62 = `0b111110`: P1 failed, the rest work | `StateMask` (bit i = 1 ⇔ works) |
| \|x\| | number of working components in x | \|62\| = 5 | `working_count(x)` |
| **0**, **1** | all-failed state, all-working state | 0 and 63 | `0`, `all_working(n)` |
| x′ ≤ x | x′ is a sub-state of x (fails everything x fails, and possibly more) | 58 ≤ 62 | `(y & x) == y` |
| Φ(x) | structure function: 1 if the system works in state x | Φ(62) = 1, Φ(0b111100) = 0 | `SystemModel::phi(x)` |
| monotone | x′ ≤ x ⇒ Φ(x′) ≤ Φ(x): breaking parts never repairs the system | — | checked in `validate()` |
| non-trivial | Φ(**0**) = 0 and Φ(**1**) = 1; also n > 1 | — | checked in `validate()` |
| Sons(x) | the \|x\| states obtained by failing exactly one working component of x | Sons(63) = {62, 61, 59, 55, 47, 31} | `sons(x)` |

## 2. Counting vectors and signatures (paper §I-A, §IV)

*k always counts **failures** (components switched off), starting from 0.*

| Symbol | Meaning | Example | In the code |
|---|---|---|---|
| Wˣₖ | number of sub-states of x with exactly k more components failed in which the system still works (k = 0 … \|x\|) | W⁶³ = [1, 6, 12, 8, 0, 0, 0] | `CountingVector` (length \|x\|+1) |
| Wˣ₀ = Φ(x) | zero extra failures: the state itself | — | `W[0]` |
| Lemma 1, Eq. (8) | Wˣₖ = (1/k) Σ over sons x′ of Wˣ′ₖ₋₁ (each case is seen by exactly k sons) | W⁶³₂ = (1/2)·(6 sons × 4) = 12, since every son has W₁ = 4 | `sweep()` (Day 4) |
| S̄ˣₖ = Wˣₖ / C(\|x\|,k) | tail signature: fraction of k-failure cases that still work | S̄⁶³ = (1, 1, 0.8, 0.4, 0, 0, 0) | `tail_signature(W)` |
| sˣₖ = S̄ˣₖ₋₁ − S̄ˣₖ, k = 1 … \|x\| | signature: probability the system fails exactly at the k-th component failure; ≥ 0, sums to 1 | s⁶³ = (0, 0.2, 0.4, 0.4, 0, 0) | `signature(W)` |
| Minimal path set | smallest set of components that keeps the system up; state x is one iff Wˣ₀ = 1 and Wˣ₁ = 0 | 8 sets, one component from each pair | `SweepResult::minimal_path_sets` |

## 3. Time and MTTF (paper §I-A, §III, §IV)

| Symbol | Meaning | In the code |
|---|---|---|
| T₁ … Tₙ | component failure times | — |
| T_{k:w} | k-th **order statistic** of the failure times of w working components (the time of the k-th smallest). Under simultaneous failures several components can share the same time, so ties are possible. T_{0:w} = 0 | — |
| E T_{k:w} | expected value; depends only on w (components are exchangeable), not on which ones work | — |
| δ_{w,k} = E T_{k+1:w} − E T_{k:w} | expected waiting time between the k-th and (k+1)-th failure, k = 0 … w−1; always ≥ 0 | `FailureModel::increment(w, k)` |
| m(x) | MTTF from state x. m(x) = 0 if Φ(x) = 0. Eq. (7): m(x) = Σₖ₌₀^{\|x\|−1} [Wˣₖ / C(\|x\|,k)] · δ_{\|x\|,k} | `compute_all_mttf()` (Day 6) |
| Eq. (6) | m(x) = 1/ν_{\|x\|} + Σ_{x′ ≤ x} P_{\|x\|,\|x′\|} / C(\|x\|, \|x\|−\|x′\|) · m(x′): a second, independent route to m(x) | `solve_mttf_direct()` (Day 6) |

## 4. The failure model (paper §III, §VII-A)

| Symbol | Meaning | In the code |
|---|---|---|
| Assumption (A) | components are exchangeable (identical in law) | — |
| Assumption (B) | N(t) = number of working components is a continuous-time Markov chain; with (A) it only ever goes down | — |
| Assumption (C) | E T_{k:w} can be computed | — |
| ν_w | rate of the next failure **event** when w components work | `FailureModel::exit_rate(w)` |
| P_{w₁,w₂} | probability that the next event leaves w₂ working (w₂ < w₁). A jump of two or more is a simultaneous failure. Which components fail is uniform: each specific set has probability P / C(w₁, w₁−w₂) | `FailureModel::transition_prob(w1, w2)` |
| [Exp] | independent exponential(λ): ν_w = wλ, P_{w,w−1} = 1, δ_{w,k} = 1/(λ(w−k)) | `IndependentExponential` |
| Ψ(x) | Laplace exponent of the Lévy subordinator (Lévy-frailty Marshall-Olkin family, "LFMO"); Ψ(0) = 0, concave, non-decreasing | — |
| Eq. (19) | ν_w = Ψ(w) | |
| Eq. (20) | P_{w₁,w₂} / C(w₁, w₁−w₂) = Σ_{w=w₂}^{w₁} (−1)^{w−w₂+1} C(w₁−w₂, w₁−w) Ψ(w)/Ψ(w₁) — an **alternating sum** (needs high precision for large n) | |
| Eq. (21) | δ_{w,k} = Σ_{i=w−k}^{w} C(w,k) C(k, w−i) (−1)^{i−w+k} / Ψ(i). **The paper prints the lower limit as w−k+1, which is a typo** (see §6) | `LevyFrailty::increment_eq21` |
| Ψ(1) = 1 | calibration: every component has unit mean lifetime | |
| β = 2 − Ψ(2) | pairwise beta factor, Eq. (22): share of a component's failure rate due to events that also fail a given other component; β = 0 is [Exp] | |
| [LSh] | lethal-shock model: Ψ(x) = λ_ind·x + ω for x > 0. Calibrated: λ_ind = 1−β, ω = β | `make_lethal_shock(beta)` |
| [BFR] | binomial failure rate: Ψ(x) = λ_ind·x + μ(1 − (1−p)ˣ) | — |

## 5. Repair policies (paper §V — optional, P2)

| Symbol | Meaning |
|---|---|
| π_γ | repair all failed components as soon as m(x)/m(**1**) < γ (γ ∈ (0,1]) |
| r-out-of-n policy | repair as soon as r components have failed (or the system has) |
| c_comp, c_sys, c_set | cost per replaced component, cost of restoring a failed system, fixed cost of any repair visit |
| C(x) = c_set + (n−\|x\|)c_comp + 1{Φ(x)=0}c_sys | cost of repairing at state x, Eq. (10) |
| LTMC | long-term mean cost = expected cost per cycle ÷ expected cycle length, Eq. (12) |

## 6. Pitfalls we have already met

1. **Eq. (21), lower limit.** The typeset paper has `i = w−k+1`. That gives 0 for k = 0 and negative values for larger k, even for plain exponential lifetimes (true value 1/(w−k)). The correct lower limit is `i = w−k`; verified against [Exp], against the recursion f/ν, and against Eq. (6) (see [paper-validation-report.md](paper-validation-report.md) §6). Code and tests use `w−k`.
2. **Direction of the indices.** k counts failures. W has \|x\|+1 entries (k = 0 … \|x\|) but the MTTF sum stops at \|x\|−1 and uses δ_{\|x\|,k}.
3. **Order statistics, not "the k-th failure event".** With simultaneous failures several components fail at the same instant.
4. **A state with Φ(x) = 0 has the all-zero vector and MTTF 0.** `signature()` of such a state is undefined (we return an empty list).
5. **Absolute MTTF vs. benefit of redundancy.** With Ψ(1) = 1 the absolute MTTF *rises* with β for the satellite; the benefit relative to a series system falls (4.20 → 3.88 → 3.20 at β = 0, 0.2, 0.5).
6. **Exactly 2ⁿ evaluations of Φ.** The paper's Algorithm 1 sets W⁰ = [0] without calling Φ, while Theorem 2.1 says 2ⁿ evaluations. The code evaluates Φ(**0**) too, so the count is exactly 2ⁿ.
7. **Exchangeable means identical.** The three differently-purposed pairs are modelled with the same lifetime law.

## 7. Worked example — satellite state 62 (P1 failed)

- x = 62, \|x\| = 5, working: P2, C1, C2, K1, K2.
- Counting vector: W = [1, 4, 4, 0, 0, 0] (P2 cannot be removed: its partner is gone; one more failure leaves 4 ways; two more leave 4 ways; three more never work).
- Tail signature: (1, 0.8, 0.4, 0, 0, 0); signature: (0.2, 0.4, 0.4, 0, 0).
- MTTF with [Exp], λ = 1: 1/5 + (4/5)/4 + (4/10)/3 = 8/15 ≈ 0.533 (compare 0.7 for the fully working satellite).
