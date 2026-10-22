#!/usr/bin/env bash
# demo.sh - a two-minute tour of the Space System Reliability Analyzer.
#
# Needs only a C++ compiler, CMake and python3 (standard library). It builds the tool if necessary, then
#   1. analyses the fictional satellite-support system and verifies the answer independently
#   2. shows that two states with the same number of failures can be worth very different amounts
#   3. shows what a common-cause failure mechanism does to the benefit of redundancy
#   4. analyses a 20-component system (about a million states) to show the cost of exactness
#   5. compares repair policies with the paper's cost model (its Fig. 1 example, then the satellite)
#
# The satellite is a FICTIONAL, educational model: not a certified or flight-qualified analysis.
set -euo pipefail
cd "$(dirname "$0")"

BUILD="${BUILD_DIR:-build}"
if [ ! -x "$BUILD/analyze" ]; then
  echo "Building the analyzer (first run only)..."
  cmake -S . -B "$BUILD" -DCMAKE_BUILD_TYPE=Release > /dev/null
  cmake --build "$BUILD" --parallel > /dev/null
fi
ANALYZE="$BUILD/analyze"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# make_config KIND N FILE : a system of N components (series, parallel, or N/2 redundant pairs)
make_config() {
  python3 - "$1" "$2" "$3" <<'PY'
import json, sys
kind, n, path = sys.argv[1], int(sys.argv[2]), sys.argv[3]
comps = [{"id": f"C{i+1}", "bit": i} for i in range(n)]
if kind == "pairs":
    structure = {"type": "and_of_or_groups", "groups": [[f"C{2*p+1}", f"C{2*p+2}"] for p in range(n // 2)]}
else:
    structure = {"type": kind}
json.dump({"name": f"{kind}_{n}", "components": comps, "structure": structure}, open(path, "w"))
PY
}

# field FILE EXPRESSION : print a value from an analyze JSON file (python expression on `d`)
field() { python3 -c "import json,sys; d=json.load(open(sys.argv[1])); print($2)" "$1"; }

heading() { printf '\n=== %s ===\n' "$1"; }

heading "1. The system: three redundant pairs (power, communications, computers); all three pairs must survive"
"$ANALYZE" --config configs/satellite_support.json --per-state --verify --out "$WORK/sat.json"
echo "  expected lifetime of the new system (a component lives 1 on average): $(field "$WORK/sat.json" "round(d['results']['mttf_all_working'], 4)")"
echo "  working states out of 64: $(field "$WORK/sat.json" "d['results']['operational_states']")"
echo "  chance the system dies at the k-th component failure (k = 1..6): $(field "$WORK/sat.json" "[round(x, 2) for x in d['results']['system_signature']]")"
echo "  minimal sets of parts that keep it alive: $(field "$WORK/sat.json" "len(d['results']['minimal_path_sets'])")"
echo "  independent verification (brute force + a second equation): $(field "$WORK/sat.json" "'PASSED' if d['verification']['passed'] else 'FAILED'")"
echo "  structure function evaluated $(field "$WORK/sat.json" "d['results']['phi_calls']") times = 2^6, once per state"

heading "2. Same number of failures, very different futures"
python3 - "$WORK/sat.json" <<'PY'
import json, sys
states = json.load(open(sys.argv[1]))["results"]["states"]
def mttf(failed): return next(s["mttf"] for s in states if s["failed"] == failed)
print(f"  power unit A and communications unit A failed : expected remaining life {mttf(['P1', 'C1']):.4f}")
print(f"  power unit A and power unit B failed          : expected remaining life {mttf(['P1', 'P2']):.4f}   (a whole pair is gone)")
PY

heading "3. Common-cause failures (one event can fail several parts at once)"
make_config series 6 "$WORK/series6.json"
printf '  %-34s %-18s %-18s %s\n' "model" "satellite MTTF" "six in series" "benefit of redundancy"
for spec in "exp:" "lsh:0.2" "lsh:0.5" "bfr:0.5"; do
  model="${spec%%:*}"; beta="${spec##*:}"
  args=(--model "$model"); [ -n "$beta" ] && args+=(--beta "$beta")
  "$ANALYZE" --config configs/satellite_support.json "${args[@]}" --out "$WORK/a.json"
  "$ANALYZE" --config "$WORK/series6.json" "${args[@]}" --out "$WORK/b.json"
  label="$model${beta:+, beta=$beta}"; [ "$model" = exp ] && label="independent failures"
  [ "$model" = lsh ] && label="lethal shock, beta=$beta"; [ "$model" = bfr ] && label="binomial failure rate, beta=$beta"
  python3 - "$WORK/a.json" "$WORK/b.json" "$label" <<'PY'
import json, sys
a, b = (json.load(open(p))["results"]["mttf_all_working"] for p in sys.argv[1:3])
print(f"  {sys.argv[3]:<34} {a:<18.4f} {b:<18.4f} x{a / b:.2f}")
PY
done
echo "  (absolute lifetime can rise under common cause, but redundancy is worth less relative to a series system)"

heading "4. The price of exactness: 20 components = 1,048,576 states, every one analysed"
make_config pairs 20 "$WORK/pairs20.json"
"$ANALYZE" --config "$WORK/pairs20.json" --out "$WORK/big.json"
echo "  analysis time: $(field "$WORK/big.json" "round(d['timing']['analysis_ms'] / 1000, 2)") s, peak memory: $(field "$WORK/big.json" "round(d['timing']['peak_rss_kb'] / 1024, 1)") MB"
echo "  structure function evaluations: $(field "$WORK/big.json" "d['results']['phi_calls']")  (exactly 2^20)"
echo "  counting vectors held at once: $(field "$WORK/big.json" "d['results']['peak_vectors_held']")  (two levels, instead of all 1,048,576)"

heading "5. When is a repair worth it? (the paper's cost model: replace every failed part, restoring a new system)"
python3 - "$WORK/fig1.json" <<'PY'
import json, sys
json.dump({"name": "paper_fig1", "components": [{"id": f"x{i+1}", "bit": i} for i in range(3)],
           "structure": {"type": "path_sets", "paths": [["x1", "x2"], ["x3"]]}}, open(sys.argv[1], "w"))
PY
"$ANALYZE" --config "$WORK/fig1.json" --c-comp 0.03333333333333333 --c-sys 1 --c-set 0 --out "$WORK/p1.json"
echo "  the paper's Fig. 1 system (3 components, replacing a part costs 1/30 of a system failure, no set-up cost):"
python3 - "$WORK/p1.json" <<'PY'
import json, sys
t = json.load(open(sys.argv[1]))["policies"]["threshold"]["policies"]
for p, published in zip(t, ["97/105", "11/15", "1/10"]):
    print(f"    repair when MTTF falls below {p['gamma_from']:.2f} - {p['gamma_to']:.2f} of new: cost per time {p['long_term_mean_cost']:.4f}   (paper: {published})")
PY
"$ANALYZE" --config configs/satellite_support.json --c-comp 0.1667 --c-sys 1 --c-set 0.2 --out "$WORK/p2.json"
python3 - "$WORK/p2.json" <<'PY'
import json, sys
p = json.load(open(sys.argv[1]))["policies"]
r = p["r_out_of_n"]
print("  the satellite (replacing everything costs as much as one system failure, each repair visit costs 0.2):")
print(f"    best rule: repair after {r['best_r']} failures, cost per time {r['best_long_term_mean_cost']:.4f}; "
      f"waiting for the system to fail costs {p['corrective']['long_term_mean_cost']:.4f}")
PY

echo
echo "Done. Reproduce the six experiments and the figures with the commands in README.md."
