# Experiment 5 — Repair policies

**Written before the first run (2026-10-22), and before any experiment code for it existed.**

## Question
When is it worth repairing a partially failed system, and does the paper's MTTF-threshold policy beat the simple
"repair after r failures" rule?

## Design
Costs follow the paper (§VII-D): c_sys = 1 (the unit), c_comp = ρ/n, c_set = σ, with
ρ ∈ {0.1, 0.2, 0.5, 2/3, 1, 4/3, 2, 4, 10} and σ ∈ {0, 0.05, 0.1, 0.2, 0.5}: 45 cost pairs.
Policies compared by long-term mean cost: corrective maintenance (repair only after the system fails), every
r-out-of-n policy (r = 1..n), and **every** MTTF-threshold policy (one per distinct operational MTTF ratio).

Systems: the **satellite** (n = 6) under three failure models (independent exponential λ = 1; lethal shock, β = 0.2;
binomial failure rate, β = 0.2); and three **asymmetric** systems under the independent model: the paper's Fig. 1 system
(n = 3), the five-component bridge network, and the 8-component system with minimal path sets {90, 203, 22, 41}
(bitmasks).

## Reproduction check (not a prediction)
The paper's Fig. 1 system with c_comp = 1/30, c_sys = 1, c_set = 0 must give long-term mean costs 97/105, 11/15 and 1/10
for its three distinct threshold policies.

## Predictions
1. **Symmetry (derived by theory).** On the satellite every state with the same number of failed components is
   equivalent by symmetry, and MTTF falls as components fail (Lemma 4), so the MTTF-threshold policies are exactly the
   r-out-of-n policies. Therefore, for all 135 combinations of cost pair and model, the best threshold cost equals the best
   r-out-of-n cost to within 1e-9 relative, and there are exactly 4 distinct threshold policies.
2. **Cheap components, no set-up cost: repair early.** For ρ = 0.1 and σ = 0 the best policy under the independent model
   is to repair at the first failure (r = 1).
3. **Expensive components: do not repair early.** For ρ = 10 the best policy under the independent model is corrective
   maintenance (r = n) for every σ.
4. **Set-up costs group replacements.** For each fixed ρ under the independent model, the best r is non-decreasing in σ.
5. **Asymmetric systems.** For each of the three asymmetric systems, the best threshold policy is never worse than the best
   r-out-of-n policy (within 1e-9 relative) in any of the 45 cost pairs, and is strictly cheaper (by more than 0.1 %) in at
   least one cost pair.

## How the hypothesis would be rejected
* any satellite combination in which the two families differ by more than 1e-9 relative, or a number of distinct threshold
  policies other than 4 (rejects 1 — this would indicate a bug or a wrong symmetry argument);
* a different best r for ρ = 0.1, σ = 0 (rejects 2); a best r other than n for any σ at ρ = 10 (rejects 3);
* a decrease of the best r when σ grows at some fixed ρ (rejects 4);
* an asymmetric system where the best r-out-of-n policy beats the best threshold policy by more than 1e-9 relative in any
  cost pair, or where the best threshold policy is never more than 0.1 % cheaper (rejects 5).
