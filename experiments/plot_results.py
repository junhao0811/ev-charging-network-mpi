#!/usr/bin/env python3
"""
Task 3 graphs and summary tables.

Reads the comm_timing.csv files written by run_sweep.sh and produces:
  results/plots/delay_<message>.png    one figure per message type:
                                       left = grid sizes (Task 3a), right = line topology (Task 3b)
  results/plots/version_comparison.png average delay of each version, per message type
  results/plots/period_effect.png      effect of the predefined period (Task 2 note)
  results/plots/caas_vs_local.png      CAAS against this machine (only if results/caas exists)
  results/summary.md                   the same numbers as tables, for the slides and report

Every point is the mean over the repeated runs of one configuration; error bars are
one standard deviation between those runs.

Usage:  python3 plot_results.py [results_dir]      (default: results/ next to this script)
Needs:  matplotlib  (pip install matplotlib)
"""
import csv
import math
import os
import statistics
import sys
from collections import defaultdict

import matplotlib

matplotlib.use("Agg")  # Write image files; no window needed
import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__))
RESULTS = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "results")
PLOTS = os.path.join(RESULTS, "plots")

# The three versions, in a fixed order with fixed colours and markers
VERSIONS = ["blocking", "nonblocking_iprobe", "nonblocking_irecv"]
STYLE = {
    "blocking": dict(color="#d62728", marker="o", label="blocking"),
    "nonblocking_iprobe": dict(color="#1f77b4", marker="s", label="non-blocking (MPI_Iprobe BS)"),
    "nonblocking_irecv": dict(color="#2ca02c", marker="^", label="non-blocking (MPI_Irecv BS)"),
}

# Message types. Each entry: file name, figure title, and the CSV columns to plot
# (one row of subplots per column). Incident report delays only exist for runs where
# at least one report was answered.
MESSAGES = [
    ("neighbour_exchange", "Neighbour availability exchange (node <-> up to 4 neighbours)",
     [("avg_exchange_ms", "Avg exchange time (ms)")]),
    ("status_update", "Status update (node -> Base Station, one way)",
     [("avg_status_delay_ms", "Avg one-way delay (ms)")]),
    ("incident_report", "Incident report -> Base Station reply",
     [("avg_report_rtt_ms", "Avg round trip (ms)"),
      ("avg_report_comm_ms", "Avg communication only\n(round trip - BS processing) (ms)")]),
    ("stop_check", "Stop check collective (MPI_Reduce + MPI_Bcast, every tick)",
     [("avg_stopcheck_ms", "Avg stop check time (ms)")]),
    ("terminate_ack", "Terminate signal -> acknowledgement",
     [("avg_shutdown_ms", "Avg time to acknowledge (ms)")]),
]
REPORT_COLUMNS = {"avg_report_rtt_ms", "avg_report_comm_ms", "max_report_rtt_ms", "max_report_comm_ms"}


# -----------------------------------------------------------------------------
# Loading and averaging
# -----------------------------------------------------------------------------
def load(experiment):
    """Rows of results/<experiment>/comm_timing.csv with numbers converted; [] if missing."""
    path = os.path.join(RESULTS, experiment, "comm_timing.csv")
    if not os.path.exists(path):
        return []
    rows = []
    with open(path, newline="") as f:
        for r in csv.DictReader(f):
            row = {"version": r["version"]}
            for k, v in r.items():
                if k != "version":
                    row[k] = float(v)
            row["answered"] = row["suggest_replies"] + row["no_node_replies"]
            rows.append(row)
    return rows


def value(row, column):
    """A row's value for a column, or None when it was not measured in that run."""
    if column in REPORT_COLUMNS and row["answered"] == 0:
        return None  # No answered incident report, so no round trip was measured
    return row[column]


def aggregate(rows, key, column):
    """{key(row): (mean, stdev, runs)} over the rows that measured the column."""
    groups = defaultdict(list)
    for r in rows:
        v = value(r, column)
        if v is not None:
            groups[key(r)].append(v)
    return {k: (statistics.mean(v), statistics.stdev(v) if len(v) > 1 else 0.0, len(v))
            for k, v in groups.items()}


def series(rows, version, column):
    """Sorted x (nodes), mean, stdev for one version."""
    agg = aggregate([r for r in rows if r["version"] == version], lambda r: int(r["nodes"]), column)
    xs = sorted(agg)
    return xs, [agg[x][0] for x in xs], [agg[x][1] for x in xs]


def fit(xs, ys):
    """Least-squares slope and Pearson correlation of ys against xs."""
    if len(xs) < 3:
        return float("nan"), float("nan")
    mx, my = statistics.mean(xs), statistics.mean(ys)
    sxx = sum((x - mx) ** 2 for x in xs)
    syy = sum((y - my) ** 2 for y in ys)
    sxy = sum((x - mx) * (y - my) for x, y in zip(xs, ys))
    slope = sxy / sxx if sxx else float("nan")
    r = sxy / math.sqrt(sxx * syy) if sxx and syy else float("nan")
    return slope, r


# -----------------------------------------------------------------------------
# Figures
# -----------------------------------------------------------------------------
def plot_message(name, title, metrics, grid, line):
    """One figure per message type: rows = metrics, left = grid, right = line."""
    fig, axes = plt.subplots(len(metrics), 2, figsize=(12, 4.2 * len(metrics)), squeeze=False)
    for i, (column, ylabel) in enumerate(metrics):
        for j, (rows, topo) in enumerate([(grid, "grid (m x n)"), (line, "line (1 x N)")]):
            ax = axes[i][j]
            for v in VERSIONS:
                xs, ys, es = series(rows, v, column)
                if xs:
                    ax.errorbar(xs, ys, yerr=es, capsize=2, markersize=4, linewidth=1.2, **STYLE[v])
            ax.set_xlabel(f"Number of EV nodes, {topo}")
            ax.set_ylabel(ylabel)
            ax.set_title(("Task 3a: " if j == 0 else "Task 3b: ") + topo)
            ax.grid(alpha=0.3)
            ax.set_ylim(bottom=0)
            if i == 0 and j == 0:
                ax.legend(fontsize=8)
    fig.suptitle(title, fontsize=13)
    fig.tight_layout()
    fig.savefig(os.path.join(PLOTS, f"delay_{name}.png"), dpi=150)
    plt.close(fig)


def plot_version_comparison(grid, line):
    """Grouped bars: mean over all sizes of each message type's delay, per version."""
    metrics = [(m[2][-1][0], m[0].replace("_", "\n")) for m in MESSAGES]
    fig, axes = plt.subplots(1, 2, figsize=(13, 4.5))
    for ax, rows, topo in [(axes[0], grid, "grid sizes"), (axes[1], line, "line sizes")]:
        width = 0.27
        for k, v in enumerate(VERSIONS):
            means = []
            for column, _ in metrics:
                vals = [value(r, column) for r in rows if r["version"] == v]
                vals = [x for x in vals if x is not None]
                means.append(statistics.mean(vals) if vals else 0.0)
            pos = [i + (k - 1) * width for i in range(len(metrics))]
            ax.bar(pos, means, width, color=STYLE[v]["color"], label=STYLE[v]["label"])
        ax.set_xticks(range(len(metrics)))
        ax.set_xticklabels([m[1] for m in metrics], fontsize=8)
        ax.set_ylabel("Mean delay over all sizes (ms, log scale)")
        ax.set_yscale("log")
        ax.set_title(f"Average over all {topo}")
        ax.grid(axis="y", alpha=0.3)
    axes[0].legend(fontsize=8)
    fig.suptitle("Version comparison per message type", fontsize=13)
    fig.tight_layout()
    fig.savefig(os.path.join(PLOTS, "version_comparison.png"), dpi=150)
    plt.close(fig)


def plot_period(rows):
    """Effect of the predefined period on the Base Station's decisions."""
    periods = sorted({int(r["period_ms"]) for r in rows})
    fig, axes = plt.subplots(1, 2, figsize=(12, 4.2))
    for v in VERSIONS:
        vr = [r for r in rows if r["version"] == v]
        pct, alerts = [], []
        for p in periods:
            pr = [r for r in vr if int(r["period_ms"]) == p]
            answered = sum(r["answered"] for r in pr)
            pct.append(100.0 * sum(r["no_node_replies"] for r in pr) / answered if answered else 0.0)
            alerts.append(statistics.mean(r["alerts"] for r in pr) if pr else 0.0)
        xs = range(len(periods))
        axes[0].plot(xs, pct, **STYLE[v])
        axes[1].plot(xs, alerts, **STYLE[v])
    for ax, ylabel in [(axes[0], "'No nodes available' replies (% of replies)"),
                       (axes[1], "Incident reports per run (mean)")]:
        ax.set_xticks(range(len(periods)))
        ax.set_xticklabels([str(p) for p in periods])
        ax.set_xlabel("Predefined period (ms)")
        ax.set_ylabel(ylabel)
        ax.grid(alpha=0.3)
        ax.set_ylim(bottom=0)
    axes[0].legend(fontsize=8)
    grid = rows[0]
    fig.suptitle(f"Effect of the predefined period ({int(grid['rows'])}x{int(grid['cols'])} grid)", fontsize=13)
    fig.tight_layout()
    fig.savefig(os.path.join(PLOTS, "period_effect.png"), dpi=150)
    plt.close(fig)


def plot_caas(caas, local):
    """CAAS (solid) against this machine (dashed) for the grid sizes run on CAAS."""
    caas_grid = [r for r in caas if r["rows"] > 1]
    sizes = {int(r["nodes"]) for r in caas_grid}
    local_grid = [r for r in local if int(r["nodes"]) in sizes]
    fig, axes = plt.subplots(1, len(MESSAGES), figsize=(4 * len(MESSAGES), 4))
    for ax, (name, title, metrics) in zip(axes, MESSAGES):
        column = metrics[-1][0]
        for v in VERSIONS:
            for rows, ls, tag in [(caas_grid, "-", "CAAS"), (local_grid, "--", "local")]:
                xs, ys, _ = series(rows, v, column)
                if xs:
                    ax.plot(xs, ys, linestyle=ls, color=STYLE[v]["color"], marker=STYLE[v]["marker"],
                            markersize=4, label=f"{STYLE[v]['label']} ({tag})")
        ax.set_title(name.replace("_", " "), fontsize=10)
        ax.set_xlabel("Number of EV nodes (grid)")
        ax.set_ylabel("ms")
        ax.grid(alpha=0.3)
        ax.set_ylim(bottom=0)
    axes[0].legend(fontsize=6)
    fig.suptitle("CAAS (solid) vs local machine (dashed)", fontsize=13)
    fig.tight_layout()
    fig.savefig(os.path.join(PLOTS, "caas_vs_local.png"), dpi=150)
    plt.close(fig)


# -----------------------------------------------------------------------------
# Summary tables (Markdown)
# -----------------------------------------------------------------------------
def scaling_table(out, grid, line):
    out.append("## Scaling per message type (Task 3a grid, Task 3b line)\n")
    out.append("Slope = least-squares increase in mean delay per 10 extra nodes; "
               "r = correlation between delay and node count (1 = grows steadily with size).\n")
    for name, title, metrics in MESSAGES:
        for column, _ in metrics:
            out.append(f"### {title}: `{column}`\n")
            out.append("| Topology | Version | Smallest size (ms) | Largest size (ms) | Slope (ms / 10 nodes) | r |")
            out.append("|---|---|---|---|---|---|")
            for rows, topo in [(grid, "grid"), (line, "line")]:
                for v in VERSIONS:
                    xs, ys, _ = series(rows, v, column)
                    if not xs:
                        continue
                    slope, r = fit(xs, ys)
                    out.append(f"| {topo} | {v} | {ys[0]:.3f} ({xs[0]} nodes) | {ys[-1]:.3f} ({xs[-1]} nodes) "
                               f"| {slope * 10:.3f} | {r:.2f} |")
            out.append("")


def full_table(out, rows, topo):
    out.append(f"## All {topo} results (mean over repeats, ms)\n")
    cols = [m[0] for _, _, ms in MESSAGES for m in ms]
    out.append("| Size | Nodes | Version | Runs | " + " | ".join(cols) + " |")
    out.append("|---|---|---|---|" + "---|" * len(cols))
    keyf = lambda r: (int(r["nodes"]), int(r["rows"]), int(r["cols"]), r["version"])
    keys = sorted({keyf(r) for r in rows}, key=lambda k: (k[0], VERSIONS.index(k[3])))
    for nodes, m, n, v in keys:
        sel = [r for r in rows if keyf(r) == (nodes, m, n, v)]
        cells = []
        for c in cols:
            vals = [value(r, c) for r in sel]
            vals = [x for x in vals if x is not None]
            cells.append(f"{statistics.mean(vals):.3f}" if vals else "-")
        out.append(f"| {m}x{n} | {nodes} | {v} | {len(sel)} | " + " | ".join(cells) + " |")
    out.append("")


def period_table(out, rows):
    out.append("## Predefined period (Task 2 note)\n")
    out.append("| Period (ms) | Version | Reports | Replies with suggestions | 'No nodes' replies | 'No nodes' % |")
    out.append("|---|---|---|---|---|---|")
    for p in sorted({int(r["period_ms"]) for r in rows}):
        for v in VERSIONS:
            sel = [r for r in rows if int(r["period_ms"]) == p and r["version"] == v]
            if not sel:
                continue
            alerts = sum(r["alerts"] for r in sel)
            sug = sum(r["suggest_replies"] for r in sel)
            no = sum(r["no_node_replies"] for r in sel)
            pct = 100.0 * no / (sug + no) if sug + no else 0.0
            out.append(f"| {p} | {v} | {alerts:.0f} | {sug:.0f} | {no:.0f} | {pct:.1f} |")
    out.append("")


def main():
    grid, line, period, caas = load("grid"), load("line"), load("period"), load("caas")
    if not (grid or line or period or caas):
        sys.exit(f"No results found under {RESULTS}; run run_sweep.sh first.")
    os.makedirs(PLOTS, exist_ok=True)

    if grid or line:
        for name, title, metrics in MESSAGES:
            plot_message(name, title, metrics, grid, line)
        plot_version_comparison(grid, line)
    if period:
        plot_period(period)
    if caas:
        plot_caas(caas, grid)

    out = ["# Task 3 results summary\n",
           f"Runs: grid {len(grid)}, line {len(line)}, period {len(period)}, CAAS {len(caas)}.\n",
           "All delays in milliseconds, measured with MPI_Wtime. Status update delays use clocks "
           "synchronised with the Base Station at start-up; the incident report communication time is "
           "the round trip minus the Base Station's processing time, so it still includes the time a "
           "message waits before the Base Station picks it up.\n"]
    if grid or line:
        scaling_table(out, grid, line)
    if period:
        period_table(out, period)
    if caas:
        full_table(out, caas, "CAAS")
    if grid:
        full_table(out, grid, "grid")
    if line:
        full_table(out, line, "line")
    with open(os.path.join(RESULTS, "summary.md"), "w") as f:
        f.write("\n".join(out))
    print(f"Graphs written to {PLOTS}, tables to {os.path.join(RESULTS, 'summary.md')}")


if __name__ == "__main__":
    main()
