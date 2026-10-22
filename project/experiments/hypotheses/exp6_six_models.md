# Experiment 6 — Six common-cause models, one pairwise β

**Written before the first run (2026-10-22), and before any experiment code for it existed.**

What was already seen before writing this: Experiment 2 (satellite under [LSh] and [BFR] only) and the unit tests of the four
new models (parameters, kernels, Eq. (6) = Eq. (7)). No satellite MTTF under [LFMO-C], [LFMO-G], [LFMO-IG] or [LFMO-S] has
been looked at.

## Question
All six models of the paper's Table I are calibrated to the same pairwise β = 2 − Ψ(2). Does the pairwise β alone decide the
reliability of a redundant system, or does the shape of the common-cause law (how big the simultaneous-failure bundles are
for more than two components) matter?

## Design
Models: [LSh], [BFR], [LFMO-C], [LFMO-G], [LFMO-IG], [LFMO-S]. β ∈ {0.05, 0.2, 0.5} ([LFMO-IG] is defined for β ≤ 2 − √2 ≈ 0.586,
so all three are allowed). Systems: one redundant pair (n = 2), the satellite (n = 6, three redundant pairs in series), and a
series system of the same six components. Quantity: MTTF with all components working (Eq. (7)). Derived quantity: the
**redundancy ratio** = satellite MTTF / series MTTF under the same model and β.

## Reproduction check (not a prediction)
The one-pair MTTF depends only on Ψ(1) = 1 and Ψ(2) = 2 − β, so it must be identical across the six models for each β, and the
series MTTF must equal 1/Ψ(6) (computed from the model's own Ψ) within 1e-9 relative.

## Predictions
1. **The pair is blind to the model shape (theory).** For each β the six one-pair MTTFs agree within 1e-9 relative.
2. **At a small pairwise β the models are practically interchangeable for the satellite.** At β = 0.05 the largest satellite
   MTTF divided by the smallest over the six models is below 1.10.
3. **The spread grows with β.** The max/min satellite-MTTF ratio is strictly larger at β = 0.2 than at β = 0.05, and strictly larger
   at β = 0.5 than at β = 0.2.
4. **Bundle size orders the redundancy ratio.** For each β, order the models by their mean bundle size at six components,
   6/Ψ(6) (a larger value means more components fail together on average). The redundancy ratio is then in the **reverse**
   order: the model with the largest 6/Ψ(6) has the smallest redundancy ratio, and so on down, a strictly monotone ranking for
   all three β (no ties, no swapped pair).
5. **Series and satellite rank the models identically.** For each β the ranking of the six models by satellite MTTF equals the
   ranking by series MTTF (= 1/Ψ(6)).

## How the hypothesis would be rejected
* any β where two one-pair MTTFs differ by more than 1e-9 relative, or where a series MTTF differs from 1/Ψ(6) by more than
  1e-9 (rejects 1 and signals a bug — the reproduction check failing is not a result, it is a defect);
* a max/min satellite ratio of 1.10 or more at β = 0.05 (rejects 2);
* a ratio at larger β that is not strictly larger than at the smaller β (rejects 3);
* any β at which the two rankings of prediction 4 differ in even one pair of models (rejects 4);
* any β at which the satellite ranking and the series ranking differ (rejects 5).

Results will be reported as found; this file will not be edited after the first run.
