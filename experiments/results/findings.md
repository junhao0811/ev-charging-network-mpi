# Task 3 findings (draft for the slides / report)

Source data: `grid/`, `line/`, `period/` (639 runs, all completed and acknowledged).
Graphs: `plots/`. Full tables: `summary.md`.

## Setup

- Machine: one MacBook (10 CPU cores, 16 GB), Open MPI 5.0.9, shared-memory transport.
- **Grid:** 32 sizes, 2x2 (4 nodes) to 9x9 (81 nodes), rectangular grids included.
- **Line:** 32 sizes, 1x2 to 1x64.
- **Runs:** every size × 3 versions × 3 repeats, 4 s each, 100 ms tick, predefined period 500 ms. Versions are interleaved inside each size, so machine load affects them equally.
- **Plotted values:** each point is the mean over the 3 repeats; error bars are ±1 standard deviation.

## Measurement method (and its limits)

| Message type | How it is measured (MPI_Wtime) | What it includes |
|---|---|---|
| Neighbour exchange | Node: start to end of the exchange with all neighbours | Transfer + waiting for neighbours still busy in their own tick |
| Status update (node → BS) | One-way: BS receive time − node send time. The node's clock is mapped to the BS clock by a start-up clock sync (Cristian's algorithm, 5 rounds) | Transfer + time until the BS picks the message up (detection) |
| Incident report → reply | Node: round trip; BS processing time subtracted for the "communication only" value | Transfer both ways + detection + queueing behind other messages |
| Stop check | Node: `MPI_Reduce` + `MPI_Bcast` duration | Collective cost + waiting for the slowest node to arrive |
| Terminate → ack | BS: terminate sent → acknowledgement received | Node finishing its current tick (up to 100 ms) + transfer |

Limitations to state:
- `MPI_Wtime` is not synchronised across processes; it was measured to differ by about 0.5 ms. The clock sync error is reported per run. Median: 0.001 ms up to 10 nodes, 0.02 ms for 11–40 nodes and 0.09 ms above 40 nodes; worst case 0.48 ms.
- Above 9 nodes there are more processes than cores (oversubscription). Some of the growth at large sizes is CPU sharing, not network cost; CAAS runs avoid this.
- All values include **detection delay** (how quickly the receiver notices a message), not just the transfer time. Detection delay depends on the receive method, which is exactly what separates the versions.
- Local runs use shared memory; CAAS runs cross a real network.

## Results

Means over all sizes (ms):

| | Exchange | Status update | Report round trip | BS processing | Stop check | Terminate → ack |
|---|---|---|---|---|---|---|
| **Grid** blocking | 2.60 | 0.38 | 0.37 | 0.01 | 0.96 | 5.5 |
| **Grid** non-blocking, iprobe | 1.60 | 2.13 | 2.81 | 0.01 | 1.20 | 48.8 |
| **Grid** non-blocking, irecv | 1.67 | 1.01 | 1.73 | 0.01 | 1.18 | 46.1 |
| **Line** blocking | 1.79 | 0.37 | 0.47 | 0.02 | 1.21 | 5.1 |
| **Line** non-blocking, iprobe | 1.27 | 2.13 | 2.88 | 0.01 | 1.85 | 55.9 |
| **Line** non-blocking, irecv | 1.30 | 0.97 | 1.66 | 0.01 | 1.47 | 48.5 |

## a) How does delay scale as the grid grows? (4 → 81 nodes)

- **Neighbour exchange.**
  - Blocking grows: 1.4 → 3.9 ms, +0.37 ms per 10 nodes, r = 0.76.
  - Non-blocking stays almost flat: 1.3 → 2.0 ms, +0.05 ms per 10 nodes.

  Each node only talks to at most 4 neighbours, so the work per node doesn't depend on grid size. Non-blocking posts all transfers at once and overlaps them. In the blocking version, a node waits for its partner in each pairing phase, and a late node delays its partner, which delays that partner's next partner, and so on. These waits chain across the grid, so they get longer as the grid grows.
- **Status update and incident report (node ↔ Base Station).**
  - Blocking BS: lowest, but it **grows** with size. Status: 0.02 → 0.8 ms, r = 0.90. Report round trip: 0.03 → 0.9 ms. One BS process handles every node's messages one after another, so more nodes means more queueing.
  - Non-blocking BS: **set by the polling interval**, and almost independent of size. Status ≈ 2.1 ms (iprobe) and ≈ 0.8–1.2 ms (irecv).
- **BS processing** is about 0.01 ms in every version. The delay is communication and detection, not computation.
- **Stop check (collective):** grows for all versions, 0.05–0.4 → 1.6–2.3 ms, r ≈ 0.5–0.85. Each collective must wait for the slowest of N nodes, and the tree-based `MPI_Reduce` / `MPI_Bcast` has more levels as N grows.
- **Terminate → ack:** about 5 ms (blocking) against about 50 ms (non-blocking), with large variance. This is **not** a speed difference. The blocking BS only checks the clock after receiving a message, and status updates arrive at the start of each tick, so terminate goes out at the start of a tick and the node sees it in the same tick. The polling BS sends terminate at any moment, usually while nodes are sleeping, so the average is about half a tick. Mention this when showing the graph.

**Answer:** delay for per-node messages stays roughly constant as the grid grows when non-blocking communication is used, because the per-node work is fixed at up to 4 neighbours. Delays that funnel into one process grow with N: the blocking BS queue, and the collective stop check. So does the blocking neighbour exchange, because its waits chain across the grid.

## b) How does delay scale in a line topology? (2 → 64 nodes)

- **Neighbour exchange.**
  - Blocking grows even more consistently than in the grid: 0.6 → 2.9 ms, r = 0.88. In a line, the pairing chain runs from one end to the other, so a delay can travel along all N nodes.
  - Non-blocking stays flat: 0.7–1.0 → 1.4 ms.

  Line exchanges are cheaper than grid exchanges at the same size, because each node has at most 2 neighbours, not 4.
- **Status update and incident report:** the same pattern as the grid. The BS doesn't care about the topology, only the number of nodes.
- **Stop check:** grows from about 0.02 ms (2 nodes) to about 2–2.5 ms (64 nodes), slightly higher than a grid of the same size.

**Answer:** in a line, the topology-dependent delay (blocking neighbour exchange) grows more steadily than in a grid, because the waits chain along a single path. Delays to and from the Base Station depend only on the number of nodes, not on the shape.

## Which version is best?

- **EV node:** non-blocking. The exchange is 1.6 vs 2.6 ms (grid mean), stays flat as size grows, and can't deadlock.
- **Base Station:**
  - **Blocking** has the lowest delay, but it only works because every node sends a status update every tick. Otherwise `MPI_Recv` could block forever and the terminate time could never be checked. Its delay also grows with the number of nodes.
  - **Non-blocking with `irecv`** is consistently faster than `iprobe` at every size: status 1.0 vs 2.1 ms, report round trip 1.7 vs 2.8 ms. With a pre-posted receive, MPI delivers the message into the buffer as soon as it arrives, so the next check finds it ready. With `MPI_Iprobe`, a message is only noticed when the probe is called.
  - **Conclusion:** non-blocking node + non-blocking `irecv` Base Station. It doesn't depend on a heartbeat and has the lowest delay among the non-blocking versions.

## Period experiment (4x4 grid, 0–5000 ms)

- With a period of 0, no neighbour report counts as recent, so every reply suggests nodes. That's correct.
- For longer periods, "no nodes available" replies stay rare (0–19%), because incident reports themselves are rare: about 2–10 per run. Neighbours seldom report within the same window. The data is too sparse to show a clear trend. Longer runs, or a low-availability test case, would give more reports per period.
