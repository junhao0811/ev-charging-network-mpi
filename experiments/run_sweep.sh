#!/usr/bin/env bash
# =============================================================================
# Task 3 experiment runner.
#
# Builds both programs, then runs every version (blocking, nonblocking_iprobe,
# nonblocking_irecv) on a set of topologies. Each run appends one row of timing
# results to comm_timing.csv; this script collects those rows and a copy of every
# Base Station log, per experiment, under results/<experiment>/.
#
# Experiments:
#   grid    32 grid sizes, 2x2 (4 nodes) up to 9x9 (81 nodes)       -> Task 3a
#   line    32 line topologies, 1x2 up to 1x64 nodes               -> Task 3b
#   period  4x4 grid with 7 predefined period values               -> Task 2 note
#   caas    3x3, 4x4, 6x6, 8x8 grids + 4 lines, for the CAAS job   -> rubric (CAAS)
#   all     grid + line + period (default; about 1 hour with 3 repeats)
#
# Usage (from any directory):
#   bash run_sweep.sh [grid|line|period|caas|all]
#
# Settings (environment variables):
#   REPEATS        runs per configuration (default 3)
#   DURATION_S     simulation length of each run in seconds (default 4)
#   PERIOD_MS      predefined period for the grid / line / caas runs (default 500)
#   RUN_TIMEOUT_S  a run still going after this many seconds is stopped and
#                  recorded as failed (default 120)
#   MPIRUN         launcher (default "mpirun --oversubscribe"; on CAAS "mpirun")
#   RESULTS_DIR    where results go (default: results/ next to this script)
#
# On macOS, mpicc needs an OpenMP-capable compiler: if OPENMP_CC is set, it is
# passed to Open MPI's mpicc as OMPI_CC.
# =============================================================================
set -u

HERE=$(cd "$(dirname "$0")" && pwd)
SRC=$(cd "$HERE/.." && pwd)
RESULTS=${RESULTS_DIR:-"$HERE/results"}
BIN="$RESULTS/bin"

REPEATS=${REPEATS:-3}
DURATION_S=${DURATION_S:-4}
PERIOD_MS=${PERIOD_MS:-500}
RUN_TIMEOUT_S=${RUN_TIMEOUT_S:-120}
MPIRUN=${MPIRUN:-"mpirun --oversubscribe"}
EXPERIMENT=${1:-all}

# Topologies (rows x cols). The grid sizes increase steadily in node count and
# include rectangular grids; the line sizes are 1 x N.
GRID_SIZES="2x2 2x3 2x4 3x3 2x5 3x4 2x7 3x5 4x4 3x6 4x5 3x7 4x6 5x5 4x7 5x6
            4x8 5x7 6x6 5x8 6x7 5x9 6x8 7x7 6x9 7x8 6x10 8x8 7x10 8x9 8x10 9x9"
LINE_SIZES="1x2 1x4 1x6 1x8 1x10 1x12 1x14 1x16 1x18 1x20 1x22 1x24 1x26 1x28 1x30 1x32
            1x34 1x36 1x38 1x40 1x42 1x44 1x46 1x48 1x50 1x52 1x54 1x56 1x58 1x60 1x62 1x64"
PERIOD_VALUES="0 100 250 500 1000 2000 5000"
PERIOD_GRID="4x4"
CAAS_SIZES="3x3 4x4 6x6 8x8 1x8 1x16 1x32 1x64"

# Versions: run label, program, receive mode argument
VERSIONS="blocking:ev_network_blocking:
nonblocking_iprobe:ev_network_nonblocking:iprobe
nonblocking_irecv:ev_network_nonblocking:irecv"

# -----------------------------------------------------------------------------
# Build both programs
# -----------------------------------------------------------------------------
build() {
    mkdir -p "$BIN"
    if [ -n "${OPENMP_CC:-}" ]; then
        export OMPI_CC="$OPENMP_CC"
    fi
    for prog in ev_network_blocking ev_network_nonblocking; do
        echo "Building $prog ..."
        mpicc -fopenmp -O2 -Wall -Wextra "$SRC/$prog.c" -o "$BIN/$prog" -lm || {
            echo "ERROR: build of $prog failed" >&2
            exit 1
        }
    done
}

# -----------------------------------------------------------------------------
# Prepare a fresh results folder for one experiment. An existing folder is kept
# by renaming it (results/<experiment>.old.<timestamp>), never deleted.
# -----------------------------------------------------------------------------
prepare() {
    local exp=$1
    if [ -e "$RESULTS/$exp" ]; then
        local old="$RESULTS/$exp.old.$(date +%Y%m%d_%H%M%S)"
        mv "$RESULTS/$exp" "$old"
        echo "Previous $exp results moved to $old"
    fi
    mkdir -p "$RESULTS/$exp/work" "$RESULTS/$exp/base_station_logs"
}

# -----------------------------------------------------------------------------
# Run one configuration and record the outcome.
#   run_one <experiment> <label> <program> <mode> <rows> <cols> <period_ms> <repeat>
# The run happens in results/<experiment>/work, where the program appends its
# row to comm_timing.csv and (over)writes its log files.
# -----------------------------------------------------------------------------
run_one() {
    local exp=$1 label=$2 prog=$3 mode=$4 rows=$5 cols=$6 period=$7 rep=$8
    local work="$RESULTS/$exp/work"
    local np=$((rows * cols + 1))
    local args="$rows $cols $period $DURATION_S $mode"
    local start=$(date +%s)

    # perl's alarm stops a run that hangs (macOS has no 'timeout' command)
    ( cd "$work" && perl -e 'alarm shift @ARGV; exec @ARGV' "$RUN_TIMEOUT_S" \
        $MPIRUN -np "$np" "$BIN/$prog" $args < /dev/null > last_run.out 2>&1 )
    # (stdin from /dev/null: mpirun forwards stdin to rank 0 and would otherwise consume
    # the list of versions this script is reading)
    local rc=$?
    local acks=$(grep -c "Acknowledged" "$work/last_run.out")
    local secs=$(( $(date +%s) - start ))

    # Keep this run's Base Station log under a descriptive name
    if [ -f "$work/base_station_$label.log" ]; then
        cp "$work/base_station_$label.log" \
           "$RESULTS/$exp/base_station_logs/${label}_${rows}x${cols}_p${period}_rep${rep}.log"
    fi

    if [ "$rc" -eq 0 ] && [ "$acks" -eq $((np - 1)) ]; then
        echo "ok    (${secs}s)"
    else
        echo "FAILED rc=$rc acks=$acks/$((np - 1)) (${secs}s)"
        echo "$exp $label ${rows}x${cols} period=$period rep=$rep rc=$rc acks=$acks/$((np - 1))" \
            >> "$RESULTS/failures.txt"
        cp "$work/last_run.out" "$RESULTS/$exp/FAILED_${label}_${rows}x${cols}_p${period}_rep${rep}.out"
    fi
}

# -----------------------------------------------------------------------------
# Run one experiment: every size (or period) x every version x REPEATS.
# Versions are interleaved inside each size, so any slowdown of the machine
# affects all versions equally.
# -----------------------------------------------------------------------------
run_experiment() {
    local exp=$1 sizes=$2 periods=$3
    prepare "$exp"
    local total=0 n=0
    for _ in $sizes; do for _ in $periods; do total=$((total + 3 * REPEATS)); done; done

    echo "=== Experiment '$exp': $total runs ==="
    for rep in $(seq 1 "$REPEATS"); do
        for size in $sizes; do
            local rows=${size%x*} cols=${size#*x}
            for period in $periods; do
                while IFS=: read -r label prog mode; do
                    n=$((n + 1))
                    printf "[%s %3d/%d] %-19s %5s period=%-5s rep %d ... " \
                        "$exp" "$n" "$total" "$label" "$size" "$period" "$rep"
                    run_one "$exp" "$label" "$prog" "$mode" "$rows" "$cols" "$period" "$rep"
                done <<< "$VERSIONS"
            done
        done
    done

    # The collected rows: one per successful run
    cp "$RESULTS/$exp/work/comm_timing.csv" "$RESULTS/$exp/comm_timing.csv" 2>/dev/null
    echo "=== '$exp' done: results in $RESULTS/$exp/comm_timing.csv ==="
}

# -----------------------------------------------------------------------------
# Main
# -----------------------------------------------------------------------------
case "$EXPERIMENT" in
    grid|line|period|caas|all) ;;
    *) echo "Usage: bash run_sweep.sh [grid|line|period|caas|all]" >&2; exit 1 ;;
esac

build
mkdir -p "$RESULTS"
echo "Settings: REPEATS=$REPEATS DURATION_S=$DURATION_S PERIOD_MS=$PERIOD_MS MPIRUN='$MPIRUN'"

case "$EXPERIMENT" in
    grid)   run_experiment grid "$GRID_SIZES" "$PERIOD_MS" ;;
    line)   run_experiment line "$LINE_SIZES" "$PERIOD_MS" ;;
    period) run_experiment period "$PERIOD_GRID" "$PERIOD_VALUES" ;;
    caas)   run_experiment caas "$CAAS_SIZES" "$PERIOD_MS" ;;
    all)
        run_experiment grid "$GRID_SIZES" "$PERIOD_MS"
        run_experiment line "$LINE_SIZES" "$PERIOD_MS"
        run_experiment period "$PERIOD_GRID" "$PERIOD_VALUES"
        ;;
esac

if [ -s "$RESULTS/failures.txt" ]; then
    echo "Some runs failed; see $RESULTS/failures.txt"
else
    echo "All runs completed successfully."
fi
