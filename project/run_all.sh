#!/usr/bin/env bash
# run_all.sh - build everything, run every test, run the analysis tool and the seven experiments, draw the figures.
#
# Needs a C++20 compiler, CMake >= 3.20 and python3 with the venv module. It needs network access the first time
# (GoogleTest, nlohmann/json and the Python packages are downloaded).
#
#   1. creates project/.venv with pandas and matplotlib, so that no Python test skips itself
#   2. configures and builds the library, the `analyze` tool and the tests (Release)
#   3. runs all tests (C++ unit tests, Python end-to-end tests, traceability check)
#   4. runs `analyze` on the satellite system: independent failures, lethal shock, binomial failure rate,
#      and a repair-policy comparison            -> results/analysis/*.json
#   5. runs the seven pre-registered experiments -> results/exp1 .. results/exp7
#   6. tabulates them with a SUPPORTED / REJECTED verdict per prediction
#   7. draws the figures                         -> results/figures/*.png
#
# Usage: ./run_all.sh [--skip-tests] [--skip-experiments]
#   BUILD_DIR=<dir>  build folder (default: build)
#
# The experiments take several minutes (the scaling experiment analyses up to 1,048,576 states).
# The script stops at the first failure. A REJECTED prediction is a result, not a failure.
set -euo pipefail
cd "$(dirname "$0")"

BUILD="${BUILD_DIR:-build}"
VENV=".venv"
SKIP_TESTS=0
SKIP_EXPERIMENTS=0
for arg in "$@"; do
  case "$arg" in
    --skip-tests) SKIP_TESTS=1 ;;
    --skip-experiments) SKIP_EXPERIMENTS=1 ;;
    -h|--help) sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "run_all.sh: unknown option '$arg' (try --help)" >&2; exit 2 ;;
  esac
done

heading() { printf '\n=== %s ===\n' "$1"; }
need() { command -v "$1" > /dev/null || { echo "run_all.sh: '$1' is required but not installed" >&2; exit 1; }; }

need cmake
need python3
command -v "${CXX:-c++}" > /dev/null || command -v g++ > /dev/null || command -v clang++ > /dev/null \
  || { echo "run_all.sh: no C++ compiler found" >&2; exit 1; }

heading "1. Python environment ($VENV: pandas, matplotlib)"
if [ ! -x "$VENV/bin/python" ]; then
  python3 -m venv "$VENV" || { echo "run_all.sh: could not create a virtual environment (on Debian/Ubuntu: sudo apt install python3-venv)" >&2; exit 1; }
fi
"$VENV/bin/pip" install --quiet -r python/requirements.txt
PY="$VENV/bin/python"
echo "using $("$PY" --version)"

heading "2. Build (Release)"
cmake -S . -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DPython3_EXECUTABLE="$(pwd)/$PY" > /dev/null
cmake --build "$BUILD" --parallel

if [ "$SKIP_TESTS" -eq 0 ]; then
  heading "3. Tests"
  ctest --test-dir "$BUILD" --output-on-failure
else
  heading "3. Tests (skipped)"
fi

heading "4. Analysis of the satellite-support system"
mkdir -p results/analysis
ANALYZE="$BUILD/analyze"
SAT=configs/satellite_support.json
"$ANALYZE" --config "$SAT" --per-state --verify --out results/analysis/satellite_exp.json
"$ANALYZE" --config "$SAT" --model lsh --beta 0.5 --per-state --verify --out results/analysis/satellite_lethal_shock.json
"$ANALYZE" --config "$SAT" --model bfr --beta 0.5 --per-state --verify --out results/analysis/satellite_binomial_failure_rate.json
"$ANALYZE" --config "$SAT" --c-comp 0.1 --c-sys 1 --c-set 0.05 --out results/analysis/satellite_repair_policies.json
"$PY" - <<'PY'
import json
for name in ["satellite_exp", "satellite_lethal_shock", "satellite_binomial_failure_rate"]:
    d = json.load(open(f"results/analysis/{name}.json"))
    verdict = "PASSED" if d["verification"]["passed"] else "FAILED"
    print(f"  {name:<34} MTTF of the new system {d['results']['mttf_all_working']:.4f}   verification {verdict}")
p = json.load(open("results/analysis/satellite_repair_policies.json"))["policies"]
print(f"  repair policies: best r-out-of-n rule repairs after {p['r_out_of_n']['best_r']} failures, "
      f"cost per time {p['r_out_of_n']['best_long_term_mean_cost']:.4f} (corrective: {p['corrective']['long_term_mean_cost']:.4f})")
PY

if [ "$SKIP_EXPERIMENTS" -eq 0 ]; then
  heading "5. Experiments 1-7 (several minutes)"
  "$PY" python/run_experiments.py all --analyze "$BUILD/analyze"

  heading "6. Tables and verdicts"
  "$PY" python/analyze_results.py

  heading "7. Figures"
  "$PY" python/plot_results.py
else
  heading "5-7. Experiments, tables and figures (skipped)"
fi

heading "Done"
echo "analysis results: project/results/analysis/"
[ "$SKIP_EXPERIMENTS" -eq 0 ] && echo "experiments:      project/results/exp1 .. exp7 (tables in each tables/ folder)" \
  && echo "figures:          project/results/figures/"
exit 0
