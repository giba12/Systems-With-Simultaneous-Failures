# Experiment 7 — How far is the best MTTF-threshold policy from the optimal state-based policy?

**Written before the first run (2026-10-22), and before any experiment code for it existed.**

What was already seen before writing this: Experiment 5 (best threshold against best r-out-of-n); the unit tests of the
optimal policy, which show that on the paper's Fig. 1 system the optimum equals the cheapest threshold policy (1/10), that on the
satellite at one cost setting the excess is 0, and that on *some* systems among the satellite, the Fig. 1 and bridge systems and
four random 7-component systems the optimum is strictly cheaper than the best threshold (which systems and which costs was not
looked at). The paper's own result (Table III, Fig. 5), for 26 505 combinations of benchmark networks with up to 27 components,
was read: the best threshold policy is optimal in 17 % of the combinations, its excess cost has a median of 0.36 % and a
90th percentile of 3.4 %; the gap shrinks as β grows, is largest for cheap components (small ρ), and grows with the size of the
system. These predictions test whether the same *pattern* shows up on **much smaller, random** systems; they do not test the
paper's numbers, and a difference in size of population is expected.

## Question
How much money is lost by using the simple MTTF-threshold rule instead of the optimal state-based policy, and does the
pattern the paper reports (by β, by cost ratio and by size) appear on small systems?

## Design
Costs as in Experiment 5: c_sys = 1, c_comp = ρ/n, c_set = σ, with ρ ∈ {0.1, 0.2, 0.5, 2/3, 1, 4/3, 2, 4, 10} and
σ ∈ {0, 0.05, 0.1, 0.2, 0.5} (45 cost pairs). Excess = (cost of the best MTTF-threshold policy) / (cost of the optimal
state-based policy) − 1.

* **Satellite** (n = 6) under [Exp] (λ = 1), [LSh] (β = 0.2), [BFR] (β = 0.2): 135 combinations.
* **Random systems**: 8 systems of 6 components and 8 systems of 10 components, drawn with the recorded seed (20261021). Each
  system is a union of 3 to 6 path sets of 2 to ⌈n/2⌉ components. Models: [Exp] (λ = 1), [LSh] (β = 0.5), [BFR] (β = 0.5).
  16 systems × 3 models × 45 cost pairs = 2 160 combinations.

## Predictions
1. **The optimum is a lower bound (theory).** In every combination the optimal cost is at most the best threshold cost, at
   most the best r-out-of-n cost and at most the corrective-maintenance cost (within 1e-9 relative).
2. **Symmetric systems are nearly solved by the threshold.** On the satellite the excess is below 1e-9 in at least 95 % of
   the 135 combinations.
3. **On small random systems the threshold is a good rule.** Among the 6- and 10-component systems under [Exp] (720
   combinations) the median excess is below 2 % and the 90th percentile below 10 %.
4. **The optimum is not always a threshold.** Among the 2 160 random-system combinations at least 10 % have an excess above
   1e-9, and at least 10 % have an excess below 1e-9.
5. **The gap shrinks with β.** The mean excess over the random systems is larger under [Exp] than under [LSh] (β = 0.5) and
   larger under [Exp] than under [BFR] (β = 0.5).
6. **The gap is largest for cheap components.** The mean excess over all random-system combinations with ρ ≤ 0.2 is larger
   than the mean over those with ρ ≥ 4.
7. **The gap grows with the size of the system.** The mean excess over the 10-component systems is larger than over the
   6-component systems.

## How the hypothesis would be rejected
* any combination where the optimum is more than 1e-9 relative above one of the other policies (rejects 1; a bug, not a result);
* fewer than 95 % of the satellite combinations with excess below 1e-9 (rejects 2);
* a median excess of 2 % or more, or a 90th percentile of 10 % or more, under [Exp] (rejects 3);
* fewer than 10 % of the random-system combinations with an excess above 1e-9, or fewer than 10 % with an excess below 1e-9
  (rejects 4);
* a mean excess under [Exp] that is not larger than under [LSh], or not larger than under [BFR] (rejects 5);
* a mean excess for ρ ≤ 0.2 that is not larger than for ρ ≥ 4 (rejects 6);
* a mean excess for n = 10 that is not larger than for n = 6 (rejects 7).

## Limits stated in advance
Random systems are not benchmark networks; only 16 of them are drawn, so results depend on the seed; components are identical;
the paper's systems have up to 27 components and these have at most 10. Results will be reported as found; this file will not
be edited after the first run.
