#!/usr/bin/env python3
"""
Architecture diagrams for the Lab 3 presentation (Task 4a / 4b).

Writes PNG (for slides) and SVG (editable) files next to this script:
  1_architecture.png        processes, communicators and message types (Task 1 + Task 2)
  2_ev_node.png             one EV node: OpenMP port threads + shared array, and the per-tick flow (Task 1)
  3_base_station.png        the Base Station: receive loop versions, message handling, shutdown (Task 2)
  4_info_update.png         the information update mechanism on the specification's Node 0 example (Task 2)

Usage:  python3 make_diagrams.py        (needs matplotlib)
"""
import os

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyArrowPatch, FancyBboxPatch, Polygon

OUT = os.path.dirname(os.path.abspath(__file__))

# Colour scheme shared by all diagrams
NODE_FC, NODE_EC = "#dbeafe", "#1d4ed8"       # EV nodes: blue
BS_FC, BS_EC = "#ffedd5", "#c2410c"           # Base Station: orange
THREAD_FC, THREAD_EC = "#dcfce7", "#15803d"   # OpenMP threads: green
GREY_FC, GREY_EC = "#f3f4f6", "#6b7280"
NBR = "#dc2626"                               # node <-> neighbour messages: red
BSL = "#2563eb"                               # node <-> Base Station messages: blue
TEXT = "#111827"


def canvas(w, h):
    fig, ax = plt.subplots(figsize=(w * 0.85, h * 0.85))
    ax.set_xlim(0, w)
    ax.set_ylim(0, h)
    ax.set_aspect("equal")
    ax.axis("off")
    return fig, ax


def box(ax, x, y, w, h, text="", fc="white", ec=TEXT, fs=10, bold=False, ls="-", lw=1.5, z=2,
        align="center", r=0.12):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle=f"round,pad=0,rounding_size={r}",
                                fc=fc, ec=ec, lw=lw, ls=ls, zorder=z))
    if text:
        if align == "center":
            ax.text(x + w / 2, y + h / 2, text, ha="center", va="center", fontsize=fs,
                    weight="bold" if bold else "normal", color=TEXT, zorder=z + 1, linespacing=1.3)
        else:  # left-aligned, top
            ax.text(x + 0.15, y + h - 0.12, text, ha="left", va="top", fontsize=fs,
                    weight="bold" if bold else "normal", color=TEXT, zorder=z + 1, linespacing=1.3)


def diamond(ax, cx, cy, w, h, text, fc="#fef9c3", ec="#a16207", fs=9):
    ax.add_patch(Polygon([(cx, cy + h / 2), (cx + w / 2, cy), (cx, cy - h / 2), (cx - w / 2, cy)],
                         closed=True, fc=fc, ec=ec, lw=1.5, zorder=2))
    ax.text(cx, cy, text, ha="center", va="center", fontsize=fs, zorder=3, linespacing=1.2)


def arrow(ax, x1, y1, x2, y2, color=TEXT, both=False, lw=1.5, ls="-", rad=0.0, z=1, ms=14):
    ax.add_patch(FancyArrowPatch((x1, y1), (x2, y2), arrowstyle="<|-|>" if both else "-|>",
                                 mutation_scale=ms, color=color, lw=lw, linestyle=ls,
                                 connectionstyle=f"arc3,rad={rad}", zorder=z, shrinkA=0, shrinkB=0))


def label(ax, x, y, text, fs=9, color=TEXT, ha="left", va="center", bold=False, style="normal"):
    ax.text(x, y, text, fontsize=fs, color=color, ha=ha, va=va, weight="bold" if bold else "normal",
            style=style, zorder=5, linespacing=1.3)


def save(fig, name):
    fig.tight_layout(pad=0.3)
    for ext in ("png", "svg"):
        fig.savefig(os.path.join(OUT, f"{name}.{ext}"), dpi=200, bbox_inches="tight")
    plt.close(fig)


# =============================================================================
# 1. Overall architecture
# =============================================================================
def architecture():
    fig, ax = canvas(17, 11.2)
    label(ax, 8.5, 10.9, "System architecture: 3 x 3 example (m x n EV nodes + 1 Base Station = m*n + 1 MPI processes)",
          fs=14, ha="center", bold=True)

    # MPI_COMM_WORLD
    box(ax, 0.3, 2.6, 16.4, 7.9, fc="none", ec=GREY_EC, ls="--", lw=1.5, z=0)
    label(ax, 0.5, 10.25, "MPI_COMM_WORLD  (all processes; nodes and Base Station talk here, point-to-point)",
          fs=10, color=GREY_EC, bold=True)

    # comm2D with the 3x3 grid
    box(ax, 0.7, 3.0, 9.2, 6.9, fc="#f8fafc", ec=NODE_EC, ls="--", lw=1.5, z=0)
    label(ax, 0.9, 9.6, "comm2D: EV nodes only (Task 1)\nMPI_Cart_create, 2D, non-periodic, reorder = 0",
          fs=9.5, color=NODE_EC, bold=True, va="top")
    w, h = 2.1, 1.35
    xs = [1.2, 4.3, 7.4]
    ys = [7.0, 5.1, 3.2]
    for i in range(3):
        for j in range(3):
            r = i * 3 + j
            box(ax, xs[j], ys[i], w, h, f"Node {r}  ({i},{j})\n1 MPI process\n6 ports (OpenMP)",
                fc=NODE_FC, ec=NODE_EC, fs=8.5)
    for i in range(3):          # horizontal neighbour links
        for j in range(2):
            arrow(ax, xs[j] + w, ys[i] + h / 2, xs[j + 1], ys[i] + h / 2, NBR, both=True, lw=2)
    for i in range(2):          # vertical neighbour links
        for j in range(3):
            arrow(ax, xs[j] + w / 2, ys[i], xs[j] + w / 2, ys[i + 1] + h, NBR, both=True, lw=2)

    # Base Station in its own communicator
    box(ax, 11.6, 4.0, 4.7, 4.3, fc="#fffaf5", ec=BS_EC, ls="--", lw=1.5, z=0)
    label(ax, 11.8, 8.05, "Base Station communicator\n(alone; not part of the grid)", fs=9, color=BS_EC,
          bold=True, va="top")
    box(ax, 12.2, 4.5, 3.5, 2.6,
        "Base Station (Task 2)\nrank 9 = size - 1\n1 MPI process, 1 thread\nmaster_io()",
        fc=BS_FC, ec=BS_EC, fs=10, bold=False)

    # Node <-> Base Station link
    arrow(ax, 9.9, 6.2, 12.2, 6.2, BSL, both=True, lw=3, ms=18)
    label(ax, 11.05, 6.55, "every node", fs=8.5, color=BSL, ha="center")
    label(ax, 11.05, 5.85, "point-to-point", fs=8.5, color=BSL, ha="center")

    # Split explanation
    label(ax, 10.3, 3.25,
          "MPI_Comm_split(MPI_COMM_WORLD, rank == size-1, 0, &new_comm)\n"
          "reorder = 0  ->  world rank = grid rank = Node N;  (row, col) = (r / cols, r % cols)",
          fs=8.5, color=GREY_EC, va="center")

    # Legend: message types
    box(ax, 0.3, 0.1, 16.4, 2.25, fc="white", ec=GREY_EC, lw=1)
    label(ax, 0.5, 2.05, "Message types", fs=10, bold=True)
    arrow(ax, 0.6, 1.5, 1.6, 1.5, NBR, both=True, lw=2, z=6)
    label(ax, 1.8, 1.5, "TAG_NEIGHBOR_STATUS: free-port exchange with up to 4 neighbours, every tick (comm2D, found with MPI_Cart_shift)",
          fs=9)
    arrow(ax, 0.6, 0.95, 1.6, 0.95, BSL, lw=2, z=6)
    label(ax, 1.8, 0.95, "node -> BS:  TAG_STATUS_UPDATE (every tick)   TAG_INCIDENT_REPORT (node + all neighbours congested)   "
                         "TAG_TERMINATE (ack)   TAG_CLOCK_SYNC (start-up)", fs=9)
    arrow(ax, 1.6, 0.4, 0.6, 0.4, BSL, lw=2, z=6)
    label(ax, 1.8, 0.4, "BS -> node:  TAG_REDIRECT_INFO (nearest available nodes, or 'none available')   "
                        "TAG_TERMINATE (stop)   TAG_CLOCK_SYNC (reply)", fs=9)
    save(fig, "1_architecture")


# =============================================================================
# 2. Inside one EV node
# =============================================================================
def ev_node():
    fig, ax = canvas(17, 12)
    label(ax, 8.5, 11.7, "EV charging node (Task 1): one MPI process, k = 6 ports simulated by OpenMP threads",
          fs=14, ha="center", bold=True)

    # ---- Left: ports ----
    box(ax, 0.3, 0.3, 7.4, 11.0, fc="#f8fafc", ec=NODE_EC, ls="--", z=0)
    label(ax, 0.5, 11.0, "Inside the process (rank r)", fs=10.5, color=NODE_EC, bold=True)
    box(ax, 0.6, 9.3, 6.8, 1.2, "#pragma omp parallel for num_threads(6)\nshared(ports, port_seeds) reduction(+:free_count)",
        fc="white", ec=THREAD_EC, fs=8.5)
    tw, gap, x0 = 0.95, 1.12, 0.75
    for i in range(6):
        x = x0 + i * gap
        box(ax, x, 7.6, tw, 1.0, f"Thread\n{i}", fc=THREAD_FC, ec=THREAD_EC, fs=8.5)
        arrow(ax, x + tw / 2, 7.6, x + tw / 2, 6.6, THREAD_EC, lw=1.3)
        free = i in (1, 4, 5)
        box(ax, x, 5.6, tw, 1.0, f"ports[{i}]\n{1 if free else 0}", fc="#bbf7d0" if free else "#e5e7eb",
            ec=TEXT, fs=8.5, r=0.05)
    label(ax, 0.75, 8.95, "port i -> thread i, each with its own seed: rand_r(&port_seeds[i])", fs=8.5, color=THREAD_EC)
    label(ax, 0.75, 5.25, "Shared array ports[6]: 1 = free (green), 0 = in use (grey)", fs=8.5, bold=True)
    arrow(ax, 4.0, 5.0, 4.0, 4.35, TEXT)
    box(ax, 1.8, 3.35, 4.4, 1.0, "free_ports = 3   (sum of the array)", fc="white", ec=TEXT, fs=9.5)
    box(ax, 0.6, 0.6, 6.8, 2.3,
        "Each tick, each port independently:\n"
        "  30%  becomes free     (EV leaves)\n"
        "  50%  becomes in use  (EV arrives)\n"
        "  20%  unchanged\n"
        "Congested when free_ports <= 1 (threshold)",
        fc="white", ec=GREY_EC, fs=8.8, align="left")

    # ---- Right: per-tick flow ----
    label(ax, 12.4, 11.0, "Every tick (100 ms)", fs=10.5, bold=True, ha="center")
    cx, bw, bh = 12.4, 6.6, 0.75

    def step(y, text, fc="white", ec=TEXT):
        box(ax, cx - bw / 2, y, bw, bh, text, fc=fc, ec=ec, fs=8.6)

    step(10.0, "1  Update ports with OpenMP threads (left)", fc=THREAD_FC, ec=THREAD_EC)
    step(9.0, "2  Log one row to node_r.log: date, time, availability")
    step(8.0, "3  Status update -> Base Station (TAG_STATUS_UPDATE)", ec=BSL)
    step(7.0, "4  Exchange free ports with up to 4 neighbours (comm2D)\n"
              "    blocking: Send/Recv pairs  |  non-blocking: Isend/Irecv + Waitall", ec=NBR)
    for y1, y2 in [(10.0, 9.75), (9.0, 8.75), (8.0, 7.75)]:
        arrow(ax, cx, y1, cx, y2)
    arrow(ax, cx, 7.0, cx, 6.55)
    diamond(ax, cx, 6.0, 2.6, 1.1, "free_ports\n<= 1 ?")
    diamond(ax, cx, 4.3, 2.8, 1.2, "any neighbour\nfree?")
    arrow(ax, cx, 5.45, cx, 4.9)
    label(ax, cx + 0.15, 5.18, "yes", fs=8.5)
    box(ax, 7.95, 3.95, 2.6, 0.7, "Redirect EVs\nto that neighbour", fc="#dcfce7", ec=THREAD_EC, fs=8)
    arrow(ax, cx - 1.4, 4.3, 10.55, 4.3)
    label(ax, 10.65, 4.5, "yes", fs=8.5)
    arrow(ax, cx, 3.7, cx, 3.2)
    label(ax, cx + 0.15, 3.45, "no", fs=8.5)
    step(2.45, "5  Incident report -> Base Station (TAG_INCIDENT_REPORT)\n"
               "    then wait for its reply (nearest nodes) or terminate", fc=BS_FC, ec=BS_EC)
    arrow(ax, cx, 2.45, cx, 2.0)
    step(1.25, "6  Stop check: MPI_Reduce(MAX) + MPI_Bcast of 'terminate received'\n"
               "    -> all nodes stop on the same tick")
    # "no" from first diamond straight down the right side to the stop check
    ax.plot([cx + 1.3, 15.9, 15.9], [6.0, 6.0, 1.62], color=TEXT, lw=1.5, zorder=1)
    arrow(ax, 15.9, 1.62, cx + bw / 2, 1.62)
    label(ax, cx + 1.45, 6.2, "no", fs=8.5)
    ax.plot([8.5, 8.5], [3.95, 1.62], color=TEXT, lw=1.5, zorder=1)
    arrow(ax, 8.5, 1.62, cx - bw / 2, 1.62)
    # Outcome
    label(ax, cx, 0.65, "stop -> send statistics as acknowledgement, close log, free comm2D\n"
                        "otherwise sleep until the next tick", fs=8.6, ha="center", color=GREY_EC)
    arrow(ax, cx, 1.25, cx, 1.0)
    save(fig, "2_ev_node")


# =============================================================================
# 3. Inside the Base Station
# =============================================================================
def base_station():
    fig, ax = canvas(17, 11.5)
    label(ax, 8.5, 11.2, "Base Station (Task 2): one MPI process, one thread (rank size - 1)", fs=14, ha="center",
          bold=True)

    # 1. Receive loop versions
    label(ax, 0.4, 10.55, "1  Receive loop in master_io(): three versions", fs=10.5, bold=True)
    vers = [
        ("Blocking  (ev_network_blocking.c)", "MPI_Recv(ANY_SOURCE, ANY_TAG)\nwaits until any message arrives"),
        ("Non-blocking, iprobe  (default)", "MPI_Iprobe -> if a message waits:\nMPI_Recv it;  else sleep 1 ms"),
        ("Non-blocking, irecv", "pre-posted MPI_Irecv -> MPI_Test;\ndone: handle + re-post; else sleep 1 ms"),
    ]
    bx = [0.4, 4.25, 8.1]
    for x, (t, d) in zip(bx, vers):
        box(ax, x, 8.45, 3.65, 1.8, f"{t}\n\n{d}", fc=BS_FC, ec=BS_EC, fs=8.5)
        arrow(ax, x + 1.82, 8.45, 6.05, 7.55, BS_EC, lw=1.3)
    box(ax, 12.2, 8.45, 4.4, 1.8,
        "In every loop iteration (all 3 versions):\nbs_check_duration()\nafter duration_s -> MPI_Send\nTAG_TERMINATE to every node",
        fc="#fee2e2", ec=NBR, fs=8.8)

    # 2. Dispatch
    box(ax, 3.4, 6.75, 5.3, 0.8, "2  bs_handle_message(): switch on MPI_TAG", fc="white", ec=TEXT, fs=9.5, bold=True)
    branches = [
        (0.4, "TAG_STATUS_UPDATE  (every tick)",
         "availability[r] = free ports\ninfo_time[r] = now\none-way delay recorded"),
        (5.75, "TAG_INCIDENT_REPORT",
         "handle_incident_report():\ninformation update mechanism\n-> reply TAG_REDIRECT_INFO\n-> log report + decision"),
        (11.1, "TAG_TERMINATE  (acknowledgement)",
         "store the node's statistics\nacks = acks + 1"),
    ]
    for x, t, d in branches:
        box(ax, x, 3.85, 5.0, 2.2, f"{t}\n\n{d}", fc="white", ec=BSL, fs=8.8)
        arrow(ax, 6.05, 6.75, x + 2.5, 6.05, TEXT, lw=1.3)

    # State
    box(ax, 2.5, 2.05, 12.0, 1.25,
        "Base Station state (BaseStation struct): availability[], info_time[], last_report_time[] per node;\n"
        "counters: alerts, replies, messages, processing and delay times; per-node statistics",
        fc=GREY_FC, ec=GREY_EC, fs=8.8)
    for x, _, _ in branches:
        arrow(ax, x + 2.5, 3.85, x + 2.5 if x < 11 else x + 1.5, 3.3, GREY_EC, lw=1.2, ls="--")

    # 3. Finish
    box(ax, 2.5, 0.25, 12.0, 1.3,
        "3  When acks == number of nodes: bs_finish()\n"
        "summary -> base_station_<label>.log,  one row -> comm_timing.csv,  free memory  (graceful shutdown)",
        fc="#dcfce7", ec=THREAD_EC, fs=9)
    arrow(ax, 8.5, 2.05, 8.5, 1.55, TEXT)
    label(ax, 0.4, 2.65, "Only this one thread\nmakes MPI calls\n(spec hint)", fs=8, color=GREY_EC)
    save(fig, "3_base_station")


# =============================================================================
# 4. Information update mechanism
# =============================================================================
def info_update():
    fig, ax = canvas(17, 10.2)
    label(ax, 8.5, 9.95, "Base Station information update mechanism: Node 0 reports (specification example)",
          fs=14, ha="center", bold=True)

    def grid(ox, oy, colours, title, result, paths):
        label(ax, ox + 3.3, oy + 6.5, title, fs=10, ha="center", bold=True)
        s, w = 2.25, 1.55
        for i in range(3):
            for j in range(3):
                n = i * 3 + j
                fc, ec, sub = colours.get(n, (GREY_FC, GREY_EC, ""))
                box(ax, ox + j * s, oy + (2 - i) * s, w, w, f"Node {n}\n({i},{j})\n{sub}", fc=fc, ec=ec, fs=8.5)
        for (a, b) in paths:  # neighbouring cells: arrow from edge to edge
            ai, aj, bi, bj = a // 3, a % 3, b // 3, b % 3
            ax0, ay0 = ox + aj * s, oy + (2 - ai) * s
            bx0, by0 = ox + bj * s, oy + (2 - bi) * s
            if ai == bi:   # same row
                arrow(ax, ax0 + w, ay0 + w / 2, bx0, by0 + w / 2, THREAD_EC, lw=2.2, ls="--", z=4)
            else:          # same column, b below a
                arrow(ax, ax0 + w / 2, ay0, bx0 + w / 2, by0 + w, THREAD_EC, lw=2.2, ls="--", z=4)
        box(ax, ox - 0.1, oy - 1.55, 7.0, 1.3, result, fc="white", ec=TEXT, fs=9)

    red = ("#fecaca", NBR, "REPORTER")
    rep = ("#fed7aa", BS_EC, "reported\nrecently")
    quiet = ("#dbeafe", NODE_EC, "did not\nreport")
    cand = ("#bbf7d0", THREAD_EC, "suggested")
    skip = (GREY_FC, GREY_EC, "only via\nNode 1")

    grid(0.6, 2.4, {0: red, 1: rep, 3: quiet, 4: cand, 6: cand, 2: skip},
         "Case A: Node 1 reported within the period, Node 3 did not",
         "Search through Node 3 (did not report): its neighbours 4 and 6\n"
         "are 2 steps from Node 0 -> reply suggests Nodes 4 and 6",
         [(0, 3), (3, 4), (3, 6)])
    covered = (GREY_FC, GREY_EC, "covered by\nreports")
    grid(9.4, 2.4, {0: red, 1: rep, 3: rep, 2: covered, 4: covered, 6: covered},
         "Case B: Nodes 1 and 3 both reported within the period",
         "All neighbours report congestion, so their neighbours\n(2, 4, 6) are covered -> reply 'no nodes available nearby'",
         [])
    label(ax, 8.5, 0.35,
          "Distance = grid steps (Manhattan). Candidates are distance-2 nodes reached through neighbours that did not report;\n"
          "duplicates removed; a candidate recently known to be congested (status updates) is skipped. "
          "No report within the period (e.g. period = 0) -> always search.",
          fs=8.8, ha="center", color=GREY_EC)
    save(fig, "4_info_update")


if __name__ == "__main__":
    architecture()
    ev_node()
    base_station()
    info_update()
    print("Diagrams written to", OUT)
