#!/usr/bin/env python3
"""analyze_results.py - turn saved raw results into tables and check them against the hypotheses.
                                                             (Plan: Day 8; EXP-002, EXP-003, NFR-006)

IDEA
    Reads only what run_experiments.py saved (never runs the C++ tool). For every experiment it writes
    CSV tables to results/<exp>/tables/ and a verdict file results/<exp>/hypothesis_check.md that tests each
    prediction made BEFORE the run against the data. A prediction that fails is reported as REJECTED, not
    explained away.

ALGORITHM
    load(results_dir, exp)      read <exp>.json and <exp>.timing.json; refuse files without settings, seed,
                                tool_version, hypothesis hash and raw results (traceability)
    tables_expN(...)            build the pandas DataFrames
    checks_expN(...)            one (id, statement, passed, evidence) per prediction
    main()                      write the CSVs and the verdicts, print a summary

USAGE
    python analyze_results.py [--results ../results] [exp1 exp2 exp3 exp4]
"""

import argparse
import json
import math
import sys
from pathlib import Path

import pandas as pd

PROJECT_ROOT = Path(__file__).resolve().parent.parent
REQUIRED_FIELDS = ("experiment", "settings", "seed", "tool_version", "hypothesis", "raw_results")


class ResultsError(RuntimeError):
    pass


def load(results_dir, name):
    """Read an experiment's two result files. Returns (deterministic, timing)."""
    folder = Path(results_dir) / name
    main_file, timing_file = folder / f"{name}.json", folder / f"{name}.timing.json"
    if not main_file.is_file() or not timing_file.is_file():
        raise ResultsError(f"{name}: expected {main_file} and {timing_file}; run run_experiments.py first")
    deterministic = json.loads(main_file.read_text())
    missing = [field for field in REQUIRED_FIELDS if field not in deterministic]
    if missing:
        raise ResultsError(f"{main_file} lacks required field(s) {missing}: results must carry their "
                           "settings, seed, tool version and hypothesis hash")
    return deterministic, json.loads(timing_file.read_text())


def timing_by(timing, **match):
    for run in timing["runs"]:
        if all(run.get(key) == value for key, value in match.items()):
            return run
    raise ResultsError(f"no timing entry for {match}")


def close(a, b, tolerance=1e-9):
    return abs(a - b) <= tolerance * max(1.0, abs(b))


# ----------------------------------------------------------------------------------------------------------
# Experiment 1
# ----------------------------------------------------------------------------------------------------------

EXP1_EXPECTED = {  # label: (MTTF, operational states, minimal path sets)
    "series6": (1 / 6, 1, 1),
    "one_pair": (1.5, 3, 2),
    "satellite": (0.7, 27, 8),
    "parallel6": (49 / 20, 63, 6),
}


def tables_exp1(det, timing):
    rows = []
    for item in det["raw_results"]:
        out, res = item["output"], item["output"]["results"]
        rows.append({
            "system": item["label"],
            "n": out["system"]["n"],
            "mttf": res["mttf_all_working"],
            "signature": " ".join(f"{s:.4f}" for s in res["system_signature"]),
            "operational_states": res["operational_states"],
            "total_states": out["system"]["total_states"],
            "minimal_path_sets": len(res["minimal_path_sets"]),
            "phi_calls": res["phi_calls"],
            "verification_passed": out["verification"]["passed"],
            "analysis_ms": timing_by(timing, label=item["label"])["analysis_ms_median_of_5"],
        })
    return {"exp1_systems": pd.DataFrame(rows)}


def checks_exp1(det, timing, tables):
    t = tables["exp1_systems"].set_index("system")
    checks = []
    for label, (mttf, ops, paths) in EXP1_EXPECTED.items():
        checks.append((f"MTTF of {label}", f"MTTF = {mttf:.6f} within 1e-9 relative",
                       close(t.loc[label, "mttf"], mttf), f"measured {t.loc[label, 'mttf']:.12f}"))
        checks.append((f"states and path sets of {label}", f"{ops} operational states and {paths} minimal path sets",
                       t.loc[label, "operational_states"] == ops and t.loc[label, "minimal_path_sets"] == paths,
                       f"measured {t.loc[label, 'operational_states']} and {t.loc[label, 'minimal_path_sets']}"))
    order = t.sort_values("mttf").index.tolist()
    checks.append(("MTTF ordering", "series6 < satellite < one_pair < parallel6",
                   order == ["series6", "satellite", "one_pair", "parallel6"], " < ".join(order)))
    point_masses = {"series6": 0, "one_pair": 1, "parallel6": 5}
    for item in det["raw_results"]:
        sig = item["output"]["results"]["system_signature"]
        label = item["label"]
        ok = all(x >= 0 for x in sig) and close(sum(sig), 1.0, 1e-12)
        if label in point_masses:
            ok = ok and close(sig[point_masses[label]], 1.0, 1e-12)
        if label == "satellite":
            ok = ok and all(close(a, b, 1e-12) for a, b in zip(sig, [0, 0.2, 0.4, 0.4, 0, 0]))
        checks.append((f"signature of {label}", "non-negative, sums to 1, and has the predicted shape", ok,
                       " ".join(f"{x:.4f}" for x in sig)))
    checks.append(("verification", "every run's independent verification passed",
                   bool(t["verification_passed"].all()), "all runs"))
    checks.append(("run time", "every analysis takes well under 1 ms",
                   bool((t["analysis_ms"] < 1.0).all()), f"slowest {t['analysis_ms'].max():.3f} ms"))
    return checks


# ----------------------------------------------------------------------------------------------------------
# Experiment 2
# ----------------------------------------------------------------------------------------------------------

def tables_exp2(det, timing):
    by_case = {}
    for item in det["raw_results"]:
        by_case.setdefault((item["model"], item["beta"]), {})[item["label"]] = item["output"]
    rows = []
    for (model, beta), pair in by_case.items():
        satellite = pair["satellite"]["results"]["mttf_all_working"]
        series6 = pair["series6"]["results"]["mttf_all_working"]
        rows.append({"model": model, "beta": 0.0 if beta is None else beta, "satellite_mttf": satellite,
                     "series6_mttf": series6, "redundancy_ratio": satellite / series6,
                     "verified": bool(pair["satellite"]["verification"]["passed"]
                                      and pair["series6"]["verification"]["passed"])})
    table = pd.DataFrame(rows)
    # the independent model is the beta = 0 starting point of both curves
    baseline = table[table["model"] == "exp"].iloc[0]
    curves = []
    for model in ("lsh", "bfr"):
        start = baseline.copy()
        start["model"] = model
        curves.append(pd.concat([pd.DataFrame([start]), table[table["model"] == model]], ignore_index=True))
    return {"exp2_cases": table, "exp2_curves": pd.concat(curves, ignore_index=True)}


def checks_exp2(det, timing, tables):
    curves = tables["exp2_curves"]
    lsh = curves[curves["model"] == "lsh"].sort_values("beta").reset_index(drop=True)
    bfr = curves[curves["model"] == "bfr"].sort_values("beta").reset_index(drop=True)
    checks = []
    for name, frame in (("lethal shock", lsh), ("binomial failure rate", bfr)):
        rising = bool((frame["satellite_mttf"].diff().dropna() > 0).all())
        checks.append((f"absolute MTTF rises with beta ({name})", "satellite MTTF increases at every beta step", rising,
                       " -> ".join(f"{v:.4f}" for v in frame["satellite_mttf"])))
        falling = bool((frame["redundancy_ratio"].diff().dropna() < 0).all())
        checks.append((f"redundancy ratio falls with beta ({name})", "ratio decreases at every beta step", falling,
                       " -> ".join(f"{v:.3f}" for v in frame["redundancy_ratio"])))
    higher = bool((bfr["redundancy_ratio"].iloc[1:].values > lsh["redundancy_ratio"].iloc[1:].values).all())
    checks.append(("BFR hurts redundancy less than lethal shock", "BFR ratio above the lethal-shock ratio at every beta",
                   higher, "BFR " + " ".join(f"{v:.3f}" for v in bfr["redundancy_ratio"].iloc[1:])
                   + " | LSh " + " ".join(f"{v:.3f}" for v in lsh["redundancy_ratio"].iloc[1:])))
    base_ratio = lsh["redundancy_ratio"].iloc[0]
    small = [(f, abs(f["redundancy_ratio"] / base_ratio - 1.0)) for f in (lsh.iloc[1], bfr.iloc[1])]
    checks.append(("beta = 0.01 is near the independent baseline", "both models within 2 % of the baseline ratio",
                   all(deviation < 0.02 for _, deviation in small),
                   ", ".join(f"{frame['model']}: {deviation * 100:.2f} %" for frame, deviation in small)))
    worst = 0.0
    for _, row in lsh.iloc[1:].iterrows():
        beta, r = row["beta"], 1 - row["beta"]
        closed = 8 / (beta + 3 * r) - 12 / (beta + 4 * r) + 6 / (beta + 5 * r) - 1 / (beta + 6 * r)
        worst = max(worst, abs(row["satellite_mttf"] - closed) / closed)
    checks.append(("lethal-shock closed form", "satellite MTTF within 1e-9 relative of the closed form",
                   worst <= 1e-9, f"worst relative difference {worst:.2e}"))
    checks.append(("verification", "every run's independent verification passed",
                   bool(tables["exp2_cases"]["verified"].all()), "all runs"))
    return checks


# ----------------------------------------------------------------------------------------------------------
# Experiment 3
# ----------------------------------------------------------------------------------------------------------

EXP3_EXPECTED = {  # failed count: (states, operational, distinct MTTF values)
    0: (1, 1, [0.7]),
    1: (6, 6, [8 / 15]),
    2: (15, 12, [0.0, 5 / 12]),
    3: (20, 8, [0.0, 1 / 3]),
    4: (15, 0, [0.0]),
    5: (6, 0, [0.0]),
    6: (1, 0, [0.0]),
}


def tables_exp3(det, timing):
    states = det["raw_results"][0]["output"]["results"]["states"]
    n = det["raw_results"][0]["output"]["system"]["n"]
    frame = pd.DataFrame([{
        "mask": s["mask"],
        "failed_count": n - s["working_count"],
        "failed": "+".join(s["failed"]) if s["failed"] else "none",
        "operational": s["operational"],
        "mttf": s["mttf"],
    } for s in states])
    summary = []
    for count, group in frame.groupby("failed_count"):
        values = sorted(set(round(v, 9) for v in group["mttf"]))
        summary.append({"failed_count": count, "states": len(group), "operational_states": int(group["operational"].sum()),
                        "distinct_mttf_values": len(values), "mttf_values": " ".join(f"{v:.6f}" for v in values),
                        "min_mttf": group["mttf"].min(), "max_mttf": group["mttf"].max(),
                        "spread": group["mttf"].max() - group["mttf"].min()})
    return {"exp3_states": frame, "exp3_by_failure_count": pd.DataFrame(summary)}


def checks_exp3(det, timing, tables):
    summary = tables["exp3_by_failure_count"].set_index("failed_count")
    states = tables["exp3_states"]
    checks = []
    for count, (n_states, ops, values) in EXP3_EXPECTED.items():
        row = summary.loc[count]
        measured = sorted(set(round(float(v), 9) for v in row["mttf_values"].split()))
        ok = (row["states"] == n_states and row["operational_states"] == ops and len(measured) == len(values)
              and all(close(a, b, 1e-6) for a, b in zip(measured, sorted(values))))
        checks.append((f"{count} failed components", f"{n_states} states, {ops} operational, MTTF values "
                       + ", ".join(f"{v:.4f}" for v in sorted(values)), bool(ok),
                       f"{row['states']} states, {row['operational_states']} operational, values {row['mttf_values']}"))
    broken = states[(~states["operational"]) & (states["mttf"] != 0.0)]
    checks.append(("failed states have MTTF 0", "no state with Phi = 0 has a non-zero MTTF", broken.empty,
                   f"{len(broken)} violations"))
    two_failed = states[states["failed_count"] == 2]
    partner_pairs = {"P1+P2", "C1+C2", "K1+K2"}
    zero = set(two_failed[two_failed["mttf"] == 0.0]["failed"])
    checks.append(("zero exactly for partners", "among two-failure states MTTF is 0 exactly when the failed units are partners",
                   zero == partner_pairs, f"zero-MTTF states: {sorted(zero)}"))
    return checks


# ----------------------------------------------------------------------------------------------------------
# Experiment 4
# ----------------------------------------------------------------------------------------------------------

def expected_peak_vectors(n):
    """The two largest adjacent levels: max over w of C(n,w) + C(n,w-1)."""
    return max(math.comb(n, w) + (math.comb(n, w - 1) if w >= 1 else 0) for w in range(n + 1))


def tables_exp4(det, timing):
    rows = []
    for item in det["raw_results"]:
        n = item["n"]
        res, tim = item["output"]["results"], timing_by(timing, n=n)
        rows.append({
            "n": n,
            "total_states": 2 ** n,
            "phi_calls": res["phi_calls"],
            "peak_vectors_held": res["peak_vectors_held"],
            "expected_peak_vectors": expected_peak_vectors(n),
            "analysis_ms": tim["analysis_ms"],
            "peak_rss_mb": tim["peak_rss_kb"] / 1024.0,
            "verification_ms": tim["verification_ms"],
            "integer_additions_predicted": n * (n + 1) * 2 ** (n - 2),  # sum_k C(n,k) (n-k)^2, paper Theorem 2.3
            "verification_passed": item["output"]["verification"].get("passed"),  # absent when not verified (n > 14)
        })
    frame = pd.DataFrame(rows)
    frame["time_ratio_vs_previous"] = frame["analysis_ms"] / frame["analysis_ms"].shift(1)
    frame["memory_ratio_vs_previous"] = frame["peak_rss_mb"] / frame["peak_rss_mb"].shift(1)
    frame["verification_ratio_vs_previous"] = frame["verification_ms"] / frame["verification_ms"].shift(1)
    frame["ms_per_predicted_addition"] = frame["analysis_ms"] / frame["integer_additions_predicted"]
    return {"exp4_scaling": frame}


def checks_exp4(det, timing, tables):
    t = tables["exp4_scaling"].set_index("n")
    checks = [
        ("Phi evaluated exactly 2^n times", "phi_calls == 2^n for every n", bool((t["phi_calls"] == t["total_states"]).all()),
         f"n = {t.index.min()}..{t.index.max()}"),
        ("peak vectors", "equal to the largest sum of two adjacent binomials for every n",
         bool((t["peak_vectors_held"] == t["expected_peak_vectors"]).all()), "all n"),
    ]
    big = t[t.index >= 14]["time_ratio_vs_previous"].dropna()
    if not big.empty:
        checks.append(("time growth per +2 components (n >= 14)", "between 3 and 6 (predicted 4 to 5)",
                       bool(((big >= 3) & (big <= 6)).all()),
                       "ratios " + " ".join(f"{v:.2f}" for v in big)))
    if 20 in t.index and 18 in t.index:
        mem_ratio = t.loc[20, "peak_rss_mb"] / t.loc[18, "peak_rss_mb"]
        checks.append(("memory at n = 20", "below 100 MB, growing about x4 per +2 components from n = 18",
                       bool(t.loc[20, "peak_rss_mb"] < 100 and 2.5 <= mem_ratio <= 6),
                       f"{t.loc[20, 'peak_rss_mb']:.1f} MB, ratio {mem_ratio:.2f}"))
    verified = t[(t.index >= 10) & t["verification_ms"].notna()]["verification_ratio_vs_previous"].dropna()
    if not verified.empty:
        checks.append(("verification growth per +2 components (n >= 10)", "between 6 and 12 (about 3^n: x9)",
                       bool(((verified >= 6) & (verified <= 12)).all()),
                       "ratios " + " ".join(f"{v:.2f}" for v in verified)))
    last = t["verification_ms"].dropna()
    if not last.empty:
        n_last = last.index.max()
        checks.append((f"verification vs analysis at n = {n_last}", "verification is at least 100 times slower than the analysis",
                       bool(t.loc[n_last, "verification_ms"] >= 100 * t.loc[n_last, "analysis_ms"]),
                       f"{t.loc[n_last, 'verification_ms']:.1f} ms vs {t.loc[n_last, 'analysis_ms']:.2f} ms"))
    if 6 in t.index:
        checks.append(("satellite-sized system is fast", "analysis time at n = 6 is below 1 ms",
                       bool(t.loc[6, "analysis_ms"] < 1.0), f"{t.loc[6, 'analysis_ms']:.3f} ms"))
    checks.append(("verification", "every verified run passed", bool(t["verification_passed"].dropna().astype(bool).all()),
                   "all verified n"))
    return checks


# ----------------------------------------------------------------------------------------------------------
# Experiment 5
# ----------------------------------------------------------------------------------------------------------

def tables_exp5(det, timing):
    rows = []
    for item in det["raw_results"]:
        policies = item["output"]["policies"]
        threshold, r_family = policies["threshold"], policies["r_out_of_n"]
        best_r_cost = r_family["best_long_term_mean_cost"]
        best_threshold_cost = threshold["best_long_term_mean_cost"]
        rows.append({
            "system": item["label"],
            "model": item["model"],
            "beta": item["beta"],
            "rho": item["rho"],
            "sigma": item["sigma"],
            "corrective_cost": policies["corrective"]["long_term_mean_cost"],
            "best_r": r_family["best_r"],
            "best_r_cost": best_r_cost,
            "threshold_policy_count": threshold["policy_count"],
            "best_threshold_gamma_to": threshold["best_gamma_to"],
            "best_threshold_cost": best_threshold_cost,
            "threshold_saving_over_best_r": (best_r_cost - best_threshold_cost) / best_r_cost,
            "threshold_saving_over_corrective": (policies["corrective"]["long_term_mean_cost"] - best_threshold_cost)
                                                / policies["corrective"]["long_term_mean_cost"],
        })
    frame = pd.DataFrame(rows)
    paper = [item for item in det["raw_results"] if item["label"] == "fig1_paper_costs"][0]
    paper_costs = pd.DataFrame([{"gamma_to": p["gamma_to"], "states_repaired": p["states_repaired"],
                                 "long_term_mean_cost": p["long_term_mean_cost"]}
                                for p in paper["output"]["policies"]["threshold"]["policies"]])
    return {"exp5_policies": frame, "exp5_fig1_paper_costs": paper_costs}


def checks_exp5(det, timing, tables):
    frame = tables["exp5_policies"]
    grid = frame[frame["system"] != "fig1_paper_costs"]
    checks = []

    paper = tables["exp5_fig1_paper_costs"]["long_term_mean_cost"].tolist()
    expected = [97 / 105, 11 / 15, 1 / 10]
    checks.append(("reproduction: the paper's Fig. 1 costs", "three threshold policies costing 97/105, 11/15 and 1/10",
                   len(paper) == 3 and all(close(a, b, 1e-12) for a, b in zip(paper, expected)),
                   " ".join(f"{v:.6f}" for v in paper)))

    sat = grid[grid["system"] == "satellite"]
    relative = (sat["best_threshold_cost"] - sat["best_r_cost"]).abs() / sat["best_r_cost"]
    counts = sorted(set(sat["threshold_policy_count"]))
    checks.append(("1 symmetry: threshold = r-out-of-n on the satellite",
                   "for all 135 combinations the best costs agree within 1e-9 relative, and there are exactly 4 threshold policies",
                   len(sat) == 135 and bool((relative <= 1e-9).all()) and counts == [4],
                   f"{len(sat)} combinations, largest relative difference {relative.max():.2e}, policy counts {counts}"))

    exp = sat[sat["model"] == "exp"]
    cheap = exp[(exp["rho"] == 0.1) & (exp["sigma"] == 0.0)]
    checks.append(("2 cheap components, no set-up: repair early", "best r = 1 at rho = 0.1, sigma = 0 (independent model)",
                   len(cheap) == 1 and int(cheap["best_r"].iloc[0]) == 1, f"best r = {cheap['best_r'].tolist()}"))

    dear = exp[exp["rho"] == 10]
    gap = ((dear["corrective_cost"] - dear["best_r_cost"]) / dear["best_r_cost"]).abs()
    checks.append(("3 expensive components: corrective maintenance",
                   "at rho = 10 the corrective cost equals the best cost for every sigma (within 1e-9)",
                   len(dear) == 5 and bool((gap <= 1e-9).all()), f"largest gap {gap.max():.2e}, best r by sigma {dear.sort_values('sigma')['best_r'].tolist()}"))

    monotone, detail = True, []
    for rho, group in exp.groupby("rho"):
        rs = group.sort_values("sigma")["best_r"].tolist()
        detail.append(f"rho={rho:.2f}: {rs}")
        monotone = monotone and all(b >= a for a, b in zip(rs, rs[1:]))
    checks.append(("4 set-up costs group replacements", "for each rho the best r never decreases as sigma grows (independent model)",
                   monotone, "; ".join(detail)))

    for name in ("fig1", "bridge", "eight"):
        rows = grid[grid["system"] == name]
        worst = (rows["best_threshold_cost"] - rows["best_r_cost"]) / rows["best_r_cost"]
        best_saving = rows["threshold_saving_over_best_r"].max()
        checks.append((f"5 asymmetric system '{name}'",
                       "threshold never worse than the best r-out-of-n (1e-9) in any of 45 cost pairs, and more than 0.1 % cheaper in at least one",
                       len(rows) == 45 and bool((worst <= 1e-9).all()) and bool(best_saving > 0.001),
                       f"worst excess {worst.max():.2e}; largest saving {best_saving * 100:.2f} % "
                       f"({int((rows['threshold_saving_over_best_r'] > 0.001).sum())} of 45 pairs above 0.1 %)"))
    return checks


EXPERIMENTS = {
    "exp1": (tables_exp1, checks_exp1, "Redundancy versus reliability"),
    "exp2": (tables_exp2, checks_exp2, "Independent versus simultaneous failures"),
    "exp3": (tables_exp3, checks_exp3, "States with equal failure counts"),
    "exp4": (tables_exp4, checks_exp4, "Scaling and performance"),
    "exp5": (tables_exp5, checks_exp5, "Repair policies"),
}


# ----------------------------------------------------------------------------------------------------------
# Output
# ----------------------------------------------------------------------------------------------------------

def write_verdicts(path, title, det, checks):
    lines = [f"# {title}: predictions against results", "",
             f"Tool version {det['tool_version']}, seed {det['seed']}, hypothesis SHA-256 `{det['hypothesis']['sha256'][:16]}…` "
             f"(`{det['hypothesis']['file']}`, written before the run).", "",
             "| prediction | what was predicted | verdict | evidence |", "|---|---|---|---|"]
    for name, statement, passed, evidence in checks:
        lines.append(f"| {name} | {statement} | {'SUPPORTED' if passed else '**REJECTED**'} | {evidence} |")
    rejected = [name for name, _, passed, _ in checks if not passed]
    lines += ["", f"**{len(checks) - len(rejected)} of {len(checks)} predictions supported.**"]
    if rejected:
        lines.append("Rejected: " + "; ".join(rejected) + ". These are findings, not errors in the hypothesis file.")
    path.write_text("\n".join(lines) + "\n")
    return rejected


def analyze(results_dir, name):
    det, timing = load(results_dir, name)
    build_tables, build_checks, title = EXPERIMENTS[name]
    tables = build_tables(det, timing)
    folder = Path(results_dir) / name
    (folder / "tables").mkdir(exist_ok=True)
    for table_name, frame in tables.items():
        frame.to_csv(folder / "tables" / f"{table_name}.csv", index=False)
    checks = build_checks(det, timing, tables)
    rejected = write_verdicts(folder / "hypothesis_check.md", title, det, checks)
    return tables, checks, rejected


def main(argv=None):
    parser = argparse.ArgumentParser(description="Make tables and hypothesis verdicts from saved results.")
    parser.add_argument("experiments", nargs="*", help="exp1 .. exp4 (default: every experiment that has results)")
    parser.add_argument("--results", default=str(PROJECT_ROOT / "results"))
    args = parser.parse_args(argv)
    names = args.experiments or [n for n in EXPERIMENTS if (Path(args.results) / n / f"{n}.json").is_file()]
    if not names:
        print("analyze_results: no results found; run run_experiments.py first", file=sys.stderr)
        return 1
    status = 0
    try:
        for name in names:
            tables, checks, rejected = analyze(args.results, name)
            print(f"{name}: {len(checks) - len(rejected)}/{len(checks)} predictions supported"
                  + (f"; REJECTED: {', '.join(rejected)}" if rejected else ""))
            for table_name, frame in tables.items():
                if len(frame) <= 12:
                    print(frame.to_string(index=False), end="\n\n")
    except ResultsError as error:
        print(f"analyze_results: {error}", file=sys.stderr)
        status = 1
    return status


if __name__ == "__main__":
    sys.exit(main())
