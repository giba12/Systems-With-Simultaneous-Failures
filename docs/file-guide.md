# File-by-File Guide to the Space System Reliability Analyzer

| | |
|---|---|
| **Audience** | A reader who follows the project at a high level but wants to understand what each technical piece is, what it does, and why it is in this file |
| **Scope** | Every file of the repository as of 2026-10-27, except the generated figures and the UML images. Files added later are listed in Part 10 |
| **Written** | 2026-10-27, from reading the files themselves. Statements about what a file does come from its code and comments. Part 10 and the document table in §7.3 were brought up to date on 2026-11-03. |
| **Status** | Explanatory document only. It changes no code. |

## How to read this guide

1. Read **Part 1** (the picture in one page) and **Part 2** (the technology primer). Part 2 explains each technology once:
   what it is, how it behaves, and what it does in this project. The file sections in Parts 3 to 7 then refer back to it
   with a short tag such as *[bitmask]*.
2. Read the file sections in the order you care about. Each one says what the file is for, what it needs from its
   companion file, which technologies it uses, and which technical idea in it is worth understanding.
3. Part 8 lists a few places where a comment in the code was out of date; they have been corrected and no result changed. Part 10 lists what was added after this guide was written.

---

## Part 1 — The picture in one page

**The question the software answers.** Given a system built from components (the example is a fictional satellite with three
redundant pairs: power, communications, computers), *on average how long does it keep working?* That number is the MTTF
(mean time to failure). The software also answers it from *every partly failed condition*, not just from "everything works",
and it can compare strategies for when to repair.

**The trick the paper contributes.** There are 2ⁿ possible conditions of n components (64 for the satellite, about a million for
20 components). Counting them naively is hopeless for large systems. The paper's algorithm walks through the conditions
*one layer at a time* (all conditions with 0 working components, then 1, then 2, …). It reuses the previous layer's counts
to produce the next layer's counts, so each condition costs only a little work, and only two layers are ever in memory.

**The data flow.**

```
 configs/*.json ──► system_model ──► signature (the layer-by-layer sweep)
   "what is the         "does it          │  for every condition it produces a
    system?"             still work?"     │  counting vector (counts of survivable failure patterns)
                                          ▼
 failure_model ───────────────────►  mttf  ──►  one MTTF number per condition
   "how fast do                               │
    components die?"                          ├──► policy   (when should we repair? what does it cost?)
                                              │
 reference (slow, independent brute force) ───┴──► used by tests and by --verify to check all of the above

 main.cpp wraps all of this as the command-line tool "analyze" ──► writes JSON
 python/ runs analyze many times, checks the results against predictions written beforehand, draws figures
 tests/ and CI check everything automatically on every push
```

**Why so many files check each other.** The project's credibility rests on getting the *same answer by independent routes*:
the fast algorithm, a deliberately naive brute-force version, closed-form formulas, and the paper's own equations.
Several files exist only to be a second opinion.

---

## Part 2 — Technology primer (read once)

### 2.1 Languages and tools

**C++20** — a compiled, general-purpose language. *Behaviour:* the human-readable source is translated by a **compiler**
(here `g++` and `clang++`) into machine code before it runs, which makes it fast and lets the programmer control memory
and number types precisely. *Here:* the whole calculation engine is C++, because the sweep touches up to millions of
conditions and speed and exact integer arithmetic matter. "C++20" is the language edition; it supplies features used below
(`std::popcount`, `constexpr`, structured bindings).

**Python 3** — an interpreted scripting language. *Behaviour:* code runs line by line with no separate compile step, which is
slower but quick to write. *Here:* Python never does the maths. It launches the C++ tool, collects its output, tabulates,
draws charts and checks results against predictions. That keeps one implementation of the mathematics.

**Bash** — the Unix shell scripting language (`demo.sh`). *Here:* glue that builds the tool and runs a guided tour.

**Markdown** — plain text with light formatting symbols (`#` headings, `|` tables). *Here:* all documents and the
pre-registered hypotheses.

**JSON** — a text format for structured data: `{"name": "value", "list": [1, 2]}`. *Here:* both the **input** (a system
described in `configs/satellite_support.json`) and the **output** (the tool's results) are JSON, so other programs can read them.

### 2.2 C++ ideas that appear in many files

**Header file (`.hpp`) and source file (`.cpp`) — "companions".** The header is the *contract*: it lists what a module offers
(function names, the data they take and return) plus the explanation. The source file is the *implementation*: the actual
steps. Other files only need to read the header to use the module, and the compiler checks that the source keeps the promise.
Every pair below (`state.hpp`/`state.cpp`, …) works this way. `#pragma once` at the top of a header means "include me at most
once per program". `namespace reliability { … }` puts all names in a labelled box so they cannot clash with other libraries.

**Bitmask (a state as one whole number).** Computers store numbers in binary. Here one 32-bit whole number describes the
condition of up to 31 components: bit *i* is 1 if component *i* works, 0 if it has failed. With six components, 63 (`111111`)
means everything works, 62 (`111110`) means component 0 has failed, 0 means everything has failed. *Behaviour:* operations on
bits (shift, AND, count the ones) are single processor instructions. *Here:* "does component i work?", "fail component i" and
"how many work?" cost almost nothing, and a whole system condition fits in one array index. This is why 2²⁰ ≈ a million
conditions are manageable. The type is `StateMask`.

**Fixed-size integers (`uint32_t`, `uint64_t`).** Unsigned whole numbers of exactly 32 or 64 bits, which never round.
*Here:* counting vectors are *counts*, so they use integers and are exact. Rounding only enters later, when a count is turned
into a probability.

**`double` and `long double`.** Decimal-like numbers with limited precision (about 15–16 digits for `double`; about 19 for
`long double` on this platform). *Behaviour:* every arithmetic step can round, and subtracting two nearly equal large numbers
can destroy most of the digits (**cancellation**). *Here:* MTTF and costs are doubles. The failure-model code uses
`long double` internally because one of the paper's formulas is an alternating sum of huge terms that cancel.

**`std::vector`** — a resizable list stored contiguously in memory. *Here:* lists of states, counting vectors, MTTF per state.
Contiguous storage matters for speed: adding two arrays element by element is much faster than chasing pointers.

**`std::unordered_map` and a hash** — a lookup table (key → value) that finds entries in roughly constant time by turning
the key into a number (a *hash*). *Here:* in `policy.cpp`, to group states whose counting vectors are identical (the key is the
vector). `VectorHash` is a small hashing recipe (FNV-1a style multiply-and-xor).

**Class, abstract interface, `virtual`, `override`, `final`.** An *abstract class* declares what any failure model must be
able to do without saying how. Concrete classes then provide that. At run time the program calls the same function names and
gets the behaviour of whichever model it was given (**polymorphism**). `override` asks the compiler to check the match;
`final` says no further variants. *Here:* `FailureModel` is the interface; `IndependentExponential` and `LevyFrailty` fill it
in. The MTTF and policy code is written once and works for every model.

**`std::unique_ptr` and factory functions.** A `unique_ptr` is an owning pointer that deletes the object automatically when it
goes out of scope, so there is no leaked or doubly freed memory. A *factory* (`make_lethal_shock(...)`) is a function that
builds and returns the right object. *Here:* the command line picks a model by name and hands the rest of the program a
`unique_ptr<FailureModel>`.

**`std::function` and lambdas (callbacks).** A lambda is a small unnamed function written inline; `std::function` is a
variable that can hold one. *Behaviour:* you pass "what to do" into code that decides "when to do it". *Here:* the sweep calls an
**observer** callback once per state, and the MTTF code simply supplies "store this state's MTTF". The sweep needs no knowledge of MTTF.
The structure function Φ itself is also stored as a `std::function`.

**Templates.** A template is a recipe from which the compiler writes several specialised versions of a function. *Here:*
`level_pass` in `policy.cpp` is the layer-by-layer engine, and each policy calculation supplies its own "visit" step. The compiler
builds a separate fast version for each, with no run-time overhead.

**Exceptions.** A way to stop and report a problem: code `throw`s an error object, and something further up `catch`es it.
*Here:* `ModelError` means "the user gave something invalid" and the command line turns it into a clear message and exit
status 1. `std::logic_error` means "the program itself is wrong" (for example a counting identity that must divide exactly does not).

**`const`, `constexpr`, `mutable`, `inline constexpr`.** `const` = "this does not change"; `constexpr` = "the compiler computes
this ahead of time"; `inline constexpr` = a named constant shared by the whole program. `mutable` is an escape hatch: the
`phi_calls_` counter in `SystemModel` can change even inside a `const` method, because counting calls is bookkeeping, not part of
the model's meaning.

**Compiler warnings (`-Wall -Wextra -Wpedantic`).** Flags that make the compiler complain about risky constructs. The project
requires zero warnings with both compilers.

### 2.3 Build, test and automation technologies

**Static library + executable.** `libreliability.a` is a bundle of compiled code with no `main` program; `analyze` is a program
that links to it; the test program links to the same bundle. *Here:* one tested engine, three users.

**CMake** — a build-system generator. *Behaviour:* reads `CMakeLists.txt`, works out which files to compile in which order for
the machine at hand, and produces the build. *Here:* defines the library, the tool, the tests, and registers the test suites.
**`FetchContent`** is a CMake feature that downloads a dependency (here nlohmann/json and GoogleTest) at configure time, so
nothing needs installing by hand.

**CTest** — CMake's test runner. It runs all registered tests and reports pass/fail. Here it runs the C++ tests one by one,
the whole Python suite as one test, and the traceability check as one test.

**nlohmann/json** — a header-only C++ library to read and write JSON (MIT licence). *Here:* reads the system configuration and
writes the analysis results.

**GoogleTest** — a C++ unit-testing framework. A `TEST(Group, Name) { … }` block runs code and uses `EXPECT_*`/`ASSERT_*` to
state what must be true; failures are reported with values. *Here:* 127 tests in 8 files.

**Sanitizers (AddressSanitizer, UndefinedBehaviorSanitizer).** Special compiler modes that watch the running program for memory
errors (reading past the end of an array, using freed memory) and undefined behaviour (signed overflow, bad shifts). The program
runs slower but silently-wrong behaviour becomes a crash with a message. Run in CI.

**Release vs Debug build.** Release enables optimisation (fast, used for timing and normal tests); Debug keeps information for
diagnosis (used with sanitizers).

**Git and `.gitignore`.** Git records history; `.gitignore` lists files that must never be tracked (build folders, caches,
generated results, the two third-party source documents).

**GitHub Actions and YAML.** Actions is GitHub's automation service. A YAML file (indentation-based settings) in
`.github/workflows/` says: on every push, start a clean Ubuntu machine, install tools, build, and run all tests. *Here:* the
"works on a fresh machine" evidence.

### 2.4 Python technologies

**Standard library:** `argparse` (command-line options), `subprocess` (launch the C++ tool and capture output), `json`,
`hashlib` (SHA-256 fingerprints), `pathlib` (file paths), `tempfile`, `random` (seeded random systems), `unittest` (tests).

**pandas** — tables ("data frames") with grouping, sorting and CSV output. **numpy** — fast array maths (pandas needs it).
**matplotlib** — drawing figures to image files. These three are only needed for tables and figures; tests that need them skip
themselves if they are missing.

### 2.5 Ideas from the paper that the code relies on (in plain language)

* **Structure function Φ.** A yes/no rule: given which components work, does the whole system work?
* **Counting vector W.** For a condition with w working components, `W[k]` = in how many ways can exactly *k* of those w
  components fail while the system still works? Satellite, all working: `[1, 6, 12, 8, 0, 0, 0]`.
* **Lemma 1 (the reuse trick).** `W[k]` of a condition equals the sum of `W[k−1]` over its **sons** (the conditions with one
  more component failed), divided by k. Each failure pattern is seen from exactly k sons, hence the division.
* **Signature.** The same counts turned into probabilities: the chance the system dies at the k-th failure.
* **MTTF (paper Eq. (7)).** Combine the survival chances with the average waiting time for each successive failure.
* **Simultaneous failures.** Several components can fail at the same instant (shared cause). Models that allow this differ in
  how big the failing bundle is. The *pairwise beta* β is the knob that makes models comparable.
* **Repair policy.** A rule for when to stop and replace failed components. Its **long-term mean cost** is the expected cost of one
  run-then-repair cycle divided by the expected cycle length.

---

## Part 3 — The engine: the library (`project/include/reliability` and `project/src`)

Reading order that builds understanding: `state` → `system_model` → `signature` (+ `level_index`) → `failure_model` → `mttf` →
`reference` → `policy` → `main`.

### 3.1 `state.hpp` / `state.cpp` — how a condition is represented

* **Companions.** The header defines the type `StateMask` (a 32-bit number, see *bitmask*) and the cheap one-line helpers
  (`working_count`, `all_working`, `is_working`, `without`) as `constexpr` functions, so they cost nothing. It only *declares*
  the three heavier functions. `state.cpp` contains their implementations.
* **`kMaxComponents = 24`.** The largest system the sweep accepts (decided on Day 2). A mask could hold 31, but 2²⁴ states is
  the practical memory limit.
* **`sons(x)`.** Loops over the bits; for each working one, returns `x` with that bit cleared. A condition with w working
  components has w sons. Used by tests and as the definition the faster code reproduces.
* **`masks_with_working(n, w)` and Gosper's hack.** The sweep needs "every n-bit number with exactly w ones". Scanning all 2ⁿ
  numbers and testing each would waste time. **Gosper's hack** is a short sequence of bit operations (find the lowest block of
  ones, push it up one place, drop the leftover ones to the bottom) that jumps straight from one such number to the next larger
  one. Starting from the smallest (`w` low ones) and repeating until 2ⁿ is passed lists exactly C(n, w) numbers in increasing
  order. It uses 64-bit arithmetic internally so the "+" cannot overflow when n = 31.
* **`binomial(n, k)`.** "n choose k", the number of ways to pick k of n. It is computed as `result × (n−k+i) / i` for i = 1…k.
  After each step the intermediate value is itself a binomial coefficient, so the division is always exact and no fractions or
  rounding occur. n above 60 is refused because the result would not fit in 64 bits.
* **Technologies:** bitmask, `std::vector`, `constexpr`, `std::popcount` (C++20: count the ones), exceptions (`out_of_range`).

### 3.2 `system_model.hpp` / `system_model.cpp` — the system and the yes/no rule Φ

* **Companions.** The header describes `Component` (id, human name, role, bit position) and the class `SystemModel`. The class
  offers *factory functions* to build a system: from a JSON file, as a series (all needed), parallel (any one enough),
  "AND of OR groups" (the satellite: every redundant pair must have at least one working unit), "OR of AND groups" (list of
  working paths), or from any user-supplied function (used by tests). It exposes `phi(x)`, the call count, and `validate()`.
  `system_model.cpp` holds the file reading, the checking and the construction.
* **What `validate()` enforces.** It checks the paper's assumptions: 2 ≤ n ≤ 24, "everything failed" ⇒ system down,
  "everything working" ⇒ system up, and **monotone** (failing one more component can never repair the system). It checks
  every state and every single-component step, and resets the call counter afterwards so that validation does not pollute the
  count the sweep later verifies.
* **The call counter.** `phi()` increments `phi_calls_` (a `mutable` field). The paper claims the whole sweep asks Φ exactly 2ⁿ
  times (Theorem 2), and the code checks that claim on every run.
* **`std::function`.** The structure function is stored as a `std::function<bool(StateMask)>`, so all the factory kinds look the
  same to the rest of the engine.
* **JSON reading.** `build_from_json` uses *nlohmann/json* and turns every problem into a `ModelError` that names the field
  ("missing required field 'bit' in components[2]"). It checks duplicate ids, duplicate or out-of-range bits, unknown ids in the
  structure, and unknown structure types. Components are then sorted by bit so `components[i].bit == i`.
* **Technologies:** JSON parsing, `std::function`, exceptions, `unordered_map` (id → bit), `mutable` counter.

### 3.3 `signature.hpp` / `signature.cpp` and the private `level_index.hpp` — the sweep (Algorithm 1)

This is the heart of the project.

* **Companions.** `signature.hpp` states the idea in comments, defines `CountingVector` (a list of unsigned integers),
  `SweepResult` (vectors if asked, minimal path sets, the Φ call count, memory evidence, operation counts) and declares
  `sweep`, `tail_signature`, `signature`. `signature.cpp` implements them. `level_index.hpp` is a **private helper header**
  (it lives in `src/`, not `include/`, so users of the library cannot see it; only `signature.cpp`, `policy.cpp` and one test
  include it).
* **What `sweep` does, step by step, in plain language.**
  1. For w = 0, 1, …, n (fewest working components first), list all conditions with w working components
     (`masks_with_working`).
  2. For each such condition x, `W[0]` is just Φ(x): 1 if the system works, else 0. This is the **only** place Φ is asked.
  3. For each k from 1 to w, `W[k]` = (sum, over x's sons, of the son's `W[k−1]`) ÷ k. The sons are one layer down, already
     computed. The division must come out exact (Lemma 1); if not, the code throws `logic_error`, a cheap built-in bug detector.
  4. Hand `(x, W)` to the **observer** callback (the MTTF code), note x as a **minimal path set** if `W[0]=1` and `W[1]=0` (the
     system works but every single further failure kills it), and move on.
  5. Throw away the older layer. Only two layers exist at any moment, which is why memory scales with the biggest layer rather than 2ⁿ.
  6. At the end, check that Φ was called exactly 2ⁿ times.
* **Why `level_index.hpp` exists.** To find the sons' data in the previous layer quickly you need each state's position in its
  layer without a lookup table. The **combinatorial number system** gives it by formula. If a state's working bit positions are
  c₁ < c₂ < … < c_w, its rank is C(c₁,1) + C(c₂,2) + … + C(c_w,w). That is exactly the position it has when layers are listed in
  increasing numeric order (the order Gosper's hack produces). `BinomialTable` is a precomputed Pascal's triangle for speed.
  `son_ranks` computes the ranks of *all* sons at once in O(w) time: failing the i-th working component leaves the earlier terms
  unchanged and shifts the later ones down by one place, so prefix sums and suffix sums give every son's rank in two short
  passes. This replaced a per-son recomputation and is why the sweep and the policy calculations became faster.
* **Memory detail.** Counts are stored as 32-bit integers (the largest possible entry for n ≤ 31 is under 300 million), which
  halves the memory of the two live layers.
* **Contiguity.** The inner loop adds one son's row into a running `totals` array element by element. Because each row is stored
  contiguously this is efficient for the processor's memory cache.
* **`tail_signature` and `signature`.** Turn a counting vector into probabilities. `tail_signature[k] = W[k] / C(w,k)`
  (the chance of surviving k failures). `signature[k] = tail[k−1] − tail[k]` (the chance the system dies exactly at failure k).
  The signature is computed from an integer numerator over a product of binomials, in `long double`, so each entry rounds once
  and the entries add up to 1 to machine precision.
* **Does not require monotone Φ.** The sweep is pure counting, so it is also correct for arbitrary yes/no functions. The tests
  exploit this to check it on random functions.
* **Technologies:** `std::vector`, `std::function` (observer), 32/64-bit integers, `long double`, exceptions, Gosper's hack,
  combinatorial number system.

### 3.4 `failure_model.hpp` / `failure_model.cpp` — how fast components fail

* **Why it exists.** The sweep only knows the *shape* of the system. To get a time you must say how components fail.
  The paper's models all fit one frame: components are interchangeable, and the number still working only goes down. Such a
  model is fully described by three functions of the number of working components w:
  * `exit_rate(w)` = ν_w: how often the next failure *event* happens;
  * `transition_prob(w1, w2)` = P: if an event happens, the chance it leaves w2 working (a drop of 2 or more is a simultaneous failure);
  * `increment(w, k)`: the average extra waiting time between the k-th and (k+1)-th component failure.
  The MTTF code needs only `increment`; the other two are used for cross-checks and the policy calculations.
* **Companions.** The header holds the abstract interface `FailureModel`, the two concrete classes, the size limit
  `kMaxLevyFrailtyComponents = 16`, the Laplace exponents Ψ of the paper's Table I as free functions, and the `make_*` factories.
  `failure_model.cpp` implements the constructors, the numerical work, and the factories.
* **`IndependentExponential`** ([Exp]): each component fails alone at rate λ, so `increment = 1/(λ(w−k))`. Simple closed forms.
* **`LevyFrailty`** (all the common-cause models): described by one function Ψ(x), the *Laplace exponent*. From Ψ:
  ν_w = Ψ(w); the kernel P from paper Eq. (20) (an **alternating sum**); the increments via a second route that uses only
  positive terms (the recursion after Proposition 3). The constructor computes everything once in `long double` and **refuses
  the model** if the numbers cannot be trusted: Ψ not positive, not increasing, not calibrated (Ψ(1) must be 1), any row of
  the kernel not summing to 1, or any entry meaningfully negative. This guard is what limits n to 16 for these models.
  `increment_eq21` keeps the paper's other formula (with its lower limit corrected by one) so tests can compare both routes.
* **Why `long double`, `log1p`, `expm1`.** Eq. (20) adds and subtracts huge terms; precision is lost roughly like 2ⁿ.
  The wider number type pushes the safe limit to n = 16 (error about 2e-10). `log1p(x)` and `expm1(x)` are library functions that
  give accurate results for tiny x where `log(1+x)` and `exp(x)−1` would lose digits; they appear in the gamma and BFR models.
* **Solving for η by bisection.** For the gamma and inverse-Gaussian models, the model parameter η is not given directly; it
  must satisfy Ψ(2) = 2 − β. `solve_eta` finds it by **bisection** (repeatedly halve an interval that must contain the answer),
  which is simple and cannot fail to converge for a monotone function.
* **Calibration.** Every model is scaled so Ψ(1) = 1 (each component has mean life 1) and its *pairwise β* = 2 − Ψ(2). That makes
  the six models comparable at equal β, which Experiment 6 uses.
* **Technologies:** abstract class and overrides, `unique_ptr` factories, `std::function<double(int)>` for Ψ, `long double`,
  flat vectors indexed by `(w, k)`, exceptions as the validation guard.

### 3.5 `mttf.hpp` / `mttf.cpp` — turning counts into times

* **Companions.** The header explains Eq. (7) and declares two functions. `mttf.cpp` implements them in about 30 lines.
* **`mttf_from_counting_vector(W, fm)`.** If `W[0] = 0` the system is down: MTTF 0. Otherwise add up, for k = 0 … w−1,
  `tail_signature[k] × fm.increment(w, k)`. In words: the chance the system is still alive after k failures, times the average
  time the next failure takes. Every term is ≥ 0 so nothing cancels.
* **`compute_all_mttf(model, fm)`.** Runs the sweep with an observer lambda that stores each state's MTTF in an array indexed by
  the mask. This is the "one sweep, MTTF at every state" result.
* **Role in the system.** This file is the glue between the *shape* (signature) and the *speed* (failure model). Nothing here is
  new mathematics.

### 3.6 `reference.hpp` / `reference.cpp` — the independent second opinion

* **Why it exists.** If a fast algorithm and its checker share an idea, they can share a bug. So this module is forbidden to
  use `sweep`, `sons`, or the recurrence. It does everything the slow, obviously correct way.
* **Companions.** The header lists three functions and the limit `kMaxBruteForceComponents = 16` (the work is about 3ⁿ, so larger
  systems are refused rather than running for hours). The source implements them.
* **`brute_force_counting_vectors`.** For each state, tries every subset of its working components, clears them, asks Φ, and counts.
  Subsets are enumerated by a plain counter, so the code looks nothing like the paper's recurrence.
* **`brute_force_minimal_path_sets`.** A working state is minimal if **every** smaller state (not just its sons) fails. The
  loop `y = (y − 1) & x` is a standard bit trick to walk through all sub-masks of x in decreasing order.
* **`solve_mttf_direct`.** Solves the paper's defining equation (6) state by state, in increasing numeric order (every
  sub-state is a smaller number, so it is already solved). It never touches counting vectors, so agreement with the sweep-based
  Eq. (7) is real evidence.
* **Used by:** `test_reference.cpp` (first, to prove the checker itself is right), the other tests, and `analyze --verify`.

### 3.7 `policy.hpp` / `policy.cpp` — when to repair, and what it costs

* **Why it exists.** Paper §V: after observing a partly failed condition, an operator may keep running or replace the failed
  components. Because a repair returns the system to "all working", operation is a sequence of independent **cycles**, and the
  long-term mean cost is *expected cycle cost ÷ expected cycle length*.
* **Companions.** The header defines the data types (`RepairCosts`, `RepairSet` = a 0/1 array "repair at this state?",
  `PolicyEvaluation`, `PolicyClassTable`, the result structs), the limits (`kMaxPolicyComponents = 20`,
  `kMaxEnumeratedPolicyComponents = 14`), and every function with a long explanation. `policy.cpp` implements them.
* **Four policy families.** Corrective (repair only when the system has failed); r-out-of-n (repair after r component failures);
  MTTF-threshold (repair when MTTF falls below a fraction γ of the new-system MTTF); and the **optimal state-based policy**
  (the cheapest rule of any kind).
* **Two ways to evaluate a policy (each is the other's check).**
  * *By enumeration* (`evaluate_policy_by_enumeration`): for each state add up the contributions of all its sub-states, about 3ⁿ work.
    Simple, slow, the independent check, n ≤ 14.
  * *By level sums* (`evaluate_policy`): the same layer-by-layer reuse trick as Lemma 1, but applied to sums of values over
    sub-states instead of counts. `S_x[j]` = the sum of a value over all sub-states of x with j fewer working components, and
    `S_x[j] = (1/j) × Σ over sons S_son[j−1]`. This reduces the work to about n²·2ⁿ and allows n ≤ 20.
* **`level_pass` (template).** The engine for level sums. It is written once; each calculation supplies a small "visit" function
  saying what to compute at a state from its sums. The template lets the compiler build a specialised copy for each use.
  It shares `son_ranks` with the sweep.
* **`build_policy_classes` and Proposition 3.** The paper's Proposition 3 shows that all MTTF-threshold policies can be costed in
  **one pass** if states with the *same counting vector* are grouped into **classes** (they contribute identical amounts). A hash map
  keyed by the counting vector does the grouping during the sweep; there are typically far fewer classes than states. For each class
  the code stores its size, MTTF ratio, and three sums (time spent, failures that happen, chance the next event fails the system).
  Sorting classes by MTTF and taking running totals then gives the cost of **every** threshold at once.
* **`search_threshold_policies`, `evaluate_r_out_of_n_policies`.** Both read the same class table. r-out-of-n works because its
  "keep running" sets are *up-closed* (if you keep running at a state, you also keep running at any state with more components
  working), the only property Proposition 3's proof needs. This sharing was the main speed-up of the latest change.
  The code evaluates **every** threshold breakpoint because the paper shows the cost curve over γ is not single-humped, so a search
  that stops when cost first rises can miss the best threshold.
* **`optimal_state_policy` (Dinkelbach's method).** Minimising a *ratio* (cost ÷ time) is hard directly. Dinkelbach's idea: guess a
  cost rate g, find the policy minimising "cost − g × time" (easy state by state: repair if the immediate repair cost is cheaper
  than the expected continuation), evaluate that policy to get a new, lower g, and repeat until g stops falling. Each pass uses
  level sums. Ties are resolved toward *not* repairing.
* **Technologies:** `unordered_map` with a custom hash, templates, `std::vector<uint8_t>` as a compact yes/no array,
  `double`, exceptions for invalid costs and sizes.
* **Monotone MTTF assumption.** The threshold shortcut assumes MTTF never increases when more components fail (paper Lemma 4).
  It holds for all models here; the slower enumeration version does not need it.

### 3.8 `errors.hpp` — one small header

Defines `ModelError`, a thin subclass of the standard `runtime_error`. Everything the user can get wrong raises it, with a message
that names the field or value and the rule broken. The command-line tool maps it to exit status 1. It is header-only because it has
no behaviour to implement.

### 3.9 `main.cpp` — the command-line tool `analyze`

* **No companion header.** `main.cpp` is the program's entry point and is compiled into the `analyze` executable, not the library.
  Nothing else includes it.
* **What it does.** Parse options (`--config`, `--model`, `--lambda`, `--beta`, `--per-state`, `--verify`, cost options, `--out`);
  load and validate the system; pick the failure model (command line overrides the file); run the sweep and MTTF; optionally verify
  against the reference; optionally compare repair policies; write JSON. Exit codes: 0 success, 1 invalid input, 2 usage error,
  3 verification failed (output still written).
* **`--verify`.** Brute-force counting vectors must equal the sweep's on every state; Eq. (6) must equal Eq. (7) within a tolerance;
  MTTFs are finite and non-negative; signatures sum to 1; Φ was called exactly 2ⁿ times; minimal path sets match the definition.
* **Two outputs per run.** Deterministic results and a separate `"timing"` section (run times, memory) so the rest of the file can be
  compared byte for byte between runs.
* **Peak memory (`peak_rss_kb`).** Reads `VmHWM` from `/proc/self/status` (Linux's record of this process's highest memory use).
  The more familiar `getrusage` value is inherited from the launching process and misreported the Python parent's memory, so it is not used.
* **A test-only switch.** The environment variable `ANALYZE_FAULT_INJECTION`, honoured only together with `--verify`, deliberately
  corrupts a result so the tests can prove that verification detects errors.
* **Technologies:** command-line parsing (hand-written), `<chrono>` for timing, nlohmann/json output, `std::optional` ("value or
  nothing") for options not given, `unique_ptr` for the chosen model, exit codes.

---

## Part 4 — Configuration, build and automation

### 4.1 `project/configs/satellite_support.json` — the reference system

A JSON file with: six components (id, human name, role, bit position); a structure of type `and_of_or_groups` with three groups
`[P1,P2]`, `[C1,C2]`, `[K1,K2]` meaning "a power unit AND a communications unit AND a computer must work"; and a default failure model
(`independent_exponential`, λ = 1). It is *data*, not code, so a different system needs no recompilation. The tests also load it
via a compile-time path (`RELIABILITY_CONFIG_DIR`). It describes a **fictional** satellite (NFR-008).

### 4.2 `project/CMakeLists.txt` — the build recipe

* Requires CMake 3.20 and C++20 with extensions off (standard-only code).
* Downloads nlohmann/json 3.11.3 and GoogleTest 1.14.0 with `FetchContent`.
* Builds `libreliability` from seven `.cpp` files (everything except `main.cpp`); builds `analyze` from `main.cpp`; builds the single
  test program `reliability_tests` from eight test files.
* Turns on strict warnings (`-Wall -Wextra -Wpedantic`); defines `RELIABILITY_VERSION` for the tool and `RELIABILITY_CONFIG_DIR` for the tests.
* Makes `src/` visible to the test program only, because one test checks the private helper `level_index.hpp`.
* Registers `python_end_to_end` and `traceability` as CTest tests (only if Python is found), passing the paths of the built tool through
  environment variables.
* *Note:* its header comment still says GoogleTest vs Catch2 is "pending"; the decision was made on Day 2 (see Part 8).

### 4.3 `.github/workflows/ci.yml` — automatic checking on every push

Two jobs on a pinned Ubuntu 24.04 image: **build-and-test** (run with g++ and with clang++, Release) and **sanitizers** (Debug with
AddressSanitizer + UndefinedBehaviorSanitizer). Each job checks out the code, installs Python and its packages, configures, builds and runs
`ctest`. The two compilers catch different mistakes; the sanitizers catch memory and undefined-behaviour errors the normal tests cannot see.

### 4.4 `.gitignore` (root) and `project/.gitignore`

Root: local editor/tool settings and the two source documents (the paper PDF and the project guide) that belong to someone else and are kept locally
only. `project/`: build output, caches, and experiment result files (regenerated by the scripts); `results/.gitkeep` is an empty marker file that
keeps the otherwise-empty `results/` folder in git.

### 4.5 `project/demo.sh` — a two-minute guided tour

A Bash script: builds the tool if needed, then analyses the satellite and verifies it, shows two states with the same number of failures that
have different MTTFs, shows what common-cause failure does to the benefit of redundancy, runs a 20-component system to show the cost of exactness,
and compares repair policies. It builds its test systems by running small embedded Python snippets that write JSON files, and cleans up its temporary
directory on exit. `docs/demo.md` explains it.

---

## Part 5 — The tests

All eight C++ test files are compiled into one program, `reliability_tests`, using **GoogleTest**. A comment tag such as `// Traceability: TC-C-23`
links a test to the test plan; `tools/check_traceability.py` reads those tags (Part 6.4).

| File | Tests | What it protects, and why it matters |
|---|---|---|
| `tests/test_state.cpp` | 9 | The bit helpers, Gosper's hack (right counts, increasing order, edge cases), `binomial` (including exactness and the overflow limit), `sons`, and the private `son_ranks` helper (every state up to 12 components). If this layer is wrong, everything above is. |
| `tests/test_structure_function.cpp` | 13 | Φ: truth tables of series/parallel/the satellite (27 working states of 64), monotonicity checking, JSON loading and every kind of invalid file (duplicate ids, bad bits, unknown types), the call counter. |
| `tests/test_reference.cpp` | 16 | The brute-force checker is tested **first**, against hand-computed counting vectors and closed forms, so it can then be trusted to judge the clever algorithm. |
| `tests/test_signature.cpp` | 23 | The sweep against brute force on every state, exactly 2ⁿ Φ calls, memory evidence (two layers), operation counts against the paper's formulas, known vectors, signatures sum to 1, minimal path sets, random systems including non-monotone Φ. |
| `tests/test_failure_models.cpp` | 22 | Each model's parameters and calibration (Ψ(1) = 1, β = 2 − Ψ(2)), kernel rows sum to 1, the two increment routes agree, the corrected Eq. (21) vs the printed one (the test proves the printed limit is wrong), refusal of bad parameters and oversized models, Table I formulas. |
| `tests/test_mttf.cpp` | 21 | MTTF against closed forms (series = 1/(nλ), 1-out-of-2 = 1.5, satellite = 0.7 and 0.5333 with P1 failed), against the independent Eq. (6) solver on every state, the paper's Fig. 1 example, the models agreeing where they must. |
| `tests/test_policy.cpp` | 18 | Level-sum evaluation vs enumeration; the paper's Fig. 1 costs; the exhaustive search over **all** state-based policies matching the Dinkelbach optimum; optimal never worse than the other families; every threshold breakpoint evaluated and the non-unimodal cost curve; closed forms; size limits; r-out-of-n from the class table. |
| `tests/test_properties.cpp` | 5 | Things the paper *proves*, checked on the code: MTTF never rises when components fail, signature dominance implies MTTF dominance (but not conversely), minimal-path-set criterion, same failure count with different MTTF. |

**Mutation-style checks.** Beyond these files, the verification report describes deliberately breaking the code (a "mutation") and confirming that
a test fails. That demonstrates the tests can actually detect errors.

---

## Part 6 — Python: running, analysing, plotting and checking

Python is used for **orchestration**, not mathematics. Every number in a table or figure comes from `analyze`.

### 6.1 `python/run_experiments.py` — run the experiments

* **Idea.** Seven experiments (redundancy, common cause, equal failure counts, scaling, repair policies, six models, distance to optimal policy).
  Each builds a set of configurations, calls `analyze` with `subprocess`, and saves exactly what came back plus the settings.
* **The pre-registration gate.** An experiment refuses to run unless `experiments/hypotheses/<name>.md` exists. The runner copies it into the
  results, records its **SHA-256 fingerprint** (a short code that changes if one character of the file changes) and whether the file was last
  modified before the run began. This makes "predictions were written first" checkable.
* **Two result files.** `<name>.json` (deterministic) and `<name>.timing.json` (times, memory, timestamps, platform). Re-running should reproduce
  the first byte for byte.
* **Random systems** (Experiments 4–7) use a seeded generator, recorded in the results, so they are repeatable.
* **Technologies:** `argparse`, `subprocess`, `hashlib`, `random`, `tempfile`, `statistics`, JSON.

### 6.2 `python/analyze_results.py` — tables and verdicts

Reads only the saved result files (never runs the C++ tool). For each experiment it builds tables with pandas, writes CSV files, and evaluates
every prediction made beforehand, writing a `hypothesis_check.md` with SUPPORTED or REJECTED for each. Rejected predictions are reported as rejected.
It also contains an **independent Python version of the Laplace exponents** (`psi_independent`) used to check the C++ models' calibration from a second
implementation (Experiment 6). It refuses result files that lack the traceability fields (settings, seed, tool version, hypothesis hash).

### 6.3 `python/plot_results.py` — figures from tables only

Reads the CSV tables and draws seven figures with matplotlib. Follows the project's data-visualisation rules:
colour-blind-validated palette (blue `#2a78d6`, orange `#eb6834`), never a second y-axis, a legend whenever there are two or more series, thin marks,
light background. It never runs the C++ tool, so figures can be regenerated from saved tables alone.

### 6.4 `tools/check_traceability.py` — the requirements-to-tests audit

Reads `docs/requirements-spec.md`, `docs/test-plan.md` and the `TC-…` tags in the test code and fails if anything is orphaned: a requirement with no
test case, a test case not defined in the plan, a defined test case with no tag (unless explicitly classed as verified another way, with a reason), or a
test case defined twice. With `--write` it regenerates `docs/traceability-matrix.md`. It runs as a CTest test, so a gap breaks the build. It handles shorthand
tags such as `TC-A-02/03` and `TC-D-01 to TC-D-05`. Technologies: regular expressions, `pathlib`, `argparse`.

### 6.5 `python/requirements.txt`

Three lines: numpy, pandas, matplotlib. Used by CI (`pip install -r`) so no Python test is skipped.

### 6.6 `python/tests/` — end-to-end tests of the whole pipeline

* `support.py` — shared helpers: locates the built tool (from the `ANALYZE_BIN` environment variable), runs it, parses JSON, strips the timing section.
* `test_cli.py` (24 tests) — drives the real `analyze` program: help text, defaults, every option, every error and its exit status, the `--verify`
  path including the fault-injection switch, repair-policy output, size limits.
* `test_experiments.py` (19 tests) — runs the experiment pipeline end to end on small settings: the hypothesis gate, the two result files, determinism,
  and that the verdict files and tables are produced.
* `test_traceability_tool.py` (5 tests) — tests the checker itself on small made-up documents.
* These use the standard library's `unittest`; tests that need pandas or matplotlib skip themselves if those are missing.

---

## Part 7 — Documents and non-code files

### 7.1 `project/experiments/hypotheses/exp1…exp7_*.md`

One file per experiment, written **before** that experiment's code or first run. Each has a question, a design, numbered predictions, "how it would be
rejected", and limits stated in advance. They are not edited after the first run. The experiment runner checks they exist and fingerprints them.

### 7.2 `README.md` (root), `project/README.md`, `docs/README.md`

Entry points: what the project is, how to build and run, current numbers, and an index of the documents. The root README carries the CI status badge.

### 7.3 The documents in `docs/`

| File | Systems-engineering role | What is in it |
|---|---|---|
| `user-stories.md` | stakeholder needs | Who wants what, in "as a … I want … so that …" form |
| `requirements-spec.md` | requirements | Numbered, testable requirements (SYS, ALG, REL, VER, EXP, DOC, INT, NFR) |
| `project-plan.md` | plan | Day-by-day schedule, milestones, progress table |
| `notation-glossary.md` | glossary | Paper's symbols mapped to code names |
| `paper-validation-report.md` | validation | Checks of the paper's claims, including the Eq. (21) correction and the precision analysis |
| `test-plan.md` | test strategy | Every test case (TC-x-nn), its purpose, and a requirement-to-test matrix |
| `traceability-matrix.md` | traceability | **Generated** by `check_traceability.py` |
| `verification-report.md` | verification | Evidence: test counts, compilers, CI runs, timings, requirement-by-requirement status |
| `experiment-report.md` | validation / results | Results and verdicts of the seven experiments, including rejected predictions |
| `deviation-log.md` | deviation log | Every departure from the paper or project guide, each with kind and reason |
| `technical-report.md` | summary | The whole effort for a technical reader |
| `demo.md` | user guide | Explains `demo.sh`, with a captured transcript |
| `file-guide.md` | this guide | |
| `conops.md` | concept of operations | Needs, operating modes, nominal and off-nominal scenarios, risks (NASA SEH App. S outline) |
| `software-design-description.md` | design | Design decisions, architecture and per-module design, traced to requirements |
| `interface-design-description.md` | interfaces | Config file, command line and exit codes, result JSON, library API, Python-to-CLI contract |
| `decision-log.md` | architecture decisions | Nine decision records (MADR 4.0.0), written retrospectively from the history |
| `configuration-management-plan.md` | configuration management | Configuration items, baselines, change control, audits, open gaps |
| `version-description.md` | release | What version 0.8.0 contains, how to install it, known defects |
| `sample-output/` | evidence | Captured output of `demo.sh`, the test run, the experiment verdicts and one analysis (JSON) |
| `figures/`, `uml/` | diagrams | Generated figures; UML sources with rendered images, and the SysML block definition diagram with the script that draws it |

Not published: `docs/paper3.pdf` and the project-guide `.docx` (third-party material; ignored by git).

---

## Part 8 — Out-of-date comments that were found and fixed

While writing this guide I found comments and document statements that no longer matched the code. They were corrected on
2026-10-27 (comments and documents only; no behaviour changed, and all 129 tests still pass):

* `project/CMakeLists.txt`, `system_model.hpp`: said the test-framework and JSON-library choices were pending; both were decided on Day 2.
* `state.hpp`: described the precision limit as "about n = 12"; the actual limits are 16 for the Levy-frailty models and the brute-force reference.
* `mttf.cpp`: named only [Exp] and [LSh] for the monotonicity property; it holds, and is tested, for all seven model types.
* `main.cpp`: the usage comment listed three models and no cost options.
* `failure_model.cpp`, `test_policy.cpp`: file headers did not mention the later models and tests.
* `run_experiments.py`, `plot_results.py`, `analyze_results.py`: said "four" experiments or figures, and that no random numbers are drawn (Experiment 7 draws seeded random systems).
* Documents: "four experiments / four figures", "GoogleTest or Catch2", the model count in deviation entry A5, the models named in the verification report's REL-002/REL-004 row, and a few similar statements.
* Left as written on purpose: dated records of what was measured or planned on a given day (for example the day-by-day plan and the early fresh-clone run).

## Part 9 — Questions this guide is meant to let you answer

1. *Why is a state a number?* So one condition fits in one machine word and "which components work" is a handful of bit operations.
2. *Why does the sweep go layer by layer?* Lemma 1 lets each layer be computed from the previous one, so work per state is small and only two layers are in memory.
3. *Why are there two implementations of almost everything?* Independent routes catch errors a single route cannot.
4. *Why is the failure model an interface?* The MTTF and policy code is written once and works for every model.
5. *Why is Python separate?* It orchestrates and analyses but never re-implements the maths, so every number traces to one C++ implementation.
6. *Why hash states into classes in `policy.cpp`?* States with identical counting vectors cost exactly the same, so grouping lets one pass price every threshold policy.

---

## Part 10 — Added after this guide was written (2026-11-03)

* **`project/run_all.sh`** — one command that creates a Python virtual environment, builds, runs all tests, analyses the satellite under three failure models,
  runs the seven experiments, prints the tables and verdicts and draws the figures (about 2.5 to 6 minutes). Options: `--skip-tests`, `--skip-experiments`.
  It stops at the first failure; a REJECTED prediction is a result, not a failure. Like `demo.sh`, it works from its own folder, so it can be started from anywhere.
* **`docs/uml/sysml_bdd.py`, `sysml-bdd.png`, `sysml-bdd.svg`** — a SysML block definition diagram of the analyzer, drawn with matplotlib because
  PlantUML has no SysML profile. Its blocks mirror the modules described in Part 3.
* **`docs/sample-output/`** — real captured output (see §7.3).
* **The documents** added to §7.3 above: concept of operations, software and interface design descriptions, decision log, configuration-management plan and version description.
* **The root `README.md`** was rewritten for newcomers: plain-language introduction, step-by-step setup per operating system, output snippets and a figure gallery.
