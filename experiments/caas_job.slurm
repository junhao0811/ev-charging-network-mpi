#!/bin/bash
# =============================================================================
# CAAS job for Task 3: runs every version on 3x3, 4x4, 6x6, 8x8 grids and on
# 1x8, 1x16, 1x32, 1x64 lines, as advised by the teaching team (4-5 grid sizes
# on CAAS; the full 30+ size sweep is run locally).
#
# Submit from the experiments folder:   sbatch caas_job.slurm
# Check the queue:                squeue -u $USER
# Output:                         results/caas/  and  caas_experiments_<jobid>.out
#
# Partition limits (scontrol show partition defq): at most 20 minutes per job,
# 14 nodes x 16 CPUs, 100 MB memory per CPU by default.
# This job: 8 topologies x 3 versions x 2 repeats = 48 runs of about 6 s each,
# roughly 5-6 minutes in total.
# =============================================================================
#SBATCH --job-name=ev_network
#SBATCH --partition=defq
#SBATCH --time=00:20:00
#SBATCH --ntasks=65
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=200M
#SBATCH --output=caas_experiments_%j.out

# 65 MPI processes = largest run (8x8 grid = 64 EV nodes + 1 Base Station).
# Each EV node also starts 6 short-lived OpenMP threads per tick for its ports.

# Load MPI. If this name is wrong, list the available ones with:  module avail
module load openmpi

cd "$SLURM_SUBMIT_DIR" || exit 1
echo "Job $SLURM_JOB_ID on nodes: $SLURM_JOB_NODELIST"

# Plain mpirun: Open MPI places the processes on the CPUs Slurm allocated
MPIRUN="mpirun" REPEATS=2 bash run_sweep.sh caas
