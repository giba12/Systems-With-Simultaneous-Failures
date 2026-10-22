# Experiment Report — Space System Reliability Analyzer

| | |
|---|---|
| **Date** | 2026-10-21 |
| **Tool** | `analyze` 0.8.0 (Release build, one core) |
| **Machine** | Intel Core i7-9750H @ 2.6 GHz, WSL2 Linux, Python 3.12.3 |
| **Seed** | 20261021 (recorded; all four experiments are deterministic, no random numbers are drawn) |
| **Hypotheses** | [`project/experiments/hypotheses/`](../project/experiments/hypotheses/), committed as `0a3e51c` **before** the experiment runner existed |
| **Raw results** | `project/results/exp*/` (not committed; regenerate with the commands in §7) |

This report separates what **reproduces the paper** from what is **new**, states every prediction that was
**rejected**, and lists the deviations and limits. The system is a **fictional** educational model: nothing here
is flight-qualified or a claim about any real spacecraft.

## 1. What is reproduced and what is new

| Item | Status |
|---|---|
| The per-state counting-vector recursion, signature and MTTF formulas (Lemma 1, Eqs. (1), (2), (7)) | **Reproduced** from the paper; the paper's own Fig. 1 example matches exactly (unit tests) |
| Algorithm 1 uses exactly 2ⁿ evaluations of Φ and two levels of memory (Theorem 2) | **Reproduced** in Experiment 4 |
| Lemma 4 (monotone MTTF), Theorem 5 (dominance ⇒ MTTF order) | **Reproduced** by property tests; its converse is shown false with a concrete example (new) |
| Eq. (21) | **Corrected**: the printed lower summation limit is a typo (see the validation report §6) |
| Experiments 1–4 on the fictional satellite | **New**, designed for this project (the premise of Experiment 3 is the paper's motivating observation) |
| Comparison of the lethal-shock and binomial-failure-rate models on a multi-pair system | **New** |

## 2. How the experiments were run

Each experiment has a hypothesis file with falsifiable predictions and rejection criteria, written first.
`run_experiments.py` refuses to run without it, copies it next to the results, and records its SHA-256 and
whether it was last modified before the run. Results are split into a **deterministic** file (settings, seed,
tool version, hypothesis hash, raw output of `analyze`) and a **timing** file; the deterministic files are
byte-identical across runs (tested). `analyze_results.py` then checks every prediction against the data and writes
`hypothesis_check.md`; `plot_results.py` draws the figures from the saved tables only. All raw outputs pass the
tool's own independent verification (brute-force reference, direct solution of Eq. (6), numerical checks) where
the system is small enough (n ≤ 16).

## 3. Experiment 1 — Redundancy versus reliability &nbsp;·&nbsp; 15 of 15 predictions supported

![Redundancy lengthens expected life](figures/fig1_redundancy.png)

Independent exponential lifetimes, a component lives 1 time unit on average.

| System | n | MTTF (all working) | Operational states | Minimal path sets | System signature |
|---|---|---|---|---|---|
| Six units in series | 6 | 0.1667 (= 1/6) | 1 of 64 | 1 | all weight on the 1st failure |
| One redundant pair | 2 | 1.5000 | 3 of 4 | 2 | all weight on the 2nd failure |
| **Satellite** (three pairs) | 6 | **0.7000** | 27 of 64 | 8 | (0, 0.2, 0.4, 0.4, 0, 0) |
| Six units, any one suffices | 6 | 2.4500 (= H₆) | 63 of 64 | 6 | all weight on the 6th failure |

Every analysis took about 0.1 ms. Redundancy lengthens life as predicted, and the satellite sits where its
structure puts it: four times better than six units in series, but worse than a single redundant pair, because
all three pairs must survive.

## 4. Experiment 2 — Independent versus simultaneous failures &nbsp;·&nbsp; 6 of 8 supported, **2 rejected**

![Common-cause failures erode the benefit of redundancy](figures/fig2_common_cause.png)

Every component is calibrated to a mean life of 1, so the models differ only in how failures are grouped. The
"benefit of redundancy" is the satellite's MTTF divided by the MTTF of the same six components in series under
the same model.

| β | Lethal shock: satellite | series | **ratio** | Binomial failure rate: satellite | series | **ratio** |
|---|---|---|---|---|---|---|
| 0 (independent) | 0.7000 | 0.1667 | 4.200 | 0.7000 | 0.1667 | 4.200 |
| 0.01 | 0.7035 | 0.1681 | 4.186 | 0.7015 | 0.1709 | 4.105 |
| 0.05 | 0.7178 | 0.1739 | 4.127 | 0.7074 | 0.1887 | 3.748 |
| 0.10 | 0.7364 | 0.1818 | 4.050 | 0.7144 | 0.2134 | 3.347 |
| 0.20 | 0.7761 | 0.2000 | 3.880 | 0.7275 | 0.2711 | 2.684 |
| 0.50 | 0.9143 (= 32/35) | 0.2857 | 3.200 | 0.7603 | 0.5079 | **1.497** |

**Supported:** absolute MTTF *rises* with β under both models (failures bunch together, so a series-like system
suffers less); the redundancy ratio *falls* at every step for both models; the lethal-shock values match the
closed form to 1.5 × 10⁻¹⁵.

**Rejected (reported as found):**

1. *"The binomial failure rate hurts redundancy less than the lethal shock."* The opposite is true. At β = 0.5 the
   benefit of redundancy under the binomial model is 1.50, against 3.20 under the lethal shock.
2. *"At β = 0.01 both models are within 2 % of the independent baseline."* The lethal shock is (0.34 %); the
   binomial model is 2.26 % away, narrowly missing the limit.

**Follow-up observation (not pre-registered, so a hypothesis to test, not a finding).** The paper calibrates both
models to agree on every *pair* of components. A one-pair system gives exactly the same MTTF under both
(1.444444 at β = 0.2, 1.333333 at β = 0.5), as it must. With two pairs (4 components) the models differ
(β = 0.2: 0.9779 versus 0.9216) and the gap persists with three pairs (0.7761 versus 0.7275). A plausible reason is
that under the lethal shock all pairs share a single shock stream, while under the binomial model each pair has its
own chances of being struck, so a system that needs *every* pair is exposed to more independent opportunities for
failure. This was not tested further.

## 5. Experiment 3 — States with equal failure counts &nbsp;·&nbsp; 9 of 9 predictions supported

![The number of failures does not tell the whole story](figures/fig3_equal_failure_counts.png)

All 64 satellite states, independent exponential lifetimes.

| Failed components | States | Operational | Distinct MTTF values |
|---|---|---|---|
| 0 | 1 | 1 | 0.7000 |
| 1 | 6 | 6 | 0.5333 (all six states identical) |
| 2 | 15 | 12 | **0.4167 and 0** |
| 3 | 20 | 8 | **0.3333 and 0** |
| 4, 5, 6 | 15, 6, 1 | 0 | 0 |

With two failures the system is still worth 0.4167 if the failed units are in different pairs, and is worth
nothing if they are partners of one pair (3 of the 15 states). So the failure count alone discards structural
information for exactly two counts, which is the paper's motivating observation, here measured on the satellite.

## 6. Experiment 4 — Scaling and performance &nbsp;·&nbsp; 6 of 8 supported, **2 rejected**

![Scaling](figures/fig4_scaling.png)

Systems of n/2 redundant pairs, n = 4 … 20, one core.

| n | Analysis (ms) | ×previous | Peak memory (MB) | Verification (ms) | ×previous |
|---|---|---|---|---|---|
| 4 | 0.04 | | 4.0 | 0.04 | |
| 6 | 0.08 | 2.0 | 4.1 | 0.14 | 3.3 |
| 8 | 0.38 | 4.5 | 4.1 | 1.9 | 13.3 |
| 10 | 1.8 | 4.8 | 4.1 | 8.9 | 4.8 |
| 12 | 7.4 | 4.1 | 4.2 | 79 | 8.9 |
| 14 | 37 | 5.0 | 4.5 | 856 | 10.8 |
| 16 | 168 | 4.5 | 5.9 | | |
| 18 | 696 | 4.2 | 11.6 | | |
| 20 | 3172 | 4.6 | 34.7 | | |

**Supported:** Φ is evaluated exactly 2ⁿ times for every n (Theorem 2.1); the peak number of counting vectors equals
the largest sum of two adjacent binomial coefficients for every n (so only about 1/3 of the 2ⁿ vectors exist at
once at n = 20); analysis time grows by a factor of 4.2 to 5.0 per two components for n ≥ 14, consistent with
O(n²·2ⁿ); memory at n = 20 is 34.7 MB (×3.0 from n = 18); the satellite-sized analysis takes under 1 ms.

**Rejected (reported as found):**

1. *"Verification time grows by a factor between 6 and 12 per two components for n ≥ 10."* The ratios are 4.8, 8.9
   and 10.8. The 4.8 comes from timings of a few milliseconds at n = 8 and 10 where fixed overhead still matters; the
   two larger-n ratios are inside the predicted range. As stated, the criterion is rejected.
2. *"Verification is at least 100 times slower than the analysis at n = 14."* It is 23 times slower (856 ms against
   37 ms). The gap is widening (×9 against ×4.4 per step), so an extrapolation, not a measurement, puts the crossover
   near n ≈ 18, beyond where the brute-force reference is run.

## 7. Reproducing everything

```bash
cmake -S project -B project/build -DCMAKE_BUILD_TYPE=Release && cmake --build project/build
cd project
python3 python/run_experiments.py all                  # raw results into results/
.venv/bin/python python/analyze_results.py             # tables and hypothesis_check.md
.venv/bin/python python/plot_results.py                # figures into results/figures
```

(`.venv` holds pandas and matplotlib; `python3 -m venv .venv && .venv/bin/pip install -r python/requirements.txt`.)
The deterministic result files are byte-identical on a repeat run; run times and memory differ slightly.

## 8. Limitations and deviation log

* **Identical components.** The paper's model requires exchangeable (identical) components, so the three pairs of the
  satellite are modelled with the same lifetime law; real power, communication and computing units would differ.
* **Perfect redundancy** inside each pair; no switch-over failure, shared resources, thermal coupling or software
  common-mode faults beyond what the common-cause models express.
* **Simultaneous-failure models are limited to n ≤ 16** (numerical precision of the alternating sums; measured, see the
  validation report). Experiment 4 uses the independent model and therefore reaches n = 20.
* **Single machine, single core.** Run times are medians of 1 to 5 repeats on one laptop under WSL2; treat them as
  orders of magnitude and growth rates, not benchmarks. The paper's Julia implementation reaches n = 30.
* **Verification limited to n ≤ 14** in Experiment 4 (the brute-force reference does about 3ⁿ work).
* **Measurement bug found and fixed during this work.** The first runs reported a constant 19 MB of peak memory for all
  small n. On Linux `ru_maxrss` is inherited across fork and exec, so it contained the Python parent's memory. The tool
  now reads `VmHWM` from `/proc/self/status`. All reported memory figures are from the corrected tool.
* **Reading of Algorithm 1.** The code evaluates Φ(**0**) as well, so the count is exactly 2ⁿ; the paper's pseudo-code
  sets that state's result without calling Φ while Theorem 2.1 says 2ⁿ evaluations.
* **Eq. (21).** The paper's printed lower limit is wrong; the corrected form is used and tested.
* **Light-surface figures only.** The charts use a validated light palette; a dark variant would need its own
  validated steps.
