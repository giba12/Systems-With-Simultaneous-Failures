"""analyze_results.py - turn saved raw results into tables.   (Plan: Day 8; EXP-002, EXP-003)

PSEUDO-CODE
    load(results_dir):
        read every <experiment>.json written by run_experiments.py
        refuse files without config, seed and tool_version (traceability, NFR-006)

    summarise_experiment_1(data)  -> table: system | n | MTTF | signature | operational states | runtime
    summarise_experiment_2(data)  -> table: beta | MTTF satellite | MTTF series-of-6 | ratio
    summarise_experiment_3(data)  -> table: failed count | state | failed components | MTTF
    summarise_experiment_4(data)  -> table: n | runtime | memory | Phi calls | ops | brute-force runtime

    main():
        load, summarise, write CSV files next to the raw results, print them
        (uses pandas; never calls the C++ tool)
"""


def load(results_dir):
    raise NotImplementedError("analyze_results.load: skeleton only")


def main():
    raise NotImplementedError("analyze_results: skeleton only. See docs/project-plan.md Day 8.")


if __name__ == "__main__":
    main()
