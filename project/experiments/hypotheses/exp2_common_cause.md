# Experiment 2 — Independent versus simultaneous failures

**Written before the first run (2026-10-21).**

## Question
How does a common-cause failure mechanism change the benefit of redundancy for the satellite?

## Design
Every component is calibrated to a mean lifetime of 1, so models differ only in how failures are grouped
(Lagos & Ávila, §VII-B). The strength of common cause is the **pairwise beta factor** β ∈ {0.01, 0.05, 0.10, 0.20, 0.50};
β = 0 is the independent model. Two models: **lethal shock** (a shock fails *every* working component) and
**binomial failure rate** (a shock fails each working component independently with probability β).
For each model and β we measure the MTTF of the satellite and of the **series-of-six baseline built from the same
six components under the same model**, and report the ratio *satellite ÷ series6* as the "benefit of redundancy".
(The absolute MTTF alone would mislead: see the prediction below.)

## Predictions
1. **Absolute MTTF of the satellite rises with β** under this calibration (failures become bunched), for both
   models. For the lethal-shock model: 0.7000 (β = 0) → 0.9143 (β = 0.5).
2. **The benefit ratio falls strictly as β grows**, for both models. Lethal shock, β = 0.5: ratio ≈ 3.2,
   against 4.2 at β = 0.
3. **The binomial-failure-rate model hurts redundancy less than the lethal-shock model** at the same β:
   its ratio lies above the lethal-shock ratio at every β in the table (a shock that spares some components
   leaves the redundancy partly intact).
4. At β = 0.01 both models are within 2 % of the independent baseline ratio of 4.2.

## How the hypothesis would be rejected
* the absolute MTTF *falling* with β for either model (rejects 1);
* a ratio that fails to decrease from one β to the next (rejects 2);
* the binomial-failure-rate ratio at or below the lethal-shock ratio at any β in the table (rejects 3);
* the lethal-shock numbers differing from the closed form 8/(β+3r) − 12/(β+4r) + 6/(β+5r) − 1/(β+6r), r = 1 − β,
  by more than 1e-9 relative (indicates a bug).
