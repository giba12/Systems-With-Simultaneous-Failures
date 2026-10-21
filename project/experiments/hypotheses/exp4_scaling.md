# Experiment 4 — Scaling and performance

**Written before the first run (2026-10-21).**

## Question
How do run time, memory and the amount of work grow with the number of components, and where does the
brute-force reference stop being practical?

## Design
Systems made of n/2 redundant pairs (n = 4, 6, …, 20), independent exponential lifetimes, one core,
Release build. Per n: the analysis time (median of repeats), the peak resident memory, the number of
evaluations of the structure function Φ, and the peak number of counting vectors held at once. Separately,
for n ≤ 14: the time of the verification (brute-force counting vectors + direct Eq. (6) solve).

## Predictions
1. **Φ is evaluated exactly 2ⁿ times** for every n (paper, Theorem 2.1).
2. **Run time grows like n²·2ⁿ**: for n ≥ 14 each step of +2 components multiplies the time by 4 to 5.
3. **Peak counting vectors** equal the largest sum of two adjacent binomial coefficients, which grows like
   2ⁿ/√n (so about ×3.8 per +2 components for large n); this is what keeps memory far below holding all
   2ⁿ vectors.
4. **Peak memory** at n = 20 is below 100 MB and grows by a factor of about 4 per +2 components from n = 18.
5. **The brute-force verification grows like 3ⁿ** (about ×9 per +2) and exceeds the analysis time by orders
   of magnitude at n = 14, so exhaustive verification is practical only up to n ≈ 14–16.
6. The analysis time for n = 6 (the satellite) is below 1 millisecond.

## How the hypothesis would be rejected
* any n with a Φ-evaluation count different from 2ⁿ;
* run-time growth per +2 components outside 3 to 6 for n ≥ 14;
* a peak vector count different from the adjacent-binomial formula;
* verification growth per +2 components below 6 or above 12 for n ≥ 10.
