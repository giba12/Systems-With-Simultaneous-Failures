# Experiment 3 — States with equal failure counts

**Written before the first run (2026-10-21).**

## Question
Can two states with the same number of failed components differ in reliability, and by how much?

## Design
All 64 states of the satellite, independent exponential lifetimes (λ = 1). Group the states by the number
of failed components and list the distinct MTTF values in each group.

## Predictions (by symmetry and the closed form in `docs/test-plan.md`)
| failed components | states | operational states | distinct MTTF values |
|---|---|---|---|
| 0 | 1 | 1 | {0.7} |
| 1 | 6 | 6 | {8/15 ≈ 0.5333} — all six states identical |
| 2 | 15 | 12 | {5/12 ≈ 0.4167, 0} — 0 exactly when the two failed units are partners of one pair (3 states) |
| 3 | 20 | 8 | {1/3 ≈ 0.3333, 0} — non-zero only when one unit of every pair has failed |
| 4, 5, 6 | 15, 6, 1 | 0 | {0} |

So the failure count alone loses structural information for exactly two counts (2 and 3), and the largest
gap is between a state still worth 5/12 and one worth nothing, with the same number of failures.

## How the hypothesis would be rejected
* a different number of distinct values in any group, or different values by more than 1e-9;
* any state with Φ(x) = 0 that has a non-zero MTTF.
