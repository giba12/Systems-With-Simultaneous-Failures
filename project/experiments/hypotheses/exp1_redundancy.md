# Experiment 1 — Redundancy versus reliability

**Written before the first run (2026-10-21).** Predictions come from the closed forms in `docs/test-plan.md`
and from first principles; the experiment can refute them.

## Question
How does redundancy change the expected operating life of a six-unit system, and how is the failure order
spread over the failures?

## Systems (independent exponential lifetimes, rate λ = 1, so one component lives 1 time unit on average)
1. **series6** — six components, all needed.
2. **one_pair** — a single redundant pair (2 components, either is enough).
3. **satellite** — the reference system: three redundant pairs, all three pairs needed.
4. **parallel6** — six components, any one is enough (upper bound).

## Predictions
| system | MTTF of the all-working state | operational states (of 2ⁿ) | signature (system fails at failure number k) |
|---|---|---|---|
| series6 | 1/6 ≈ 0.1667 | 1 of 64 | all weight on k = 1 |
| one_pair | 3/2 = 1.5 | 3 of 4 | all weight on k = 2 |
| satellite | 7/10 = 0.7 | 27 of 64 | (0, 0.2, 0.4, 0.4, 0, 0): nothing at k = 1, 5, 6 |
| parallel6 | H₆ = 49/20 = 2.45 | 63 of 64 | all weight on k = 6 |

* Ordering of MTTF: series6 < satellite < one_pair < parallel6 (the pair beats the six-unit satellite because
  fewer things must work at once).
* Minimal path sets: 1, 2, 8, 6.
* Run time of the analysis: well under 1 millisecond for every system.

## How the hypothesis would be rejected
* any MTTF differing from the value above by more than 1e-9 relative (that would indicate a bug, not a new finding);
* a different MTTF ordering;
* a signature that does not sum to 1 or has a negative entry.
