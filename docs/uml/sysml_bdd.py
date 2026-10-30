#!/usr/bin/env python3
"""sysml_bdd.py - draws the SysML block definition diagram of the Space System Reliability Analyzer.

Source of docs/uml/sysml-bdd.png and docs/uml/sysml-bdd.svg (the other diagrams in this folder are PlantUML;
PlantUML has no SysML profile, so this one is drawn directly with matplotlib, which sizes every box from
the measured width of its text).

    cd project && .venv/bin/python ../docs/uml/sysml_bdd.py          (needs matplotlib)

SysML 1.x notation: <<block>> boxes with compartments, filled diamond = composition (part), hollow triangle =
generalization, dashed arrow = dependency, plain line = association. The blocks mirror the modules in
project/include/reliability and project/src; the values quoted (limits, version) come from the code.
"""
import math
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon, Rectangle

W, H, DPI = 2000, 1530, 100
OUT = Path(__file__).resolve().parent

INK, FILL, GRID = "#12355b", "#f6f9fd", "#b8c6d9"
ACTOR_FILL, ABSTRACT = "#fff6e5", "#eef3f9"

matplotlib.rcParams["svg.fonttype"] = "none"
fig = plt.figure(figsize=(W / DPI, H / DPI), dpi=DPI)
ax = fig.add_axes([0, 0, 1, 1])
ax.set_xlim(0, W)
ax.set_ylim(H, 0)
ax.axis("off")
renderer = fig.canvas.get_renderer()
PX = DPI / 72.0  # pixels per point


def text_w(s, size, **kw):
    t = ax.text(0, 0, s, fontsize=size, **kw)
    w = t.get_window_extent(renderer).width
    t.remove()
    return w


class Block:
    def __init__(self, x, y, name, stereo="block", sections=(), abstract=False, fill=FILL, min_w=0):
        self.x, self.y, self.name, self.stereo = x, y, name, stereo
        self.sections, self.abstract, self.fill = sections, abstract, fill
        pad = 10
        widths = [text_w(f"«{stereo}»", 9, style="italic"),
                  text_w(name, 11.5, weight="bold", style="italic" if abstract else "normal") + (text_w("  {abstract}", 11.5, weight="bold", style="italic") + 8 if abstract else 0)]
        for label, lines in sections:
            widths.append(text_w(label, 8, weight="bold") + 4)
            widths += [text_w(line, 9) for line in lines]
        self.w = max(max(widths) + 2 * pad, min_w)
        self.head_h = (9 + 11.5) * PX * 1.35 + 10
        self.sec_h = [(8 * 1.5 + 9 * 1.5 * len(lines)) * PX * 0.95 + 8 for _, lines in sections]
        self.h = self.head_h + sum(self.sec_h)

    def anchor(self, side, frac=0.5):
        if side == "t":
            return (self.x + self.w * frac, self.y)
        if side == "b":
            return (self.x + self.w * frac, self.y + self.h)
        if side == "l":
            return (self.x, self.y + self.h * frac)
        return (self.x + self.w, self.y + self.h * frac)

    def draw(self):
        ax.add_patch(Rectangle((self.x, self.y), self.w, self.h, fc=self.fill, ec=INK, lw=1.4, zorder=3))
        cx = self.x + self.w / 2
        ax.text(cx, self.y + 8, f"«{self.stereo}»", ha="center", va="top", fontsize=9, style="italic",
                color=INK, zorder=4)
        ax.text(cx, self.y + 8 + 9 * PX * 1.3, self.name + ("  {abstract}" if self.abstract else ""), ha="center",
                va="top", fontsize=11.5, weight="bold", style="italic" if self.abstract else "normal",
                color=INK, zorder=4)
        y = self.y + self.head_h
        for (label, lines), h in zip(self.sections, self.sec_h):
            ax.plot([self.x, self.x + self.w], [y, y], color=INK, lw=0.9, zorder=4)
            ax.text(self.x + 10, y + 5, label, ha="left", va="top", fontsize=8, weight="bold", color="#5b6f87", zorder=4)
            ty = y + 5 + 8 * PX * 1.5
            for line in lines:
                ax.text(self.x + 10, ty, line, ha="left", va="top", fontsize=9, color="#1c1c1c", zorder=4)
                ty += 9 * PX * 1.5 * 0.95
            y += h


def unit(p, q):
    dx, dy = q[0] - p[0], q[1] - p[1]
    d = math.hypot(dx, dy) or 1
    return dx / d, dy / d


def label(p, q, s, dx=0, dy=0, size=8.5, frac=0.5):
    ax.text(p[0] + (q[0] - p[0]) * frac + dx, p[1] + (q[1] - p[1]) * frac + dy, s, ha="center", va="center",
            fontsize=size, style="italic", color=INK, zorder=6,
            bbox=dict(fc="white", ec="none", pad=1.5))


def composition(parent, child, mult=None, p_mult=None):
    d = unit(parent, child)
    n = (-d[1], d[0])
    L, Wd = 26, 8
    pts = [parent, (parent[0] + d[0] * L / 2 + n[0] * Wd, parent[1] + d[1] * L / 2 + n[1] * Wd),
           (parent[0] + d[0] * L, parent[1] + d[1] * L),
           (parent[0] + d[0] * L / 2 - n[0] * Wd, parent[1] + d[1] * L / 2 - n[1] * Wd)]
    ax.plot([pts[2][0], child[0]], [pts[2][1], child[1]], color=INK, lw=1.3, zorder=2)
    ax.add_patch(Polygon(pts, fc=INK, ec=INK, zorder=5))
    if mult:
        ax.text(child[0] + 9, child[1] - 14, mult, fontsize=8.5, color=INK, zorder=6,
                ha="left", va="center", bbox=dict(fc="white", ec="none", pad=1))


def generalization(parent, child):
    d = unit(parent, child)
    n = (-d[1], d[0])
    L, Wd = 22, 11
    base = (parent[0] + d[0] * L, parent[1] + d[1] * L)
    tri = [parent, (base[0] + n[0] * Wd, base[1] + n[1] * Wd), (base[0] - n[0] * Wd, base[1] - n[1] * Wd)]
    ax.plot([base[0], child[0]], [base[1], child[1]], color=INK, lw=1.3, zorder=2)
    ax.add_patch(Polygon(tri, fc="white", ec=INK, lw=1.3, zorder=5))


def arrowhead(tip, frm, size=11, filled=False):
    d = unit(frm, tip)
    n = (-d[1], d[0])
    a = (tip[0] - d[0] * size + n[0] * size * 0.45, tip[1] - d[1] * size + n[1] * size * 0.45)
    b = (tip[0] - d[0] * size - n[0] * size * 0.45, tip[1] - d[1] * size - n[1] * size * 0.45)
    if filled:
        ax.add_patch(Polygon([tip, a, b], fc=INK, ec=INK, zorder=5))
    else:
        ax.plot([a[0], tip[0], b[0]], [a[1], tip[1], b[1]], color=INK, lw=1.3, zorder=5)


def dependency(frm, to, text=None, dx=0, dy=0, via=None):
    pts = [frm] + ([via] if via else []) + [to]
    ax.plot([p[0] for p in pts], [p[1] for p in pts], color=INK, lw=1.2, ls=(0, (5, 3)), zorder=2)
    arrowhead(to, pts[-2])
    if text:
        mid = via if via else ((frm[0] + to[0]) / 2, (frm[1] + to[1]) / 2)
        label(mid, mid, text, dx, dy)


def association(a, b, text=None, dx=0, dy=0, arrow=False):
    ax.plot([a[0], b[0]], [a[1], b[1]], color=INK, lw=1.2, zorder=2)
    if arrow:
        arrowhead(b, a)
    if text:
        label(a, b, text, dx, dy)


# ---------------------------------------------------------------- blocks
frame_x0, frame_y0 = 20, 20
analyst = Block(330, 95, "ReliabilityAnalyst", "actor", fill=ACTOR_FILL)
verifier = Block(1720, 95, "Verifier", "actor", fill=ACTOR_FILL)

soi = Block(700, 80, "SpaceSystemReliabilityAnalyzer", "system", [
    ("parts", ["cli : AnalyzeCLI [1]", "library : ReliabilityLibrary [1]", "experiments : ExperimentLayer [1]",
               "audit : TraceabilityChecker [1]"]),
    ("values", ["version : String = \"0.8.0\"", "maxComponents : Integer = 24"]),
    ("constraints", ["{Φ is evaluated exactly 2ⁿ times per analysis}",
                     "{every result carries its configuration, seed and tool version}"]),
])
row1 = 400
exper = Block(60, row1, "ExperimentLayer", "block", [
    ("parts", ["runner : run_experiments.py", "analysis : analyze_results.py", "plots : plot_results.py"]),
    ("constraints", ["{never re-implements the mathematics}", "{refuses to run without a hypothesis file}"]),
])
cli = Block(470, row1, "AnalyzeCLI", "block", [
    ("operations", ["parseArgs() : Options", "run() : ExitStatus", "writeResult() : JSON"]),
    ("values", ["exitStatus : {0 ok, 1 invalid, 2 usage, 3 verification failed}"]),
])
lib = Block(1090, row1, "ReliabilityLibrary", "subsystem", [
    ("parts", ["model : SystemModel", "sweep : SignatureEngine", "mttf : MttfEngine", "failure : FailureModel",
               "reference : BruteForceReference", "policy : PolicyModule"]),
])
audit = Block(1650, row1, "TraceabilityChecker", "block", [
    ("operations", ["check() : Report"]),
    ("constraints", ["{requirement → test case", " → test code, no orphans}"]),
])

row2 = 790
ref = Block(40, row2, "BruteForceReference", "block", [
    ("operations", ["bruteForceCountingVectors()", "bruteForceMinimalPathSets()", "solveMttfDirect()  // Eq. (6)"]),
    ("values", ["maxN : Integer = 16"]),
    ("constraints", ["{never calls sweep()}"]),
])
model = Block(0, row2, "SystemModel", "block", [
    ("parts", ["components : Component [2..24]"]),
    ("values", ["n : Integer {2..24}", "phiCalls : Integer"]),
    ("operations", ["phi(x : StateMask) : Boolean", "validate() : void"]),
    ("constraints", ["{Φ(0)=0, Φ(1)=1, monotone}"]),
])
sig = Block(0, row2, "SignatureEngine", "block", [
    ("values", ["phiCalls : Integer = 2ⁿ", "levelsHeld : Integer = 2"]),
    ("operations", ["sweep(observer) : SweepResult", "signature(W) : Real[n]", "tailSignature(W) : Real[n+1]"]),
    ("constraints", ["{exact integer counting vectors}"]),
])
mt = Block(0, row2, "MttfEngine", "block", [
    ("operations", ["mttf(W : CountingVector) : Real", "computeAllMttf() : Real[2ⁿ]"]),
    ("constraints", ["{Eq. (7); all terms ≥ 0}"]),
])
fm = Block(0, row2, "FailureModel", "block", [
    ("operations", ["exitRate(w) : Real", "transitionProb(w1, w2) : Real", "increment(w, k) : Real"]),
], abstract=True, fill=ABSTRACT)
pol = Block(0, row2, "PolicyModule", "block", [
    ("operations", ["evaluatePolicy()", "searchThresholdPolicies()", "optimalStatePolicy()"]),
    ("values", ["maxN : Integer = 20", "costs : RepairCosts{component, system, setup}"]),
])

# lay the second row out left to right with a fixed gap
x = 60
gap = 74
for blk in (ref, model, sig, mt, fm, pol):
    blk.x = x
    x += blk.w + gap
row2_right = x - gap

row3 = 1150
comp = Block(model.x, row3, "Component", "block", [
    ("values", ["id : String", "name : String", "role : String", "bit : Integer"]),
])
exp = Block(fm.x - 80, row3, "IndependentExponential", "block", [("values", ["lambda : Real > 0"])])
levy = Block(0, row3, "LevyFrailty", "block", [
    ("values", ["beta : Real", "maxN : Integer = 16"]),
    ("parts", ["psi : lethal shock, binomial failure rate,", "  LFMO-C, LFMO-G, LFMO-IG, LFMO-S"]),
    ("constraints", ["{refuses models whose kernel is not a distribution}"]),
])
levy.x = exp.x + exp.w + 30
if levy.x + levy.w > W - 40:
    shift = levy.x + levy.w - (W - 40)
    exp.x -= shift
    levy.x -= shift

# ---------------------------------------------------------------- frame and title
fw, fh = W - 2 * frame_x0, H - 2 * frame_y0 - 30
ax.add_patch(Rectangle((frame_x0, frame_y0), fw, fh, fc="none", ec=INK, lw=1.8, zorder=1))
tab = "bdd [Package] SpaceSystemReliabilityAnalyzer [Structure of the analyzer]"
tw = text_w(tab, 10.5, weight="bold") + 28
ax.add_patch(Polygon([(frame_x0, frame_y0), (frame_x0 + tw, frame_y0), (frame_x0 + tw + 14, frame_y0 + 14),
                      (frame_x0 + tw + 14, frame_y0 + 30), (frame_x0, frame_y0 + 30)], fc="white", ec=INK, lw=1.4, zorder=2))
ax.text(frame_x0 + 12, frame_y0 + 15, tab, ha="left", va="center", fontsize=10.5, weight="bold", color=INK, zorder=3)

for blk in (analyst, verifier, soi, exper, cli, lib, audit, ref, model, sig, mt, fm, pol, comp, exp, levy):
    blk.draw()

# ---------------------------------------------------------------- relationships
# system of interest -> its four parts (composition)
for frac, child, mult in ((0.12, exper, "1"), (0.38, cli, "1"), (0.64, lib, "1"), (0.90, audit, "1")):
    composition(soi.anchor("b", frac), child.anchor("t", 0.5), mult)

# actors
association(analyst.anchor("r", 0.5), soi.anchor("l", 0.2), "uses", dy=-12)
association(analyst.anchor("b", 0.35), exper.anchor("t", 0.75), "runs", dx=-22)
association(analyst.anchor("b", 0.75), cli.anchor("t", 0.2), "runs", dx=26)
association(verifier.anchor("b", 0.5), audit.anchor("t", 0.75), "runs", dx=24)
association(verifier.anchor("l", 0.5), soi.anchor("r", 0.2), "reads results of", dy=-12)

# row 1 dependencies
dependency(exper.anchor("r", 0.45), cli.anchor("l", 0.45), "«invokes»", dy=-12)
dependency(cli.anchor("r", 0.45), lib.anchor("l", 0.45), "«use»", dy=-12)

# library -> six modules (composition)
fracs = (0.08, 0.26, 0.44, 0.62, 0.80, 0.94)
for frac, child in zip(fracs, (ref, model, sig, mt, fm, pol)):
    composition(lib.anchor("b", frac), child.anchor("t", 0.5), "1")

# row 2 relationships
dependency(ref.anchor("r", 0.2), model.anchor("l", 0.2), "«use» Φ", dy=-13)
dependency(sig.anchor("l", 0.2), model.anchor("r", 0.2), "«use» Φ", dy=-13)
dependency(sig.anchor("r", 0.2), mt.anchor("l", 0.2), "observer", dy=-13)
dependency(mt.anchor("r", 0.2), fm.anchor("l", 0.2), "«use»\nδ(w,k)", dy=-20)
dependency(pol.anchor("l", 0.2), fm.anchor("r", 0.2), "«use»\nν, P", dy=-20)
# composition: SystemModel -> Component
composition(model.anchor("b", 0.5), comp.anchor("t", 0.5), "2..24")

# generalization: FailureModel <- IndependentExponential, LevyFrailty
generalization(fm.anchor("b", 0.35), exp.anchor("t", 0.5))
generalization(fm.anchor("b", 0.65), levy.anchor("t", 0.5))

# ---------------------------------------------------------------- legend and notes
lx, ly = 40, H - 70
ax.text(lx, ly, "Notation (SysML 1.x block definition diagram)", fontsize=9, weight="bold", color=INK, va="center")
ax.plot([lx + 470, lx + 530], [ly, ly], color=INK, lw=1.3)
ax.add_patch(Polygon([(lx + 470, ly), (lx + 483, ly + 4), (lx + 496, ly), (lx + 483, ly - 4)], fc=INK, ec=INK))
ax.text(lx + 540, ly, "composition (part)", fontsize=9, va="center")
ax.add_patch(Polygon([(lx + 720, ly), (lx + 738, ly + 8), (lx + 738, ly - 8)], fc="white", ec=INK))
ax.plot([lx + 738, lx + 780], [ly, ly], color=INK, lw=1.3)
ax.text(lx + 790, ly, "generalization", fontsize=9, va="center")
ax.plot([lx + 910, lx + 970], [ly, ly], color=INK, lw=1.2, ls=(0, (5, 3)))
arrowhead((lx + 970, ly), (lx + 910, ly))
ax.text(lx + 980, ly, "dependency", fontsize=9, va="center")
ax.plot([lx + 1080, lx + 1140], [ly, ly], color=INK, lw=1.2)
ax.text(lx + 1150, ly, "association", fontsize=9, va="center")
ax.text(lx + 1280, ly, "Blocks mirror project/include/reliability and project/src. The analysed satellite is fictional.",
        fontsize=9, va="center", color="#444")

fig.savefig(OUT / "sysml-bdd.png", dpi=150)
fig.savefig(OUT / "sysml-bdd.svg")
print("wrote", OUT / "sysml-bdd.png", "and", OUT / "sysml-bdd.svg", file=sys.stderr)
