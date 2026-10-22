#!/usr/bin/env python3
"""plot_results.py - draw the figures from saved tables only.   (Plan: Day 8; EXP-003)

IDEA
    Reads the CSV tables that analyze_results.py wrote (results/<exp>/tables/*.csv) and draws four figures
    into results/figures/. It never runs the C++ tool and never needs the raw JSON, so the figures can be
    regenerated from the saved results alone.

DESIGN RULES APPLIED (the project's data-visualisation guidance)
    * one series -> one colour; two or three series -> fixed categorical slots 1 and 2, validated for colour-blind
      separation (blue #2a78d6, orange #eb6834: worst adjacent CVD delta E 24.7, normal-vision 33.6);
    * emphasis where one thing is the story (figure 3): context in grey, the highlighted states in slot 1;
    * never a second y axis: two measures -> two panels side by side (figures 2 and 4);
    * a legend whenever there are two or more series, plus a few direct labels, never a number on every point;
    * thin marks (2 px lines, bars <= 24 px, markers >= 8 px with a 2 px surface ring), solid hairline grid,
      text in text colours (never the series colour);
    * light surface. (A dark-surface variant would need its own validated steps: not produced here.)

USAGE
    python plot_results.py [--results ../results] [--figures ../results/figures] [--dpi 200]
"""

import argparse
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402
import pandas as pd  # noqa: E402
from matplotlib.patches import PathPatch  # noqa: E402
from matplotlib.path import Path as MplPath  # noqa: E402

PROJECT_ROOT = Path(__file__).resolve().parent.parent

SURFACE = "#fcfcfb"
TEXT_PRIMARY = "#0b0b0b"
TEXT_SECONDARY = "#52514e"
TEXT_MUTED = "#757470"
GRID = "#e8e7e3"
SERIES_1 = "#2a78d6"   # categorical slot 1 (blue)
SERIES_2 = "#eb6834"   # categorical slot 2 (orange)
CONTEXT_GREY = "#b4b3ad"


class PlotError(RuntimeError):
    pass


def style_axes(ax, x_grid=False, y_grid=True):
    """Quiet chrome: hairline solid grid one step off the surface, no box, small recessive ticks."""
    ax.set_facecolor(SURFACE)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color(GRID)
        ax.spines[side].set_linewidth(1.0)
    ax.tick_params(colors=TEXT_SECONDARY, labelsize=9, length=3, width=1, color=GRID)
    ax.grid(False)
    if y_grid:
        ax.yaxis.grid(True, color=GRID, linewidth=1.0, linestyle="-")
    if x_grid:
        ax.xaxis.grid(True, color=GRID, linewidth=1.0, linestyle="-")
    ax.set_axisbelow(True)


def new_figure(width, height, title, subtitle):
    fig = plt.figure(figsize=(width, height), facecolor=SURFACE)
    fig.text(0.02, 0.965, title, fontsize=13, fontweight="bold", color=TEXT_PRIMARY, va="top")
    fig.text(0.02, 0.905, subtitle, fontsize=9.5, color=TEXT_SECONDARY, va="top")
    return fig


def points_to_data_x(ax, points):
    """How many x-axis data units a length of `points` typographic points covers (axes limits must be set)."""
    fig = ax.figure
    width_pts = ax.get_position().width * fig.get_figwidth() * 72.0
    low, high = ax.get_xlim()
    return points * (high - low) / width_pts


def rounded_hbar(ax, y, value, thickness_pt=14.0, radius_pt=3.0, color=SERIES_1):
    """A horizontal bar growing from x = 0: square at the baseline, 4 px (3 pt) rounded at the data end.

    Drawn as an explicit path (straight sides, quadratic-curve corners) in data coordinates, so the corner
    radius is the same number of points horizontally and vertically whatever the axis scales are."""
    ax_height_in = ax.get_position().height * ax.figure.get_figheight()
    low, high = ax.get_ylim()
    half = (thickness_pt / 72.0) / ax_height_in * (high - low) / 2.0
    ry = radius_pt / 72.0 / ax_height_in * (high - low)
    rx = min(points_to_data_x(ax, radius_pt), value)
    y0, y1 = y - half, y + half
    vertices = [(0, y0), (value - rx, y0), (value, y0), (value, y0 + ry), (value, y1 - ry), (value, y1),
                (value - rx, y1), (0, y1), (0, y0)]
    codes = [MplPath.MOVETO, MplPath.LINETO, MplPath.CURVE3, MplPath.CURVE3, MplPath.LINETO, MplPath.CURVE3,
             MplPath.CURVE3, MplPath.LINETO, MplPath.CLOSEPOLY]
    ax.add_patch(PathPatch(MplPath(vertices, codes), facecolor=color, linewidth=0, clip_on=False))


def ringed_markers(ax, x, y, color, size_pt=8.0, zorder=5):
    """Filled markers (>= 8 px) with a 2 px ring in the surface colour so they stay legible where they overlap."""
    ax.plot(x, y, linestyle="none", marker="o", markersize=size_pt, markerfacecolor=color,
            markeredgecolor=SURFACE, markeredgewidth=1.5, zorder=zorder)


def read_table(results, experiment, name):
    path = Path(results) / experiment / "tables" / f"{name}.csv"
    if not path.is_file():
        raise PlotError(f"missing {path}: run analyze_results.py first")
    return pd.read_csv(path)


def save(fig, figures_dir, name, dpi):
    Path(figures_dir).mkdir(parents=True, exist_ok=True)
    path = Path(figures_dir) / f"{name}.png"
    fig.savefig(path, dpi=dpi, facecolor=SURFACE)
    plt.close(fig)
    return path


# ----------------------------------------------------------------------------------------------------------
# Figure 1 - redundancy versus reliability: one series, one colour, value at the tip of each bar
# ----------------------------------------------------------------------------------------------------------

NICE_NAMES = {"series6": "Six units in series", "satellite": "Satellite: three redundant pairs",
              "one_pair": "One redundant pair", "parallel6": "Six units, any one suffices"}


def figure_1(results, figures_dir, dpi):
    table = read_table(results, "exp1", "exp1_systems").sort_values("mttf")
    fig = new_figure(8.0, 3.9, "Redundancy lengthens expected life",
                     "Mean time to failure of the all-working state; independent exponential lifetimes, a unit lives 1 on average")
    ax = fig.add_axes([0.30, 0.14, 0.64, 0.66])
    ax.set_xlim(0, float(table["mttf"].max()) * 1.18)
    ax.set_ylim(-0.6, len(table) - 0.4)
    style_axes(ax, x_grid=True, y_grid=False)
    for position, (_, row) in enumerate(table.iterrows()):
        rounded_hbar(ax, position, float(row["mttf"]), color=SERIES_1)
        ax.text(float(row["mttf"]) + points_to_data_x(ax, 6), position, f"{row['mttf']:.3f}", va="center",
                fontsize=9.5, color=TEXT_PRIMARY)
    ax.set_yticks(range(len(table)))
    ax.set_yticklabels([NICE_NAMES.get(s, s) for s in table["system"]], fontsize=9.5, color=TEXT_PRIMARY)
    ax.tick_params(axis="y", length=0)
    ax.set_xlabel("expected time until the system fails", fontsize=9, color=TEXT_SECONDARY)
    return save(fig, figures_dir, "fig1_redundancy", dpi)


# ----------------------------------------------------------------------------------------------------------
# Figure 2 - independent versus simultaneous failures: two panels, never a second axis
# ----------------------------------------------------------------------------------------------------------

def figure_2(results, figures_dir, dpi):
    curves = read_table(results, "exp2", "exp2_curves")
    fig = new_figure(9.6, 4.5, "Common-cause failures erode the benefit of redundancy",
                     "Satellite under two common-cause models, each calibrated to the same pairwise beta factor; "
                     "every component lives 1 on average")
    panels = [("redundancy_ratio", "satellite ÷ series MTTF", "benefit of redundancy falls as beta grows"),
              ("satellite_mttf", "satellite MTTF", "absolute MTTF rises as beta grows")]
    styles = [("lsh", "Lethal shock", SERIES_1), ("bfr", "Binomial failure rate", SERIES_2)]
    for index, (column, ylabel, note) in enumerate(panels):
        ax = fig.add_axes([0.075 + index * 0.475, 0.14, 0.40, 0.64])
        style_axes(ax)
        ax.set_xlim(-0.02, 0.56)
        low, high = float(curves[column].min()), float(curves[column].max())
        pad = (high - low) * 0.12
        ax.set_ylim(low - pad, high + pad)
        for model, label, color in styles:
            frame = curves[curves["model"] == model].sort_values("beta")
            ax.plot(frame["beta"], frame[column], color=color, linewidth=2.0, solid_capstyle="round",
                    solid_joinstyle="round", label=label, zorder=3)
            ringed_markers(ax, frame["beta"], frame[column], color)
            last = frame.iloc[-1]
            ax.text(float(last["beta"]) + 0.012, float(last[column]), f"{last[column]:.2f}", fontsize=9,
                    color=TEXT_PRIMARY, va="center")
        ax.set_xlabel("pairwise beta factor  (0 = independent failures)", fontsize=9, color=TEXT_SECONDARY)
        ax.set_ylabel(ylabel, fontsize=9, color=TEXT_SECONDARY)
        ax.set_title(note, fontsize=9.5, color=TEXT_PRIMARY, loc="left", pad=8)
        if index == 0:
            ax.legend(frameon=False, fontsize=9, labelcolor=TEXT_PRIMARY, loc="lower left", handlelength=1.6)
    return save(fig, figures_dir, "fig2_common_cause", dpi)


# ----------------------------------------------------------------------------------------------------------
# Figure 3 - same number of failures, different reliability: emphasis (grey context, highlighted points)
# ----------------------------------------------------------------------------------------------------------

def figure_3(results, figures_dir, dpi):
    states = read_table(results, "exp3", "exp3_states")
    # one dot per distinct (failure count, MTTF) value; its area says how many states share it
    states["mttf_key"] = states["mttf"].round(9)
    dots = states.groupby(["failed_count", "mttf_key"]).agg(count=("mask", "size"), example=("failed", "first")).reset_index()
    spread = states.groupby("failed_count")["mttf_key"].nunique()
    highlighted_counts = set(spread[spread > 1].index)

    fig = new_figure(8.0, 4.6, "The number of failures does not tell the whole story",
                     "MTTF of each of the satellite's 64 states, by number of failed components; dot area = number of states")
    ax = fig.add_axes([0.085, 0.14, 0.88, 0.66])
    style_axes(ax)
    ax.set_xlim(-0.5, 6.5)
    ax.set_ylim(-0.13, 0.78)
    for _, row in dots.iterrows():
        area = 40 + 26 * row["count"]
        color = SERIES_1 if row["failed_count"] in highlighted_counts else CONTEXT_GREY
        ax.scatter([row["failed_count"]], [row["mttf_key"]], s=area, color=color, edgecolor=SURFACE, linewidth=1.5, zorder=4)

    def label(failed_count, value, text, dx=0.16, dy=0.0, centered=False):
        if centered:  # below the dot
            ax.text(failed_count, value + dy, text, fontsize=9, color=TEXT_PRIMARY, ha="center", va="top")
        else:
            ax.text(failed_count + dx, value + dy, text, fontsize=9, color=TEXT_PRIMARY, va="center")

    label(0, 0.7, "all working: 0.700")
    label(2, 5 / 12, "12 states still worth 0.417\n(P1 + C1 failed, for example)")
    label(3, 1 / 3, "8 states worth 0.333")
    label(2, 0.0, "3 states\nworth nothing", dy=-0.04, centered=True)   # a whole pair has failed
    label(3, 0.0, "12 states\nworth nothing", dy=-0.05, centered=True)
    ax.set_xticks(range(7))
    ax.set_xlabel("number of failed components", fontsize=9, color=TEXT_SECONDARY)
    ax.set_ylabel("expected remaining life (MTTF)", fontsize=9, color=TEXT_SECONDARY)
    return save(fig, figures_dir, "fig3_equal_failure_counts", dpi)


# ----------------------------------------------------------------------------------------------------------
# Figure 4 - scaling: two panels (time; work and memory), log scales
# ----------------------------------------------------------------------------------------------------------

def figure_4(results, figures_dir, dpi):
    table = read_table(results, "exp4", "exp4_scaling")
    n = table["n"].to_numpy(dtype=float)
    fig = new_figure(9.6, 4.6, "Exact analysis of every state, at the cost of the 2ⁿ state space",
                     "Systems of n/2 redundant pairs, one core, Release build; both panels use a logarithmic y axis")

    # panel 1: time
    ax = fig.add_axes([0.085, 0.14, 0.40, 0.64])
    style_axes(ax)
    ax.set_yscale("log")
    analysis = table["analysis_ms"].to_numpy()
    ax.plot(n, analysis, color=SERIES_1, linewidth=2.0, solid_capstyle="round", label="analysis: all counting vectors and MTTFs", zorder=3)
    ringed_markers(ax, n, analysis, SERIES_1)
    verified = table.dropna(subset=["verification_ms"])
    ax.plot(verified["n"], verified["verification_ms"], color=SERIES_2, linewidth=2.0, solid_capstyle="round",
            label="verification: brute-force reference", zorder=3)
    ringed_markers(ax, verified["n"], verified["verification_ms"], SERIES_2)
    reference = analysis[-1] * (n ** 2 * 2.0 ** n) / (n[-1] ** 2 * 2.0 ** n[-1])
    ax.plot(n, reference, color=CONTEXT_GREY, linewidth=1.5, label="n² · 2ⁿ, scaled to the last point", zorder=2)
    ax.set_xlabel("number of components n", fontsize=9, color=TEXT_SECONDARY)
    ax.set_ylabel("time (milliseconds)", fontsize=9, color=TEXT_SECONDARY)
    ax.set_title("time", fontsize=9.5, color=TEXT_PRIMARY, loc="left", pad=8)
    ax.legend(frameon=False, fontsize=8.2, labelcolor=TEXT_PRIMARY, loc="lower right")
    ax.set_xticks(n[::2])

    # panel 2: work and memory held
    ax2 = fig.add_axes([0.575, 0.14, 0.40, 0.64])
    style_axes(ax2)
    ax2.set_yscale("log")
    ax2.plot(n, table["phi_calls"], color=SERIES_1, linewidth=2.0, solid_capstyle="round",
             label="evaluations of the structure function  (= 2ⁿ)", zorder=3)
    ringed_markers(ax2, n, table["phi_calls"], SERIES_1)
    ax2.plot(n, table["peak_vectors_held"], color=SERIES_2, linewidth=2.0, solid_capstyle="round",
             label="counting vectors held at once  (two levels)", zorder=3)
    ringed_markers(ax2, n, table["peak_vectors_held"], SERIES_2)
    ax2.set_xlabel("number of components n", fontsize=9, color=TEXT_SECONDARY)
    ax2.set_ylabel("count", fontsize=9, color=TEXT_SECONDARY)
    ax2.set_title("work and memory", fontsize=9.5, color=TEXT_PRIMARY, loc="left", pad=8)
    ax2.legend(frameon=False, fontsize=8.2, labelcolor=TEXT_PRIMARY, loc="upper left")
    ax2.set_xticks(n[::2])
    return save(fig, figures_dir, "fig4_scaling", dpi)


FIGURES = {"exp1": figure_1, "exp2": figure_2, "exp3": figure_3, "exp4": figure_4}


def main(argv=None):
    parser = argparse.ArgumentParser(description="Draw the figures from saved result tables.")
    parser.add_argument("--results", default=str(PROJECT_ROOT / "results"))
    parser.add_argument("--figures", default=None, help="output folder (default: <results>/figures)")
    parser.add_argument("--dpi", type=int, default=200)
    args = parser.parse_args(argv)
    figures = args.figures or str(Path(args.results) / "figures")
    plt.rcParams["font.family"] = "DejaVu Sans"
    try:
        for name, draw in FIGURES.items():
            if (Path(args.results) / name / "tables").is_dir():
                print(f"drew {draw(args.results, figures, args.dpi)}")
    except PlotError as error:
        print(f"plot_results: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
