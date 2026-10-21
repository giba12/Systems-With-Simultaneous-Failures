"""plot_results.py - draw the figures from saved results only.   (Plan: Day 8; EXP-003)

PSEUDO-CODE
    main(results_dir, figures_dir):
        load the CSV/JSON files from results_dir        (does NOT run the C++ tool)
        figure 1: bar chart of initial MTTF for series, pair, six-component systems
        figure 2: line chart of redundancy ratio versus beta
        figure 3: scatter of MTTF per state, x = number of failed components
                  (spread within one x value shows why the count alone is not enough)
        figure 4: log-scale runtime and number of Phi calls versus n (expect ~2^n growth)
        save each as PNG in figures_dir, with the configuration and seed in the file name
        (uses Matplotlib; follow the dataviz guidance for colours when this is written)
"""


def main():
    raise NotImplementedError("plot_results: skeleton only. See docs/project-plan.md Day 8.")


if __name__ == "__main__":
    main()
