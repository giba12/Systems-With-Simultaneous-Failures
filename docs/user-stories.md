# User Stories — Space System Reliability Analyzer

Related: [requirements-spec.md](requirements-spec.md) · [test-plan.md](test-plan.md) · [paper-validation-report.md](paper-validation-report.md)

*Version 1.1 — revised 2026-10-20 after validation against the paper.*

**Personas**

- **Alex, the Reliability Analyst** — builds and analyses system models.
- **Riley, the Verifier** — checks correctness and traceability.
- **Sam, the Portfolio Reader** — a hiring reviewer or mentor who reads the results.
- **Dana, the Developer** — the learner who builds the tool, using AI as a pair programmer.

Priority: P0 required · P1 extension · P2 optional. Estimates are relative (S/M/L).

## Epic 1 — Model the system

### US-01 Define a system from a config file (P0, S)
**As** Alex, **I want** to describe components and their redundancy in a JSON file, **so that** I can analyse an architecture without recompiling.
- Given `satellite_support.json`, when I load it, then I get six components P1, P2, C1, C2, K1, K2.
- Duplicate IDs, unknown components or malformed JSON give an error naming the problem.
- *Reqs:* SYS-003, INT-004 · *Tests:* TC-C-12, TC-E-02, TC-E-03, TC-E-09

### US-02 Reject invalid or non-monotone models (P0, S)
**As** Alex, **I want** invalid models rejected up front, **so that** I never get results from a system the algorithm doesn't support.
- A non-monotone structure (e.g. XOR) is rejected with an explanation.
- n ≤ 1, a constant structure function (Φ(**0**) = 0 and Φ(**1**) = 1 are required), or n above the supported maximum is rejected.
- A system with an irrelevant component is **accepted** (the paper covers semi-coherent systems).
- *Reqs:* SYS-001, SYS-005 · *Tests:* TC-E-01, TC-E-04, TC-E-06, TC-E-10, TC-E-11, TC-E-12

### US-03 Inspect states and the structure function (P0, S)
**As** Alex, **I want** to see which of the 64 states keep the satellite-support system operational, **so that** I can sanity-check my model by hand.
- All 64 masks listed with working/failed components.
- 27 states are operational.
- *Reqs:* ALG-001, SYS-004 · *Tests:* TC-C-01, TC-C-02

## Epic 2 — Compute signatures

### US-04 Compute counting vectors for every state (P0, M)
**As** Alex, **I want** the counting vector Wₖˣ for each state, **so that** I know how many failure sequences each state survives.
- For the all-working state, W = [1, 6, 12, 8, 0, 0, 0].
- The structure function is evaluated exactly once per state (64 calls), sweeping level by level, as in the paper's Algorithm 1.
- *Reqs:* ALG-002, ALG-004 · *Tests:* TC-C-04, TC-C-05, TC-C-09, TC-C-14

### US-05 Obtain the system signature (P0, S)
**As** Alex, **I want** the signature of the system and of every partially failed state, **so that** I can see at which failure number the system is likely to fail from wherever it currently is.
- Reference system signature is (0, 0.2, 0.4, 0.4, 0, 0) and sums to 1; with P1 failed it is (0.2, 0.4, 0.4, 0, 0).
- Series systems yield (1, 0, …, 0).
- *Reqs:* ALG-003 · *Tests:* TC-A-03, TC-B-03, TC-C-06, TC-F-02

### US-06 Find minimal path sets (P1, M)
**As** Alex, **I want** the minimal sets of components that keep the system working, **so that** I can identify which units matter most.
- They come out of the same sweep (states with W₀ = 1 and W₁ = 0), with no extra evaluations of Φ.
- Reference system returns 8 sets of 3 components.
- Removing any member of a returned set fails the system.
- *Reqs:* SYS-002 · *Tests:* TC-C-10

## Epic 3 — Compute reliability

### US-07 Compute MTTF under independent exponential lifetimes (P0, M)
**As** Alex, **I want** MTTF for any state under a stated failure model, **so that** I can quantify expected operating life.
- Reference system, all working: MTTF = 0.7/λ. One-out-of-two: 1.5/λ. Series of n: 1/(nλ). The paper's Fig. 1 example gives 7/6.
- Results agree with a direct solution of the MTTF equations (Eq. (6)) on every state.
- λ ≤ 0 is rejected; failed states give MTTF 0.
- *Reqs:* REL-001, REL-003, REL-005 · *Tests:* TC-A-04, TC-B-04, TC-C-07, TC-C-08, TC-C-13, TC-C-14, TC-E-07

### US-08 Model common-cause failures (P1, L)
**As** Alex, **I want** a model in which one event can fail several components, **so that** I can see how common-cause faults erode the benefit of redundancy.
- The model is the paper's lethal-shock model (Lévy-frailty Marshall-Olkin family), parameterised by the pairwise beta factor β with unit mean component lifetime.
- β = 0 reproduces the independent model; β outside [0,1] is rejected.
- The reference system's MTTF under the model matches the closed form (e.g. 32/35 at β = 0.5).
- *Reqs:* REL-002, REL-004, REL-005 · *Tests:* TC-D-01 to TC-D-08, TC-E-08

### US-09 Compare states with equal numbers of failures (P0, S)
**As** Alex, **I want** to compare states that have the same number of failed components but different ones, **so that** I understand why a simple failure count loses structural information.
- Per-state MTTF differs between states with the same number of failures: with P1 and C1 failed, MTTF = 5/(12λ); with P1 and P2 failed the system is down (MTTF 0).
- This is the paper's motivating example: a failure count alone discards information.
- *Reqs:* EXP-002 · *Tests:* TC-G-05

## Epic 4 — Verify correctness

### US-10 Check the algorithm against a brute-force reference (P0, M)
**As** Riley, **I want** an independent brute-force counter that does not use the recurrence, **so that** I can trust the recursive algorithm.
- All 64 states match between the two implementations.
- Randomly generated monotone systems (fixed seed, n ≤ 8) also match.
- *Reqs:* ALG-005, VER-001 · *Tests:* TC-C-03, TC-F-05

### US-11 Validate against analytical cases (P0, S)
**As** Riley, **I want** series and 1-out-of-2 systems with closed-form answers, **so that** a bug shared by both implementations can't hide.
- Oracles ORC-1 to ORC-4 pass.
- *Reqs:* VER-001, REL-001 · *Tests:* Tests A, B, C-04..C-07

### US-12 Detect numerical problems (P0, S)
**As** Riley, **I want** automated checks for NaN, negative values and signatures that don't sum to 1, **so that** numerical issues are caught early.
- *Reqs:* VER-002 · *Tests:* Test F

### US-13 See requirement-to-test traceability (P0, S)
**As** Riley, **I want** a matrix mapping requirements to tests, **so that** I can see nothing is unverified.
- Every P0 requirement has at least one passing test.
- *Reqs:* VER-003 · *Tests:* test plan §6

### US-14 Run tests automatically in CI (P0, S)
**As** Dana, **I want** CI to build and test on every push, **so that** regressions are caught.
- *Reqs:* NFR-007 · *Tests:* TC-H-02

### US-24 Check the paper's structural properties (P0, M)
**As** Riley, **I want** automated checks of the paper's theorems on the implementation, **so that** I get evidence beyond the worked examples.
- Signature dominance (Theorem 5): if one state's tail signature is at least another's (same number of working components), its MTTF is at least as large.
- MTTF monotonicity (Lemma 4) holds under the independent and lethal-shock models.
- *Reqs:* VER-004 · *Tests:* TC-I-01 to TC-I-04

### US-25 Reproduce the paper's worked example (P0, S)
**As** Dana, **I want** to reproduce the paper's Fig. 1 example, **so that** I can claim it as a *reproduced* result, separate from my own experiments.
- n = 3 system: W = (1,3,1,0), signature (0, 2/3, 1/3), m(**1**) = 7/6, and per-state ratios 100 %, 86 %, 43 %, 0 %.
- *Reqs:* REL-001, DOC-002 · *Tests:* TC-C-14

### US-26 Be warned about numerical limits (P1, S)
**As** Riley, **I want** the tool to reject or flag sizes at which the simultaneous-failure formulas lose precision, **so that** I don't trust results that have silently degraded.
- In plain double precision the error of the increments grows from ~1e-14 (n=6) to ~1e-8 (n=20) and ~1e-4 (n=30).
- *Reqs:* NFR-009 · *Tests:* TC-F-07

## Epic 5 — Run experiments

### US-15 Run reproducible experiments (P0, M)
**As** Alex, **I want** to run an experiment from a config and seed, **so that** I get the same output every time.
- Re-running with the same config and seed gives identical results.
- Results are saved with the configuration used.
- A hypothesis is written before the run.
- *Reqs:* EXP-001, EXP-002 · *Tests:* TC-G-01, TC-G-02, TC-G-07

### US-16 Compare redundancy levels (P0, M)
**As** Alex, **I want** to compare series, redundant-pair and six-component systems under the same failure model, **so that** I can show how redundancy extends expected life.
- Output: initial-state MTTF, signature, operational-state count and runtime.
- *Reqs:* EXP-002 · *Tests:* TC-G-04

### US-17 Compare independent and common-cause failures (P1, M)
**As** Alex, **I want** to sweep the beta factor, **so that** I can see how common-cause failures change the value of redundancy.
- Models are compared only when their parameterisation is stated (same unit mean lifetime, same β).
- Report redundancy benefit relative to a series baseline: absolute MTTF *rises* with β under this calibration, while the ratio falls (4.20 → 3.88 → 3.20 at β = 0, 0.2, 0.5).
- *Reqs:* REL-002, EXP-002 · *Tests:* TC-D-01, TC-D-04

### US-18 Measure scaling (P0, M)
**As** Alex, **I want** runtime, memory and operation counts for growing n, **so that** I can state a practical size limit.
- Brute-force comparison only where both are practical.
- *Reqs:* NFR-003, EXP-002 · *Tests:* TC-G-06

### US-19 Plot results from saved data (P0, S)
**As** Sam, **I want** plots generated from saved raw results, **so that** I can see the findings without re-running anything.
- *Reqs:* EXP-003 · *Tests:* TC-G-03

### US-20 Compare repair policies (P2, L)
**As** Alex, **I want** to compare a threshold-based and a failure-count policy, **so that** I can explore maintenance decisions.
- Policies are the paper's: repair when m(x)/m(**1**) < γ, versus repair after r failures; cost is the long-term mean cost with setup cost.
- All breakpoints of γ are searched (the cost curve is not unimodal). The Fig. 1 example gives 1/10, 11/15, 97/105.
- Only after the core is verified.
- *Reqs:* EXP-004 · *Tests:* TC-J-01 to TC-J-03

## Epic 6 — Communicate honestly

### US-21 Read the limitations (P0, S)
**As** Sam, **I want** assumptions, complexity and deviations from the paper stated plainly, **so that** I can judge what the project does and doesn't show.
- The system is labelled fictional; omitted effects are listed; reproduced results are separate from new experiments.
- *Reqs:* DOC-001, DOC-002, NFR-008 · *Tests:* TC-H-03

### US-22 Understand the architecture quickly (P0, S)
**As** Sam, **I want** an architecture diagram, a state example and one reproducible result, **so that** I can grasp the project in minutes.
- Diagrams in [uml/](uml/); README links to them.
- *Reqs:* DOC-002 · *Tests:* TC-H-03

### US-23 Learn from the build (P0, M)
**As** Dana, **I want** a notation glossary and a deviation log, **so that** I can explain each equation and what I verified myself rather than relying on the AI.
- Glossary written before coding (Day 1 deliverable).
- *Reqs:* DOC-001 · *Tests:* TC-H-03

## Story map by release

| Milestone | Stories |
|---|---|
| Core (Days 1–6) | US-01…05, US-07, US-09…US-14, US-24, US-25 |
| Extensions (Days 7–8) | US-06, US-08, US-15…US-19, US-26 |
| Delivery (Days 9–10) | US-21…US-23 |
| Optional | US-20 |
