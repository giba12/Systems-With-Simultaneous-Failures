#!/usr/bin/env python3
"""run_experiments.py - run the four experiments and save raw results.   (Plan: Day 8; EXP-001, EXP-002)

IDEA
    The Python layer never re-implements the maths. It calls the C++ tool ``analyze`` and saves exactly what
    came back plus the settings used. Every number in a table or plot can therefore be traced to a
    configuration, a seed and a tool version.

THE RULES THIS SCRIPT ENFORCES
    * A hypothesis file must exist for an experiment BEFORE it runs (experiments/hypotheses/<name>.md).
      The runner refuses to start otherwise, copies the file into the results, and records its SHA-256 and
      whether it was last modified before the run began.
    * Results are split in two files so reproducibility can be checked byte for byte:
        <name>.json         deterministic: settings, seed, tool version, hypothesis hash, raw tool output
        <name>.timing.json  volatile: run times, memory, timestamps, platform
    * All four experiments are deterministic, so the seed is recorded but no random numbers are drawn.

USAGE
    python run_experiments.py exp1 exp2 exp3 exp4        (or: all)
        [--analyze ../build/analyze] [--out ../results] [--hypotheses ../experiments/hypotheses]
        [--seed 20261021] [--max-n 20]
"""

import argparse
import datetime
import hashlib
import json
import os
import platform
import random
import statistics
import subprocess
import sys
import tempfile
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
SATELLITE_CONFIG = PROJECT_ROOT / "configs" / "satellite_support.json"
DEFAULT_SEED = 20261021

EXPERIMENT_FILES = {
    "exp1": "exp1_redundancy",
    "exp2": "exp2_common_cause",
    "exp3": "exp3_equal_failure_counts",
    "exp4": "exp4_scaling",
    "exp5": "exp5_repair_policies",
}


class ExperimentError(RuntimeError):
    """Raised for anything that should stop an experiment with a clear message."""


# ----------------------------------------------------------------------------------------------------------
# Talking to the C++ tool
# ----------------------------------------------------------------------------------------------------------

def find_analyze(explicit=None):
    """ALGORITHM: an explicit --analyze path is used as given (and is an error if it is not an executable);
    otherwise $ANALYZE_BIN, otherwise ../build/analyze."""
    def usable(candidate):
        return bool(candidate) and Path(candidate).is_file() and os.access(candidate, os.X_OK)

    if explicit is not None:
        if not usable(explicit):
            raise ExperimentError(f"--analyze {explicit}: no such executable")
        return str(Path(explicit).resolve())
    for candidate in (os.environ.get("ANALYZE_BIN"), str(PROJECT_ROOT / "build" / "analyze")):
        if usable(candidate):
            return str(Path(candidate).resolve())
    raise ExperimentError(
        "cannot find the 'analyze' executable. Build it first (cmake --build build) or pass --analyze PATH")


def run_analyze(analyze, config_path, model=None, lam=None, beta=None, per_state=False, verify=False, costs=None):
    """Run ``analyze`` once and return its parsed JSON. A non-zero exit status raises ExperimentError.
    costs = (c_comp, c_sys, c_set) also asks for the repair-policy comparison."""
    command = [analyze, "--config", str(config_path)]
    if model:
        command += ["--model", model]
    if lam is not None:
        command += ["--lambda", repr(float(lam))]
    if beta is not None:
        command += ["--beta", repr(float(beta))]
    if costs is not None:
        command += ["--c-comp", repr(float(costs[0])), "--c-sys", repr(float(costs[1])), "--c-set", repr(float(costs[2]))]
    if per_state:
        command.append("--per-state")
    if verify:
        command.append("--verify")
    done = subprocess.run(command, capture_output=True, text=True)
    if done.returncode != 0:
        raise ExperimentError(f"analyze failed (exit {done.returncode}) for {' '.join(command)}:\n{done.stderr.strip()}")
    output = json.loads(done.stdout)
    # the tool echoes the path it was given; generated configs live in a temporary directory with a random
    # name, so keep only the file name to keep the saved results reproducible
    output["config_file"] = Path(config_path).name
    return output


def split_timing(output):
    """ALGORITHM: return (deterministic part, timing part). The tool keeps volatile numbers under 'timing'."""
    deterministic = {key: value for key, value in output.items() if key != "timing"}
    return deterministic, output["timing"]


# ----------------------------------------------------------------------------------------------------------
# System configurations written on the fly
# ----------------------------------------------------------------------------------------------------------

def make_config(kind, n, paths=None):
    """ALGORITHM: a configuration (as a dict) for n components C1..Cn.

    kind = 'paths'    works if all components of some path set work; `paths` = list of lists of 1-based indices
           'series'   all n components are needed
           'parallel' any one component is enough
           'pairs'    n/2 redundant pairs, every pair needed (n even); n = 6 is the satellite's shape
    """
    components = [{"id": f"C{i + 1}", "name": f"Component {i + 1}", "role": "", "bit": i} for i in range(n)]
    if kind == "series":
        structure = {"type": "series"}
    elif kind == "parallel":
        structure = {"type": "parallel"}
    elif kind == "paths":
        structure = {"type": "path_sets", "paths": [[f"C{i}" for i in path] for path in paths]}
    elif kind == "pairs":
        if n % 2:
            raise ExperimentError("'pairs' needs an even number of components")
        structure = {"type": "and_of_or_groups",
                     "groups": [[f"C{2 * p + 1}", f"C{2 * p + 2}"] for p in range(n // 2)]}
    else:
        raise ExperimentError(f"unknown configuration kind '{kind}'")
    return {"name": f"{kind}_{n}", "components": components, "structure": structure}


def write_config(directory, kind, n, paths=None, name=None):
    path = Path(directory) / f"{name or f'{kind}_{n}'}.json"
    path.write_text(json.dumps(make_config(kind, n, paths), indent=2) + "\n")
    return path


# ----------------------------------------------------------------------------------------------------------
# Experiments. Each returns (settings, raw_results, timing_runs)
#   settings     what was configured (goes into the deterministic file)
#   raw_results  the tool's output without its timing block, labelled
#   timing_runs  one entry per run with the volatile numbers
# ----------------------------------------------------------------------------------------------------------

def timed_runs(analyze, config, repeats, **options):
    """Run the same analysis `repeats` times; return (first result, median analysis_ms, max peak_rss_kb)."""
    outputs = [run_analyze(analyze, config, **options) for _ in range(repeats)]
    deterministic = [split_timing(o)[0] for o in outputs]
    if any(d != deterministic[0] for d in deterministic):
        raise ExperimentError("the tool gave different results for identical inputs: not reproducible")
    timings = [o["timing"] for o in outputs]
    return (deterministic[0],
            statistics.median(t["analysis_ms"] for t in timings),
            max(t["peak_rss_kb"] for t in timings),
            [t["verification_ms"] for t in timings])


def experiment_1(analyze, workdir, seed, **_):
    """Redundancy versus reliability: series, one pair, the satellite, parallel; [Exp] with lambda = 1."""
    systems = [("series6", write_config(workdir, "series", 6)),
               ("one_pair", write_config(workdir, "parallel", 2)),
               ("satellite", SATELLITE_CONFIG),
               ("parallel6", write_config(workdir, "parallel", 6))]
    settings = {"model": "exp", "lambda": 1.0, "verify": True,
                "systems": [{"label": label, "config": Path(path).name} for label, path in systems]}
    raw, timing = [], []
    for label, config in systems:
        result, median_ms, rss, verification = timed_runs(analyze, config, 5, model="exp", lam=1.0, verify=True)
        raw.append({"label": label, "output": result})
        timing.append({"label": label, "analysis_ms_median_of_5": median_ms, "peak_rss_kb": rss,
                       "verification_ms": statistics.median(verification)})
    return settings, raw, timing


BETAS = [0.01, 0.05, 0.10, 0.20, 0.50]


def experiment_2(analyze, workdir, seed, **_):
    """Independent versus simultaneous failures: satellite and series-of-six under lethal shock and BFR."""
    satellite, series6 = SATELLITE_CONFIG, write_config(workdir, "series", 6)
    cases = [("exp", None)] + [(model, beta) for model in ("lsh", "bfr") for beta in BETAS]
    settings = {"betas": BETAS, "models": ["lsh", "bfr"], "baseline": "exp (lambda = 1)", "verify": True,
                "systems": ["satellite", "series6 (same six components in series)"]}
    raw, timing = [], []
    for model, beta in cases:
        for label, config in (("satellite", satellite), ("series6", series6)):
            result, median_ms, rss, verification = timed_runs(
                analyze, config, 1, model=model, beta=beta, lam=1.0 if model == "exp" else None, verify=True)
            raw.append({"label": label, "model": model, "beta": beta, "output": result})
            timing.append({"label": label, "model": model, "beta": beta, "analysis_ms": median_ms,
                           "peak_rss_kb": rss})
    return settings, raw, timing


def experiment_3(analyze, workdir, seed, **_):
    """States with equal failure counts: every state of the satellite under [Exp] with lambda = 1."""
    settings = {"model": "exp", "lambda": 1.0, "system": "satellite", "per_state": True, "verify": True}
    result, median_ms, rss, verification = timed_runs(
        analyze, SATELLITE_CONFIG, 1, model="exp", lam=1.0, per_state=True, verify=True)
    return (settings, [{"label": "satellite", "output": result}],
            [{"label": "satellite", "analysis_ms": median_ms, "peak_rss_kb": rss,
              "verification_ms": statistics.median(verification)}])


def experiment_4(analyze, workdir, seed, max_n=20, max_verify_n=14, **_):
    """Scaling: n/2 redundant pairs, n = 4, 6, ..., max_n; timing without verification, verification apart."""
    sizes = list(range(4, max_n + 1, 2))
    settings = {"model": "exp", "lambda": 1.0, "structure": "n/2 redundant pairs", "sizes": sizes,
                "max_verified_n": max_verify_n}
    raw, timing = [], []
    for n in sizes:
        config = write_config(workdir, "pairs", n)
        repeats = 3 if n <= 16 else 1
        result, median_ms, rss, _ = timed_runs(analyze, config, repeats, model="exp", lam=1.0)
        entry = {"n": n, "analysis_ms": median_ms, "peak_rss_kb": rss, "repeats": repeats,
                 "verification_ms": None}
        if n <= max_verify_n:
            verified, _, _, verification_ms = timed_runs(analyze, config, 1, model="exp", lam=1.0, verify=True)
            entry["verification_ms"] = statistics.median(verification_ms)
            result = verified  # carries the verification report
        raw.append({"n": n, "output": result})
        timing.append(entry)
        print(f"  n = {n:2d}: analysis {median_ms:10.2f} ms, peak memory {rss / 1024:8.1f} MB", file=sys.stderr)
    return settings, raw, timing


RHOS = [0.1, 0.2, 0.5, 2 / 3, 1, 4 / 3, 2, 4, 10]
SIGMAS = [0.0, 0.05, 0.1, 0.2, 0.5]


def bits_to_path(mask):
    """A bitmask of component positions -> the 1-based component numbers C1..Cn it names."""
    return [i + 1 for i in range(mask.bit_length()) if mask >> i & 1]


# the three asymmetric systems of experiment 5 (path sets as 1-based component numbers)
ASYMMETRIC_SYSTEMS = {
    "fig1": (3, [[1, 2], [3]]),                                               # the paper's Fig. 1: x3 or (x1 and x2)
    "bridge": (5, [[1, 4], [2, 5], [1, 3, 5], [2, 3, 4]]),                     # the five-component bridge network
    "eight": (8, [bits_to_path(m) for m in (90, 203, 22, 41)]),                # found to have a non-unimodal cost curve
}


def experiment_5(analyze, workdir, seed, **_):
    """Repair policies: corrective, r-out-of-n and every MTTF-threshold policy over the paper's cost grid.

    c_sys = 1; c_comp = rho / n; c_set = sigma. The satellite under three failure models, three asymmetric systems under
    the independent model, and the paper's own Fig. 1 cost setting as a reproduction check."""
    satellite_models = [("exp", None), ("lsh", 0.2), ("bfr", 0.2)]
    systems = [("satellite", SATELLITE_CONFIG, 6, satellite_models)]
    for label, (n, paths) in ASYMMETRIC_SYSTEMS.items():
        systems.append((label, write_config(workdir, "paths", n, paths=paths, name=label), n, [("exp", None)]))
    settings = {"rhos": RHOS, "sigmas": SIGMAS, "c_sys": 1.0, "c_comp": "rho / n", "c_set": "sigma",
                "satellite_models": [{"model": m, "beta": b} for m, b in satellite_models],
                "asymmetric_systems": {k: {"n": n, "path_sets": p} for k, (n, p) in ASYMMETRIC_SYSTEMS.items()},
                "reproduction": "fig1 with c_comp = 1/30, c_sys = 1, c_set = 0"}
    raw, timing = [], []
    for label, config, n, models in systems:
        for model, beta in models:
            for rho in RHOS:
                for sigma in SIGMAS:
                    output, t = split_timing(run_analyze(analyze, config, model=model, beta=beta,
                                                         lam=1.0 if model == "exp" else None, costs=(rho / n, 1.0, sigma)))
                    raw.append({"label": label, "model": model, "beta": beta, "rho": rho, "sigma": sigma, "output": output})
                    timing.append({"label": label, "model": model, "beta": beta, "rho": rho, "sigma": sigma,
                                   "analysis_ms": t["analysis_ms"], "policies_ms": t["policies_ms"]})
    output, t = split_timing(run_analyze(analyze, workdir_config(workdir, "fig1"), model="exp", lam=1.0,
                                         costs=(1 / 30, 1.0, 0.0)))
    raw.append({"label": "fig1_paper_costs", "model": "exp", "beta": None, "rho": None, "sigma": 0.0, "output": output})
    timing.append({"label": "fig1_paper_costs", "analysis_ms": t["analysis_ms"], "policies_ms": t["policies_ms"]})
    return settings, raw, timing


def workdir_config(workdir, name):
    return Path(workdir) / f"{name}.json"


EXPERIMENTS = {"exp1": experiment_1, "exp2": experiment_2, "exp3": experiment_3, "exp4": experiment_4,
               "exp5": experiment_5}


# ----------------------------------------------------------------------------------------------------------
# The runner
# ----------------------------------------------------------------------------------------------------------

def sha256_of(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def utc(timestamp):
    return datetime.datetime.fromtimestamp(timestamp, datetime.timezone.utc).isoformat()


def require_hypothesis(hypotheses_dir, name):
    """ALGORITHM: the hypothesis must exist and not be empty, or the experiment does not run."""
    path = Path(hypotheses_dir) / f"{EXPERIMENT_FILES[name]}.md"
    if not path.is_file() or not path.read_text().strip():
        raise ExperimentError(
            f"no hypothesis for {name}: write {path} BEFORE running the experiment "
            "(state what you expect and what would refute it)")
    return path


def run_experiment(name, analyze, out_dir, hypotheses_dir, seed, max_n=20):
    """ALGORITHM
        1. require the hypothesis file; remember when this run started
        2. random.Random(seed) is created for the record (all experiments here are deterministic)
        3. run the experiment function; check the tool version is the same in every output
        4. write <name>.json (deterministic) and <name>.timing.json (volatile) and copy the hypothesis
    """
    hypothesis = require_hypothesis(hypotheses_dir, name)
    started = datetime.datetime.now(datetime.timezone.utc)
    random.Random(seed)  # no random numbers are drawn; the seed is recorded so a stochastic experiment could be added

    out = Path(out_dir) / name
    out.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as workdir:
        settings, raw, timing = EXPERIMENTS[name](analyze, workdir, seed, max_n=max_n)
        # keep a copy of every generated configuration next to the results (traceability)
        configs_dir = out / "configs"
        configs_dir.mkdir(exist_ok=True)
        for generated in sorted(Path(workdir).glob("*.json")):
            (configs_dir / generated.name).write_text(generated.read_text())
    (configs_dir / SATELLITE_CONFIG.name).write_text(SATELLITE_CONFIG.read_text())

    versions = set()
    def collect(items):
        for item in items:
            versions.add(item["output"]["tool"]["version"])
    collect(raw)
    if len(versions) != 1:
        raise ExperimentError(f"outputs came from different tool versions: {sorted(versions)}")

    (out / "hypothesis.md").write_text(hypothesis.read_text())
    deterministic = {
        "experiment": name,
        "settings": settings,
        "seed": seed,
        "randomness": "none: every experiment is deterministic",
        "tool_version": versions.pop(),
        "hypothesis": {"file": hypothesis.name, "sha256": sha256_of(hypothesis)},
        "raw_results": raw,
    }
    (out / f"{name}.json").write_text(json.dumps(deterministic, indent=2, sort_keys=True) + "\n")

    finished = datetime.datetime.now(datetime.timezone.utc)
    volatile = {
        "experiment": name,
        "started_utc": started.isoformat(),
        "finished_utc": finished.isoformat(),
        "hypothesis_modified_utc": utc(hypothesis.stat().st_mtime),
        "hypothesis_written_before_run": hypothesis.stat().st_mtime <= started.timestamp(),
        "python": sys.version.split()[0],
        "platform": platform.platform(),
        "runs": timing,
    }
    (out / f"{name}.timing.json").write_text(json.dumps(volatile, indent=2, sort_keys=True) + "\n")
    return out


def main(argv=None):
    parser = argparse.ArgumentParser(description="Run the reliability experiments and save raw results.")
    parser.add_argument("experiments", nargs="+", help="exp1 exp2 exp3 exp4, or 'all'")
    parser.add_argument("--analyze", help="path of the analyze executable (default: ../build/analyze)")
    parser.add_argument("--out", default=str(PROJECT_ROOT / "results"))
    parser.add_argument("--hypotheses", default=str(PROJECT_ROOT / "experiments" / "hypotheses"))
    parser.add_argument("--seed", type=int, default=DEFAULT_SEED)
    parser.add_argument("--max-n", type=int, default=20, help="largest n of experiment 4 (default 20)")
    args = parser.parse_args(argv)

    names = list(EXPERIMENTS) if args.experiments == ["all"] else args.experiments
    unknown = [n for n in names if n not in EXPERIMENTS]
    if unknown:
        parser.error(f"unknown experiment(s): {', '.join(unknown)} (choose from {', '.join(EXPERIMENTS)} or all)")
    try:
        analyze = find_analyze(args.analyze)
        for name in names:
            print(f"running {name} ...", file=sys.stderr)
            out = run_experiment(name, analyze, args.out, args.hypotheses, args.seed, args.max_n)
            print(f"  saved to {out}", file=sys.stderr)
    except ExperimentError as error:
        print(f"run_experiments: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
