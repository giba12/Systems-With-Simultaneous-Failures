# Interface Design Description — Space System Reliability Analyzer

| | |
|---|---|
| **Version** | 1.0 |
| **Date** | 2026-10-29 |
| **Template** | DI-IPSC-81436A, *Interface Design Description* (the software-oriented IDD listed in the template catalog), tailored: sections 1 – 5 of the DID are kept; the DID's per-interface "priority / type / data-element / communication-method" headings are folded into the tables below. This tailoring was written from the DID's content structure, not from a fresh copy of the DID; confirm the current revision in the DLA ASSIST database before any contractual use |
| **Describes** | `analyze` 0.8.0 (`RELIABILITY_VERSION` from `CMakeLists.txt`), commit `b662153` |
| **Related** | [requirements-spec.md](requirements-spec.md) §4 (INT-001 – INT-004) · [software-design-description.md](software-design-description.md) · [conops.md](conops.md) · [test-plan.md](test-plan.md) |

## 1. Scope

### 1.1 Identification
Interfaces of the Space System Reliability Analyzer (CSCI: library `reliability`, executable `analyze`, Python layer).

### 1.2 System overview
A C++20 library and command-line tool that compute the signature and MTTF of every state of a coherent binary system, under
independent or common-cause failures, and compare repair policies. A Python layer runs pre-registered experiments by calling
the tool. See [conops.md](conops.md) §1.2.

### 1.3 Document overview
Section 3 specifies each interface. Section 4 traces them to the requirements INT-001 – INT-004 and to test cases.
The fictional satellite system is an educational model, not a flight-qualified design.

## 2. Referenced documents

- [requirements-spec.md](requirements-spec.md) §4, §5 (NFR-006, NFR-009)
- [software-design-description.md](software-design-description.md)
- `project/src/main.cpp` (header comment: usage, exit codes), `project/include/reliability/*.hpp`
- `project/python/run_experiments.py`, `analyze_results.py`, `plot_results.py`
- Lagos & Ávila, arXiv:2610.22181 (equation numbers)

## 3. Interface design

### 3.1 Interface identification and diagram

| ID | Interface | Between | Medium | Direction |
|---|---|---|---|---|
| IF-1 | System-definition file | analyst → `analyze`, tests, experiments | JSON file | in |
| IF-2 | Command line | analyst / Python layer / CI → `analyze` | argv, stdout, stderr, exit status | in / out |
| IF-3 | Result file | `analyze` → analyst / Python layer | JSON on stdout or `--out` file | out |
| IF-4 | C++ library API | `analyze`, tests ↔ `reliability` | headers in `project/include/reliability/` | call |
| IF-5 | Python → CLI contract | `run_experiments.py` → `analyze` | subprocess, JSON | call |
| IF-6 | Hypothesis file | analyst → `run_experiments.py` | Markdown file | in |
| IF-7 | Experiment result files | `run_experiments.py` → `analyze_results.py` → `plot_results.py` | JSON, CSV, Markdown, PNG | file chain |
| IF-8 | Requirements ↔ tests tags | test code, `test-plan.md`, `requirements-spec.md` → `check_traceability.py` | `TC-x-nn` tags in comments, Markdown tables | in |
| IF-9 | External: dependency download | CMake FetchContent → GitHub | HTTPS | in |
| IF-10 | External: CI | GitHub → `.github/workflows/ci.yml` | GitHub Actions | in / out |

```
 IF-1 config ─┐                                   ┌─► IF-3 result ──► IF-5/IF-7 Python layer ─► tables, verdicts, figures
              ├─► IF-2 analyze (CLI) ─ IF-4 ─► library ┤
 flags/env ───┘                                   └─► stderr + exit status 0 | 1 | 2 | 3
 IF-6 hypothesis ─► run_experiments.py (refuses without it)
```

### 3.2 IF-1 — System-definition file (INT-003, SYS-003)

JSON object. Reference instance: [project/configs/satellite_support.json](../project/configs/satellite_support.json).

| Field | Type | Required | Rule |
|---|---|---|---|
| `name` | string | no | echoed in the result as `system.name` |
| `description` | string | no | ignored by the tool |
| `components` | array of objects | **yes** | 2 ≤ n ≤ 24 (`kMaxComponents`); n beyond a model's own limit is rejected by that model |
| `components[].id` | string | **yes** | unique |
| `components[].bit` | integer | **yes** | 0 ≤ bit < n, unique; position in the state mask (bit = 1 means working) |
| `components[].name`, `.role` | string | no | labels only |
| `structure` | object | **yes** | |
| `structure.type` | string | **yes** | one of the four below |
| `structure.groups` | array of arrays of ids | for `and_of_or_groups` | Φ = AND over groups of (OR within the group); a group may not be empty |
| `structure.paths` | array of arrays of ids | for `path_sets` | Φ = OR over paths of (AND within the path); a path may not be empty |
| (none) | | for `series`, `parallel` | `series`: every component is its own group; `parallel`: one group of all |
| `failure_model` | object | no | default for the failure model; command-line flags win (see IF-2) |
| `failure_model.type` | string | no | `independent_exponential`, `lethal_shock`, `binomial_failure_rate`, `compound_poisson`, `gamma_process`, `inverse_gaussian`, `stable_subordinator`; default `independent_exponential` |
| `failure_model.lambda` | number > 0 | no | rate of the exponential model, default 1 |
| `failure_model.beta` | number | for the six common-cause types, unless `--beta` is given | pairwise β = 2 − Ψ(2); ranges below |

Validation after loading (`SystemModel::validate`): Φ(0) = 0, Φ(1) = 1 and Φ monotone; otherwise a `ModelError` names the offending state. An unknown id in a group or path, a duplicate id or bit, a missing field, a wrong type, a non-object file, or malformed JSON is also a `ModelError` naming the field.

β ranges: `lethal_shock`, `compound_poisson`, `stable_subordinator` [0, 1]; `binomial_failure_rate` (0, 1]; `gamma_process` (0, 1); `inverse_gaussian` (0, 2 − √2 ≈ 0.5858].

### 3.3 IF-2 — Command line (INT-001, INT-004)

```
analyze --config <file.json> [--model exp|lsh|bfr|lfmo-c|lfmo-g|lfmo-ig|lfmo-s] [--lambda L] [--beta B]
        [--c-comp C --c-sys C --c-set C] [--per-state] [--verify] [--out <file.json>]
```

| Flag | Argument | Meaning |
|---|---|---|
| `--config` | path | **required**; IF-1 file |
| `--model` | `exp`, `lsh`, `bfr`, `lfmo-c`, `lfmo-g`, `lfmo-ig`, `lfmo-s` | maps to `independent_exponential`, `lethal_shock`, `binomial_failure_rate`, `compound_poisson`, `gamma_process`, `inverse_gaussian`, `stable_subordinator`; overrides the file's `failure_model.type` |
| `--lambda` | number | exponential rate (default: file, else 1) |
| `--beta` | number | β of the common-cause models; required for them if the file has none |
| `--c-comp`, `--c-sys`, `--c-set` | numbers ≥ 0 | repair costs; **all three or none**; all three add `policies` to the result |
| `--per-state` | – | add one record per state to `results.states` |
| `--verify` | – | run the independent checks |
| `--out` | path | write the result here instead of stdout |

Environment: `ANALYZE_FAULT_INJECTION=counting_vector|mttf` corrupts one value, and acts **only** together with `--verify`; it exists to test the failure path (TC-H-06). Any other value is an input error.

**Exit status and streams**

| Status | Meaning | stderr | Result file |
|---|---|---|---|
| 0 | success (including a skipped verification) | – | written |
| 1 | invalid input or model (`ModelError`, unwritable output) | `analyze: <message naming the field/value and the rule>` | not written |
| 2 | usage error (missing `--config`, unknown flag, missing value, non-numeric value, partial cost flags, unknown `--model`) | message and usage text | not written |
| 3 | verification failed | `analyze: VERIFICATION FAILED (see the "verification" section of the output)` | **still written** |

Limits that surface as status 1: Lévy-frailty models n ≤ 16 (`kMaxLevyFrailtyComponents`); repair policies 2 ≤ n ≤ 20 (`kMaxPolicyComponents`); negative costs; β outside its range; missing β.

### 3.4 IF-3 — Result file (INT-003, NFR-006)

One JSON object. Everything except `timing` is byte-for-byte reproducible for the same inputs and tool version.

| Key | Present | Content |
|---|---|---|
| `tool` | always | `{name: "analyze", version}` |
| `config_file` | always | path as given on the command line |
| `system` | always | `name`, `n`, `total_states` (2ⁿ), `components[]` `{id, name, role, bit}` |
| `failure_model` | always | `{type, name, parameters: {lambda} or {beta}}` |
| `results.mttf_all_working` | always | m(**1**) |
| `results.counting_vector_all_working`, `.tail_signature_all_working`, `.system_signature` | always | arrays of length n + 1 (counting vector, tail signature) and n (signature) |
| `results.operational_states` | always | number of states with Φ = 1 |
| `results.minimal_path_sets` | always | `[{mask, components[]}]` |
| `results.phi_calls`, `.peak_vectors_held`, `.integer_additions`, `.integer_divisions` | always | instrumentation of the sweep (phi_calls = 2ⁿ) |
| `results.states` | `--per-state` | `[{mask, working_count, failed[], operational, counting_vector, signature, mttf}]` |
| `verification` | always | `{performed: false}` without `--verify`. With it: `performed`, `passed`, `counting_vectors_match_reference`, `first_mismatch`, `mttf_matches_eq6`, `max_relative_mttf_difference_vs_eq6`, `numerical_checks{…}`. If n exceeds the reference limit (16): `performed: false`, `skipped_reason`, `passed: null` |
| `policies` | all three cost flags | `costs`, `corrective`, `threshold{policy_count, best_index, best_gamma_from, best_gamma_to, best_long_term_mean_cost, policies[]}`, `r_out_of_n{best_r, best_long_term_mean_cost, policies[]}`, `threshold_saving_over_best_r_out_of_n`, `optimal_state_based{…, iterations, states_repaired, threshold_excess_over_optimal}`; otherwise `null` |
| `timing` | always | `analysis_ms`, `verification_ms`, `policies_ms`, `total_ms`, `peak_rss_kb` (from `VmHWM`) — **volatile**, kept apart so the rest is reproducible |

Masks are unsigned integers: bit *i* = 1 means the component whose `bit` is *i* works. Numbers are IEEE doubles; counting vectors are exact integers.

### 3.5 IF-4 — C++ library API (INT-001)

Namespace `reliability`; one header per module in `project/include/reliability/`; the library target is `reliability`
(include path `include`). Errors are reported only by throwing `ModelError` (`errors.hpp`); there are no error codes and no
global state, except the mutable Φ-call counter of a `SystemModel`.

| Header | Public interface (summary) |
|---|---|
| `state.hpp` | `StateMask = uint32_t`; `kMaxComponents = 24`; `working_count`, `all_working`, `is_working`, `without`, `sons`, `masks_with_working`, `binomial` |
| `system_model.hpp` | `Component`; `SystemModel::from_json_file`, `series`, `parallel`, `and_of_or_groups`, `or_of_and_groups`, `from_function`; `n()`, `components()`, `phi(x)`, `phi_calls()`, `reset_phi_calls()`, `validate()` |
| `signature.hpp` | `CountingVector`; `sweep(model, observer, options) → SweepResult{vectors, minimal_path_sets, phi_calls, peak_vectors_held, integer_additions, integer_divisions}`; `tail_signature(w)`, `signature(w)` |
| `failure_model.hpp` | abstract `FailureModel{name, exit_rate(w), transition_prob(w1,w2), increment(w,k)}`; `IndependentExponential`, `LevyFrailty`; `make_exponential`, `make_lethal_shock`, `make_binomial_failure_rate`, `make_compound_poisson`, `make_gamma_process`, `make_inverse_gaussian`, `make_stable_subordinator`; `psi_*` functions; `kMaxLevyFrailtyComponents = 16` |
| `mttf.hpp` | `mttf_from_counting_vector(w, fm)`, `compute_all_mttf(model, fm)` (Eq. (7)) |
| `reference.hpp` | `brute_force_counting_vectors`, `brute_force_minimal_path_sets`, `solve_mttf_direct` (Eq. (6)); `kMaxBruteForceComponents = 16` |
| `policy.hpp` | `RepairCosts{component, system, setup}`; `evaluate_policy`, `corrective_policy`, `r_out_of_n_policy`, `threshold_policy`, `build_policy_classes`, `search_threshold_policies`, `evaluate_r_out_of_n_policies`, `optimal_state_policy`, plus `*_by_enumeration` counterparts (independent check, n ≤ 14); `kMaxPolicyComponents = 20` |
| `errors.hpp` | `ModelError : std::runtime_error` |

The private header `src/level_index.hpp` (rank of sons within a level) is not part of the interface; one test includes it directly.

### 3.6 IF-5 — Python → CLI contract (INT-002)

- `run_experiments.py` locates the executable by `--analyze PATH`, else `$ANALYZE_BIN`, else `project/build/analyze`.
- It builds the argument list of IF-2 (numbers passed with `repr(float(x))`), runs it as a subprocess, parses stdout as IF-3, and treats **any non-zero exit** as an `ExperimentError` carrying stderr.
- It keeps the deterministic part of the output and moves `timing` to a separate file. It replaces `config_file` by the bare file name, so saved results do not depend on a temporary directory.
- The Python layer never re-implements the mathematics (checked by TC-G-10). The Python tests receive the executable and project root through `ANALYZE_BIN` and `PROJECT_ROOT`.
- Generated systems (`series`, `parallel`, `pairs`, `path_sets`) are written as IF-1 files.

### 3.7 IF-6 and IF-7 — Hypothesis and experiment files (EXP-001, EXP-002)

| File | Location | Producer → consumer | Content |
|---|---|---|---|
| Hypothesis | `project/experiments/hypotheses/exp1_redundancy.md` … `exp7_distance_to_optimal.md` (tracked) | analyst → runner | question, systems, numbered predictions with numeric expectations, and how each would be rejected; **must exist before the run** |
| Deterministic result | `project/results/<expN>/<expN>.json` | runner → analysis | `experiment`, `settings`, `seed`, `randomness` (`seeded …` for exp7, else `none …`), `tool_version`, `hypothesis{file, sha256}`, `raw_results[]` |
| Timing result | `project/results/<expN>/<expN>.timing.json` | runner → analysis | `started_utc`, `finished_utc`, `hypothesis_modified_utc`, `hypothesis_written_before_run`, `platform`, per-run timings |
| Copies | `…/<expN>/hypothesis.md`, `…/configs/*.json` | runner | the hypothesis and every configuration used |
| Tables | `…/<expN>/tables/*.csv` | `analyze_results.py` → `plot_results.py` | pandas tables |
| Verdicts | `…/<expN>/hypothesis_check.md` | `analyze_results.py` | one SUPPORTED / REJECTED line per prediction with evidence |
| Figures | `…/figures/*.png` | `plot_results.py` | seven figures, from the tables only |

`analyze_results.py` refuses a result file that lacks `experiment`, `settings`, `seed`, `tool_version`, `hypothesis` or
`raw_results`. `project/results/` is ignored by git except `.gitkeep`; the deterministic files are byte-identical on repeat.
`run_experiments.py` exits 1 and names the missing hypothesis file if none exists.

Runner options: `exp1 … exp7 | all`, `--analyze`, `--out`, `--hypotheses`, `--seed` (default 20261021), `--max-n` (default 20), `--systems-per-size` (default 8).

### 3.8 IF-8 — Traceability tags (VER-003)

`tools/check_traceability.py` reads (a) requirement ids matching `(SYS|ALG|REL|VER|EXP|DOC|INT|NFR)-\d{3}` in `requirements-spec.md`, (b) the matrix and the test-case definitions in `test-plan.md`, and (c) `TC-x-nn` tags in comment lines of the C++ and Python tests (shorthand such as `TC-A-02/03`, `TC-C-02, C-03`, `TC-D-01 to TC-D-05` is expanded). It exits 1 on any orphan; `--write docs/traceability-matrix.md` regenerates the matrix. It runs as the CTest test `traceability`.

### 3.9 IF-9 and IF-10 — External interfaces

| Interface | Detail |
|---|---|
| FetchContent | nlohmann/json 3.11.3 (`json.tar.xz` release asset) and GoogleTest 1.14.0 (tag archive), downloaded at configure time over HTTPS; no checksum is pinned in `CMakeLists.txt` |
| CI | `.github/workflows/ci.yml`, triggers `push` and `pull_request`; job `build-and-test` (matrix g++, clang++; Release; `ctest`) and job `sanitizers` (Debug, `-fsanitize=address,undefined`); `ubuntu-24.04`; Python 3.12; `actions/checkout@v7`, `actions/setup-python@v7`; installs `numpy pandas matplotlib` so no Python test is skipped |
| CTest | `reliability_tests` (127 GoogleTest cases, each registered by `gtest_discover_tests`), `python_end_to_end`, `traceability`: 129 entries |
| Build options | `CMAKE_BUILD_TYPE` (default Release), `BUILD_TESTING` (default ON); compile definition `RELIABILITY_VERSION` from the project version; `RELIABILITY_CONFIG_DIR` for the tests |

## 4. Requirements traceability

| Requirement | Interface(s) | Evidence |
|---|---|---|
| INT-001 library API and CLI | IF-2, IF-4 | TC-H-05, TC-H-08, TC-H-09; all C++ test files link the library |
| INT-002 Python calls the CLI only | IF-5 | TC-G-10 |
| INT-003 JSON configuration and results, provenance recorded | IF-1, IF-3, IF-7 | TC-G-01, TC-G-02, TC-G-08 (byte-identical results, provenance fields, runner), CLI output tests |
| INT-004 errors name the field and rule; non-zero status | IF-1, IF-2 | TC-E-09, TC-H-05, TC-H-06 |
| SYS-003 JSON system definition | IF-1 | TC-E-02, TC-E-03, TC-E-13 and the structure-function tests |
| NFR-006 auditability | IF-3, IF-7 | provenance fields in every result file |
| VER-003 traceability | IF-8 | TC-H-07 |

(Test-case ids are those of [test-plan.md](test-plan.md); the generated [traceability-matrix.md](traceability-matrix.md) is the authority.)

## 5. Notes

- **Not interfaces of this system:** there is no network service, no database, no GUI and no plug-in mechanism.
- **Stability:** the result schema is not versioned beyond `tool.version`; additive changes (new keys) have been the practice. A consumer should ignore keys it does not know.
- **Known gaps:** (1) the downloads in IF-9 are not checksum-pinned; (2) no JSON Schema file exists for IF-1 or IF-3 — the tables above are the specification, and the tests are the check; (3) `randomness` in IF-7 reads `"seeded: …"` for Experiment 7 (random systems from `random.Random(seed)`) and `"none: …"` for the others.
- **Platform:** the CLI reads `/proc/self/status` for peak memory (`VmHWM`) and includes `<sys/resource.h>`; it is Linux-specific as written.
