#!/usr/bin/env python3
"""plot_results.py - draw the figures from saved tables only.   (Plan: Day 8; EXP-003)

IDEA
    Reads the CSV tables that analyze_results.py wrote (results/<exp>/tables/*.csv) and draws one figure per experiment
    (seven in all) into results/figures/. It never runs the C++ tool and never needs the raw JSON, so the figures can be
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
from matplotlib.patches import PathPatch, Rectangle  # noqa: E402
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


# ----------------------------------------------------------------------------------------------------------
# Figure 5 - repair policies: left, how much the MTTF-threshold policy saves on asymmetric systems; right, which
# policy is best on the symmetric satellite (an ORDERED quantity -> one blue ramp, labelled cells)
# ----------------------------------------------------------------------------------------------------------

ORDINAL_BLUES = ["#86b6ef", "#5598e7", "#2a78d6", "#184f95"]   # ramp steps 250, 350, 450, 600 (ordinal floor respected)
ORDINAL_TEXT = [TEXT_PRIMARY, TEXT_PRIMARY, "#ffffff", "#ffffff"]
POLICY_NAMES = {1: "1 = after the 1st failure", 2: "2 = after the 2nd", 3: "3 = after the 3rd",
                4: "–  = only after the system fails"}
ASYMMETRIC_NAMES = {"fig1": "paper's Fig. 1 system (3 components)", "bridge": "bridge network (5)",
                    "eight": "8-component system"}


def figure_5(results, figures_dir, dpi):
    table = read_table(results, "exp5", "exp5_policies")
    fig = new_figure(10.4, 4.9, "Which components failed matters for repair decisions",
                     "Long-term cost of repair policies; component cost = rho / n of a system failure, set-up cost sigma "
                     "(units of one system failure)")

    # ---- left: saving of the best MTTF-threshold policy over the best 'repair after r failures' rule ----
    ax = fig.add_axes([0.075, 0.15, 0.38, 0.62])
    style_axes(ax)
    colors = {"fig1": SERIES_1, "bridge": SERIES_2, "eight": "#1baf7a"}
    sigma = 0.1
    for name, color in colors.items():
        rows = table[(table["system"] == name) & (table["sigma"] == sigma)].sort_values("rho")
        saving = rows["threshold_saving_over_best_r"].to_numpy() * 100.0
        ax.plot(rows["rho"], saving, color=color, linewidth=2.0, solid_capstyle="round", label=ASYMMETRIC_NAMES[name], zorder=3)
        ringed_markers(ax, rows["rho"], saving, color)
    ax.set_xscale("log")
    ax.set_xticks([0.1, 0.2, 0.5, 1, 2, 4, 10])
    ax.set_xticklabels(["0.1", "0.2", "0.5", "1", "2", "4", "10"])
    ax.minorticks_off()
    ax.set_ylim(-1.0, max(26.0, float(table["threshold_saving_over_best_r"].max() * 100) + 3))
    ax.axhline(0, color=GRID, linewidth=1.0)
    ax.set_xlabel(f"rho  (component cost ÷ system-failure cost),  sigma = {sigma}", fontsize=8.6, color=TEXT_SECONDARY)
    ax.set_ylabel("saving of the best threshold policy\nover the best 'repair after r failures' (%)", fontsize=9, color=TEXT_SECONDARY)
    ax.set_title("asymmetric systems: the MTTF threshold saves money", fontsize=9.5, color=TEXT_PRIMARY, loc="left", pad=8)
    ax.legend(frameon=False, fontsize=8.4, labelcolor=TEXT_PRIMARY, loc="upper right")

    # ---- right: the best policy on the satellite, every cost pair ----
    ax2 = fig.add_axes([0.575, 0.15, 0.40, 0.62])
    sat = table[(table["system"] == "satellite") & (table["model"] == "exp")]
    rhos = sorted(sat["rho"].unique())
    sigmas = sorted(sat["sigma"].unique())
    ax2.set_facecolor(SURFACE)
    for side in ax2.spines.values():
        side.set_visible(False)
    gap = 0.04
    for xi, rho in enumerate(rhos):
        for yi, sig in enumerate(sigmas):
            r = int(sat[(sat["rho"] == rho) & (sat["sigma"] == sig)]["best_r"].iloc[0])
            level = min(r, 4) - 1
            ax2.add_patch(Rectangle((xi + gap / 2, yi + gap / 2), 1 - gap, 1 - gap, facecolor=ORDINAL_BLUES[level], linewidth=0))
            ax2.text(xi + 0.5, yi + 0.5, str(r) if r < 4 else "–", ha="center", va="center", fontsize=9.5,
                     color=ORDINAL_TEXT[level], fontweight="bold")
    ax2.set_xlim(0, len(rhos))
    ax2.set_ylim(0, len(sigmas))
    ax2.set_xticks([i + 0.5 for i in range(len(rhos))])
    ax2.set_xticklabels([f"{v:g}" if v != 2 / 3 and v != 4 / 3 else {2 / 3: "⅔", 4 / 3: "1⅓"}[v] for v in rhos], fontsize=8.5, color=TEXT_SECONDARY)
    ax2.set_yticks([i + 0.5 for i in range(len(sigmas))])
    ax2.set_yticklabels([f"{v:g}" for v in sigmas], fontsize=8.5, color=TEXT_SECONDARY)
    ax2.tick_params(length=0)
    ax2.set_xlabel("rho  (component cost ÷ system-failure cost)", fontsize=8.6, color=TEXT_SECONDARY)
    ax2.set_ylabel("sigma  (set-up cost per repair visit)", fontsize=9, color=TEXT_SECONDARY)
    ax2.set_title("satellite: the cheapest moment to repair", fontsize=9.5, color=TEXT_PRIMARY, loc="left", pad=8)
    handles = [Rectangle((0, 0), 1, 1, facecolor=ORDINAL_BLUES[i], linewidth=0) for i in range(4)]
    fig.legend(handles, [POLICY_NAMES[i + 1] for i in range(4)], loc="lower center", ncol=4, frameon=False, fontsize=8.4,
               labelcolor=TEXT_PRIMARY, bbox_to_anchor=(0.5, -0.005), handlelength=1.2, columnspacing=1.6)
    return save(fig, figures_dir, "fig5_repair_policies", dpi)


MODEL_LABELS = {"lsh": "LSh", "bfr": "BFR", "lfmo-c": "LFMO-C", "lfmo-g": "LFMO-G", "lfmo-ig": "LFMO-IG", "lfmo-s": "LFMO-S"}


def figure_6(results, figures_dir, dpi):
    table = read_table(results, "exp6", "exp6_cases")
    fig = new_figure(10.4, 4.2, "Same pairwise beta, different systems: the models rank in opposite order",
                     "Series system of six components (x) against the satellite (y), one dot per failure model; "
                     "MTTF with all components working")
    betas = sorted(table["beta"].unique())
    for i, beta in enumerate(betas):
        ax = fig.add_axes([0.065 + i * 0.325, 0.14, 0.26, 0.6])
        style_axes(ax, x_grid=True)
        rows = table[table["beta"] == beta]
        x, y = rows["series6_mttf"].to_numpy(), rows["satellite_mttf"].to_numpy()
        ringed_markers(ax, x, y, SERIES_1)
        xpad, ypad = (x.max() - x.min()) * 0.28, (y.max() - y.min()) * 0.2
        ax.set_xlim(x.min() - xpad * 0.4, x.max() + xpad)
        ax.set_ylim(y.min() - ypad, y.max() + ypad)
        # the dots run from upper left to lower right; alternate the labels between the two free sides so that
        # dots that sit close together keep readable names
        for k, idx in enumerate(np.argsort(x)):
            above = k % 2 == 0
            ax.annotate(MODEL_LABELS[rows["model"].iloc[idx]], (x[idx], y[idx]), textcoords="offset points",
                        xytext=(8, 5) if above else (-8, -13), ha="left" if above else "right", fontsize=8.4,
                        color=TEXT_PRIMARY)
        ax.set_title(f"pairwise beta = {beta:g}", fontsize=9.5, color=TEXT_PRIMARY, loc="left", pad=8)
        ax.set_xlabel("series system MTTF  (higher = larger bundles)", fontsize=8.6, color=TEXT_SECONDARY)
        if i == 0:
            ax.set_ylabel("satellite MTTF", fontsize=9, color=TEXT_SECONDARY)
    return save(fig, figures_dir, "fig6_six_models", dpi)


def figure_7(results, figures_dir, dpi):
    table = read_table(results, "exp7", "exp7_combinations")
    random_rows = table[table["group"] == "random"]
    fig = new_figure(10.4, 4.6, "How much does the MTTF threshold lose against the optimal policy?",
                     "Excess cost of the best MTTF-threshold policy over the optimal state-based policy; "
                     "16 random systems of 6 and 10 components, 45 cost pairs each")

    # ---- left: mean excess by component cost, one line per failure model ----
    ax = fig.add_axes([0.075, 0.16, 0.40, 0.58])
    style_axes(ax)
    colors = {"exp": SERIES_1, "lsh": SERIES_2, "bfr": "#1baf7a"}
    names = {"exp": "independent failures", "lsh": "lethal shock, beta 0.5", "bfr": "binomial failure rate, beta 0.5"}
    for model, color in colors.items():
        rows = random_rows[random_rows["model"] == model].groupby("rho")["excess"].mean().sort_index()
        ax.plot(rows.index, rows.to_numpy() * 100.0, color=color, linewidth=2.0, solid_capstyle="round", label=names[model], zorder=3)
        ringed_markers(ax, rows.index, rows.to_numpy() * 100.0, color)
    ax.set_xscale("log")
    ax.set_xticks([0.1, 0.2, 0.5, 1, 2, 4, 10])
    ax.set_xticklabels(["0.1", "0.2", "0.5", "1", "2", "4", "10"])
    ax.minorticks_off()
    ax.set_ylim(bottom=0)
    ax.set_xlabel("rho  (component cost \u00f7 system-failure cost)", fontsize=8.6, color=TEXT_SECONDARY)
    ax.set_ylabel("mean excess cost (%)", fontsize=9, color=TEXT_SECONDARY)
    ax.set_title("largest for independent failures with cheap components", fontsize=9.5, color=TEXT_PRIMARY, loc="left", pad=8)
    ax.legend(frameon=False, fontsize=8.4, labelcolor=TEXT_PRIMARY, loc="upper right")

    # ---- right: the worst cost pair of every random system under independent failures ----
    ax2 = fig.add_axes([0.575, 0.16, 0.40, 0.58])
    style_axes(ax2, x_grid=False)
    exp_rows = random_rows[random_rows["model"] == "exp"]
    worst = exp_rows.groupby(["n", "system"])["excess"].max().reset_index()
    worst["order"] = worst["system"].str.extract(r"_(\d+)$").astype(int)
    worst = worst.sort_values(["n", "order"]).reset_index(drop=True)
    x = np.arange(len(worst)) + (worst["n"] == 10).to_numpy() * 1.0  # a gap between the two sizes
    ax2.bar(x, worst["excess"].to_numpy() * 100.0, width=0.62, color=SERIES_1, zorder=3)
    ax2.set_xticks([x[: len(worst) // 2].mean(), x[len(worst) // 2:].mean()])
    ax2.set_xticklabels(["6 components", "10 components"], fontsize=8.6, color=TEXT_SECONDARY)
    ax2.set_ylabel("largest excess over the 45 cost pairs (%)", fontsize=9, color=TEXT_SECONDARY)
    ax2.set_title("worst case of each system (independent failures)", fontsize=9.5, color=TEXT_PRIMARY, loc="left", pad=8)
    return save(fig, figures_dir, "fig7_distance_to_optimal", dpi)


FIGURES = {"exp1": figure_1, "exp2": figure_2, "exp3": figure_3, "exp4": figure_4, "exp5": figure_5, "exp6": figure_6, "exp7": figure_7}


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
