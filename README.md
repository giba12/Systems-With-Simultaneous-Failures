# Space System Reliability Analyzer

An exact reliability analysis of a **fictional** six-component satellite-support system, implementing the per-state
signature / mean-time-to-failure algorithm of Lagos & Ávila,
[*Per-State Signatures and Mean Time-to-Failure: An Algorithm for Systems With Simultaneous Failures*](https://arxiv.org/abs/2610.22181)
(arXiv:2610.22181). It answers: *given which components have failed, how much useful life is left — and how does that
change when one event can fail several components at once?*

> Educational portfolio project. Not a certified safety analysis, not flight-qualified, and not a statement about any
> real spacecraft or any organisation's tools.

## What it does

* computes the **signature and MTTF at every one of the 2ⁿ states** while evaluating the structure function exactly once
  per state, for independent failures and for common-cause failures (lethal shock, binomial failure rate);
* checks every answer against an **independent brute-force implementation**, a second equation, closed forms and the
  paper's own worked example;
* runs **four experiments whose hypotheses were committed before they were run**, and reports the predictions that failed.

| | |
|---|---|
| Satellite (three redundant pairs), MTTF of the new system | **0.700** (six in series: 0.167; one pair: 1.5) |
| Two failures, different units | MTTF **0.417** if the failed units are in different pairs, **0** if they are partners |
| Common cause (β = 0.5), benefit of redundancy | 4.20 → **3.20** (lethal shock), **1.50** (binomial failure rate) |
| 20 components, 1 048 576 states | analysed exactly in about 3 s and 35 MB |
| Pre-registered predictions | 36 of 40 supported in the first run, 37 in a repeat; **3 rejections are stable** (a 4th is timing noise) and reported as found |

![Common-cause failures erode the benefit of redundancy](docs/figures/fig2_common_cause.png)

## Quick start

Needs a C++20 compiler (g++ 13 and clang++ 18 tested), CMake ≥ 3.20 and Python 3 (standard library). The first
configure downloads GoogleTest and nlohmann/json.

```bash
cmake -S project -B project/build -DCMAKE_BUILD_TYPE=Release
cmake --build project/build --parallel
ctest --test-dir project/build --output-on-failure     # 101 tests
project/demo.sh                                         # a two-minute tour (see docs/demo.md)
```

Analyse a system yourself:

```bash
project/build/analyze --config project/configs/satellite_support.json --verify
project/build/analyze --config project/configs/satellite_support.json --model lsh --beta 0.5 --per-state --out result.json
```

## Reproduce the experiments and figures

```bash
cd project
python3 python/run_experiments.py all                       # standard library only; results go to project/results/
python3 -m venv .venv && .venv/bin/pip install -r python/requirements.txt
.venv/bin/python python/analyze_results.py                  # tables and a SUPPORTED / REJECTED verdict per prediction
.venv/bin/python python/plot_results.py                     # figures into project/results/figures
```

The runner refuses to start an experiment that has no hypothesis file in `project/experiments/hypotheses/`. The saved
result files are byte-identical when the experiments are repeated.

## Where to read next

| If you want… | read |
|---|---|
| the whole story in one document | [docs/technical-report.md](docs/technical-report.md) |
| the results and the rejected predictions | [docs/experiment-report.md](docs/experiment-report.md) |
| how it was verified, and what was not | [docs/verification-report.md](docs/verification-report.md) |
| every deviation from the paper and the guide | [docs/deviation-log.md](docs/deviation-log.md) |
| the paper's symbols in plain language | [docs/notation-glossary.md](docs/notation-glossary.md) |
| requirements, tests, plan, diagrams | [docs/README.md](docs/README.md) |
| a guided run | [docs/demo.md](docs/demo.md) |

## Repository layout

| Path | Contents |
|---|---|
| `project/` | the C++20 library, the `analyze` tool, tests, the Python experiment layer, tools, `demo.sh` (see [project/README.md](project/README.md)) |
| `docs/` | requirements, test plan, user stories, UML diagrams, reports, the paper and the project guide |
| `.github/workflows/ci.yml` | continuous integration: g++ and clang++ in Release, plus an ASan/UBSan job |

## Honest status

Built and tested on Linux (WSL2) with g++ 13.3 and clang++ 18.1: no compiler warnings, 101/101 tests, sanitizer-clean.
The CI workflow has been validated and executed locally but has **not yet run on a CI service**. The simultaneous-failure
models are limited to 16 components (numerical precision). The paper's repair-policy part is not implemented. Components
are assumed identical, as in the paper. See the [deviation log](docs/deviation-log.md) for the complete list.

## Licence

No licence has been chosen yet; until one is added, all rights are reserved by default. The paper (`docs/paper3.pdf`) is
the authors' work and is included for reference only.
