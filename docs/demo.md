# Demonstration

A two-minute tour of the analyzer. It needs only a C++ compiler, CMake and `python3` (standard library):

```bash
project/demo.sh
```

The script builds the tool on the first run, then walks through five steps: the fictional satellite and its
independent verification; two states with the same number of failures and very different futures; what common-cause
failures do to the benefit of redundancy; the cost of analysing a million states exactly; and when a repair is worth it.

Captured output (the analysis times in step 4 vary by machine; everything else is deterministic):

```text

=== 1. The system: three redundant pairs (power, communications, computers); all three pairs must survive ===
  expected lifetime of the new system (a component lives 1 on average): 0.7
  working states out of 64: 27
  chance the system dies at the k-th component failure (k = 1..6): [0.0, 0.2, 0.4, 0.4, 0.0, 0.0]
  minimal sets of parts that keep it alive: 8
  independent verification (brute force + a second equation): PASSED
  structure function evaluated 64 times = 2^6, once per state

=== 2. Same number of failures, very different futures ===
  power unit A and communications unit A failed : expected remaining life 0.4167
  power unit A and power unit B failed          : expected remaining life 0.0000   (a whole pair is gone)

=== 3. Common-cause failures (one event can fail several parts at once) ===
  model                              satellite MTTF     six in series      benefit of redundancy
  independent failures               0.7000             0.1667             x4.20
  lethal shock, beta=0.2             0.7761             0.2000             x3.88
  lethal shock, beta=0.5             0.9143             0.2857             x3.20
  binomial failure rate, beta=0.5    0.7603             0.5079             x1.50
  (absolute lifetime can rise under common cause, but redundancy is worth less relative to a series system)

=== 4. The price of exactness: 20 components = 1,048,576 states, every one analysed ===
  analysis time: 7.85 s, peak memory: 34.7 MB
  structure function evaluations: 1048576  (exactly 2^20)
  counting vectors held at once: 352716  (two levels, instead of all 1,048,576)

=== 5. When is a repair worth it? (the paper's cost model: replace every failed part, restoring a new system) ===
  the paper's Fig. 1 system (3 components, replacing a part costs 1/30 of a system failure, no set-up cost):
    repair when MTTF falls below 0.00 - 0.43 of new: cost per time 0.9238   (paper: 97/105)
    repair when MTTF falls below 0.43 - 0.86 of new: cost per time 0.7333   (paper: 11/15)
    repair when MTTF falls below 0.86 - 1.00 of new: cost per time 0.1000   (paper: 1/10)
  the satellite (replacing everything costs as much as one system failure, each repair visit costs 0.2):
    best rule: repair after 2 failures, cost per time 2.0002; waiting for the system to fail costs 2.4763

Done. Reproduce the six experiments and the figures with the commands in README.md.
```

What to notice:

* **Step 1.** The structure function is asked exactly 64 = 2⁶ times, once per state, and the result is checked by a
  brute-force counter and by a second, independent equation.
* **Step 2.** Same number of failures, MTTF 0.4167 against 0: *which* components failed matters.
* **Step 3.** Under common cause the satellite's absolute lifetime can rise, yet redundancy is worth less relative to
  six components in series (4.20 → 3.20 → 1.50). The binomial model hurts more than the lethal shock at the same beta —
  a prediction I had made the other way round (see the [experiment report](experiment-report.md)).
* **Step 4.** 2²⁰ states analysed exactly in about three seconds, holding about a third of the vectors at once.
* **Step 5.** The paper's own Fig. 1 repair-policy costs (97/105, 11/15, 1/10) come out of the delivered code; on the
  satellite, repairing after the second failure beats waiting for the system to fail.

The satellite is a **fictional** educational model.
