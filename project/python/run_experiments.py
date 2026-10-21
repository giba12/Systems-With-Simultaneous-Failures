"""run_experiments.py - run the four experiments and save raw results.   (Plan: Day 8; EXP-001, EXP-002)

IDEA
    The Python layer never re-implements the maths. It calls the C++ tool "analyze",
    saves exactly what came back plus the settings used, and nothing else. That way every
    number in a plot can be traced to a configuration, a seed and a tool version.

PSEUDO-CODE
    main(experiment_name, config_path, seed, out_dir):
        hypothesis = read_text(out_dir / "hypothesis.md")
        if it is missing: stop with "write your hypothesis BEFORE running" (guide Section 8)
        set the random seed (only used where an experiment draws random systems)
        results = EXPERIMENTS[experiment_name](config, seed)
        write out_dir / "<experiment>.json" containing
              { config, seed, tool_version, hypothesis_file, raw_results, timestamp }

    run_analyze(config_path, model, lambda_or_beta):
        run `build/analyze --config ... --verify --out <temp>` with subprocess
        if the exit status is non-zero: raise with the tool's message
        return the parsed JSON it wrote

    experiment_1_redundancy():          # series vs one pair vs the six-component system, [Exp]
        for each system: record initial-state MTTF, signature, number of operational
        states and runtime
    experiment_2_common_cause():        # needs the lethal-shock model (Day 7)
        for beta in {0.01, 0.05, 0.10, 0.20, 0.50}:
            record MTTF of the satellite AND of the series-of-6 baseline under the same model
            report the RATIO (absolute MTTF goes UP with beta; the ratio goes DOWN)
    experiment_3_equal_failure_counts():
        record MTTF of every state grouped by number of failed components
        highlight states with the same failure count but different MTTF
        (P1+C1 failed -> 5/12, P1+P2 failed -> 0)
    experiment_4_scaling():
        for n = 4, 6, 8, ... up to the agreed maximum:
            record runtime, peak memory, number of Phi calls (must be 2^n), operation counts
            compare with brute force only while that is still practical
"""

EXPERIMENTS = {}  # name -> function, filled in as experiments are written


def run_analyze(config_path, model="exp", parameter=1.0):
    raise NotImplementedError("run_analyze: see pseudo-code in the module docstring")


def main():
    raise NotImplementedError("run_experiments: skeleton only. See docs/project-plan.md Day 8.")


if __name__ == "__main__":
    main()
