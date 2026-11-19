# Task 3 results summary

Runs: grid 288, line 288, period 63, CAAS 0.

All delays in milliseconds, measured with MPI_Wtime. Status update delays use clocks synchronised with the Base Station at start-up; the incident report communication time is the round trip minus the Base Station's processing time, so it still includes the time a message waits before the Base Station picks it up.

## Scaling per message type (Task 3a grid, Task 3b line)

Slope = least-squares increase in mean delay per 10 extra nodes; r = correlation between delay and node count (1 = grows steadily with size).

### Neighbour availability exchange (node <-> up to 4 neighbours): `avg_exchange_ms`

| Topology | Version | Smallest size (ms) | Largest size (ms) | Slope (ms / 10 nodes) | r |
|---|---|---|---|---|---|
| grid | blocking | 1.382 (4 nodes) | 3.937 (81 nodes) | 0.372 | 0.76 |
| grid | nonblocking_iprobe | 1.298 (4 nodes) | 1.989 (81 nodes) | 0.049 | 0.48 |
| grid | nonblocking_irecv | 1.425 (4 nodes) | 1.836 (81 nodes) | 0.055 | 0.40 |
| line | blocking | 0.612 (2 nodes) | 2.920 (64 nodes) | 0.269 | 0.88 |
| line | nonblocking_iprobe | 1.026 (2 nodes) | 1.411 (64 nodes) | 0.042 | 0.66 |
| line | nonblocking_irecv | 0.733 (2 nodes) | 1.376 (64 nodes) | 0.038 | 0.29 |

### Status update (node -> Base Station, one way): `avg_status_delay_ms`

| Topology | Version | Smallest size (ms) | Largest size (ms) | Slope (ms / 10 nodes) | r |
|---|---|---|---|---|---|
| grid | blocking | 0.024 (4 nodes) | 0.817 (81 nodes) | 0.123 | 0.90 |
| grid | nonblocking_iprobe | 2.022 (4 nodes) | 2.487 (81 nodes) | 0.010 | 0.16 |
| grid | nonblocking_irecv | 0.778 (4 nodes) | 1.211 (81 nodes) | 0.047 | 0.47 |
| line | blocking | 0.005 (2 nodes) | 0.728 (64 nodes) | 0.120 | 0.84 |
| line | nonblocking_iprobe | 2.159 (2 nodes) | 2.088 (64 nodes) | 0.004 | 0.09 |
| line | nonblocking_irecv | 0.782 (2 nodes) | 0.918 (64 nodes) | 0.018 | 0.15 |

### Incident report -> Base Station reply: `avg_report_rtt_ms`

| Topology | Version | Smallest size (ms) | Largest size (ms) | Slope (ms / 10 nodes) | r |
|---|---|---|---|---|---|
| grid | blocking | 0.030 (4 nodes) | 0.886 (81 nodes) | 0.102 | 0.77 |
| grid | nonblocking_iprobe | 2.755 (4 nodes) | 3.450 (81 nodes) | 0.036 | 0.38 |
| grid | nonblocking_irecv | 1.265 (4 nodes) | 1.823 (81 nodes) | 0.107 | 0.50 |
| line | blocking | 0.034 (2 nodes) | 0.623 (64 nodes) | 0.118 | 0.58 |
| line | nonblocking_iprobe | 2.546 (2 nodes) | 2.974 (64 nodes) | 0.010 | 0.07 |
| line | nonblocking_irecv | 1.390 (2 nodes) | 1.589 (64 nodes) | 0.086 | 0.54 |

### Incident report -> Base Station reply: `avg_report_comm_ms`

| Topology | Version | Smallest size (ms) | Largest size (ms) | Slope (ms / 10 nodes) | r |
|---|---|---|---|---|---|
| grid | blocking | 0.018 (4 nodes) | 0.876 (81 nodes) | 0.102 | 0.77 |
| grid | nonblocking_iprobe | 2.737 (4 nodes) | 3.441 (81 nodes) | 0.037 | 0.39 |
| grid | nonblocking_irecv | 1.240 (4 nodes) | 1.816 (81 nodes) | 0.107 | 0.51 |
| line | blocking | 0.028 (2 nodes) | 0.616 (64 nodes) | 0.128 | 0.66 |
| line | nonblocking_iprobe | 2.523 (2 nodes) | 2.968 (64 nodes) | 0.012 | 0.08 |
| line | nonblocking_irecv | 1.378 (2 nodes) | 1.584 (64 nodes) | 0.087 | 0.54 |

### Stop check collective (MPI_Reduce + MPI_Bcast, every tick): `avg_stopcheck_ms`

| Topology | Version | Smallest size (ms) | Largest size (ms) | Slope (ms / 10 nodes) | r |
|---|---|---|---|---|---|
| grid | blocking | 0.053 (4 nodes) | 2.020 (81 nodes) | 0.291 | 0.83 |
| grid | nonblocking_iprobe | 0.350 (4 nodes) | 2.307 (81 nodes) | 0.187 | 0.85 |
| grid | nonblocking_irecv | 0.367 (4 nodes) | 1.623 (81 nodes) | 0.112 | 0.50 |
| line | blocking | 0.019 (2 nodes) | 2.410 (64 nodes) | 0.368 | 0.83 |
| line | nonblocking_iprobe | 0.075 (2 nodes) | 2.472 (64 nodes) | 0.318 | 0.82 |
| line | nonblocking_irecv | 0.022 (2 nodes) | 1.833 (64 nodes) | 0.219 | 0.73 |

### Terminate signal -> acknowledgement: `avg_shutdown_ms`

| Topology | Version | Smallest size (ms) | Largest size (ms) | Slope (ms / 10 nodes) | r |
|---|---|---|---|---|---|
| grid | blocking | 3.416 (4 nodes) | 7.663 (81 nodes) | 0.614 | 0.76 |
| grid | nonblocking_iprobe | 66.342 (4 nodes) | 65.356 (81 nodes) | 4.470 | 0.43 |
| grid | nonblocking_irecv | 65.257 (4 nodes) | 16.340 (81 nodes) | 2.835 | 0.27 |
| line | blocking | 0.947 (2 nodes) | 19.946 (64 nodes) | 0.706 | 0.42 |
| line | nonblocking_iprobe | 66.678 (2 nodes) | 45.951 (64 nodes) | 4.023 | 0.38 |
| line | nonblocking_irecv | 69.461 (2 nodes) | 97.378 (64 nodes) | 4.541 | 0.39 |

## Predefined period (Task 2 note)

| Period (ms) | Version | Reports | Replies with suggestions | 'No nodes' replies | 'No nodes' % |
|---|---|---|---|---|---|
| 0 | blocking | 21 | 21 | 0 | 0.0 |
| 0 | nonblocking_iprobe | 20 | 19 | 0 | 0.0 |
| 0 | nonblocking_irecv | 13 | 13 | 0 | 0.0 |
| 100 | blocking | 17 | 16 | 1 | 5.9 |
| 100 | nonblocking_iprobe | 23 | 21 | 2 | 8.7 |
| 100 | nonblocking_irecv | 15 | 15 | 0 | 0.0 |
| 250 | blocking | 21 | 20 | 1 | 4.8 |
| 250 | nonblocking_iprobe | 27 | 25 | 2 | 7.4 |
| 250 | nonblocking_irecv | 15 | 15 | 0 | 0.0 |
| 500 | blocking | 29 | 25 | 4 | 13.8 |
| 500 | nonblocking_iprobe | 12 | 12 | 0 | 0.0 |
| 500 | nonblocking_irecv | 30 | 27 | 2 | 6.9 |
| 1000 | blocking | 9 | 9 | 0 | 0.0 |
| 1000 | nonblocking_iprobe | 28 | 26 | 1 | 3.7 |
| 1000 | nonblocking_irecv | 16 | 14 | 2 | 12.5 |
| 2000 | blocking | 27 | 26 | 1 | 3.7 |
| 2000 | nonblocking_iprobe | 17 | 17 | 0 | 0.0 |
| 2000 | nonblocking_irecv | 15 | 13 | 0 | 0.0 |
| 5000 | blocking | 16 | 13 | 3 | 18.8 |
| 5000 | nonblocking_iprobe | 14 | 14 | 0 | 0.0 |
| 5000 | nonblocking_irecv | 12 | 12 | 0 | 0.0 |

## All grid results (mean over repeats, ms)

| Size | Nodes | Version | Runs | avg_exchange_ms | avg_status_delay_ms | avg_report_rtt_ms | avg_report_comm_ms | avg_stopcheck_ms | avg_shutdown_ms |
|---|---|---|---|---|---|---|---|---|---|
| 2x2 | 4 | blocking | 3 | 1.382 | 0.024 | 0.030 | 0.018 | 0.053 | 3.416 |
| 2x2 | 4 | nonblocking_iprobe | 3 | 1.298 | 2.022 | 2.755 | 2.737 | 0.350 | 66.342 |
| 2x2 | 4 | nonblocking_irecv | 3 | 1.425 | 0.778 | 1.265 | 1.240 | 0.367 | 65.257 |
| 2x3 | 6 | blocking | 3 | 1.539 | 0.011 | 0.063 | 0.055 | 0.104 | 2.619 |
| 2x3 | 6 | nonblocking_iprobe | 3 | 1.675 | 2.187 | 2.836 | 2.808 | 0.566 | 33.400 |
| 2x3 | 6 | nonblocking_irecv | 3 | 1.437 | 0.749 | 1.660 | 1.645 | 0.446 | 5.090 |
| 2x4 | 8 | blocking | 3 | 2.156 | 0.073 | 0.100 | 0.090 | 0.428 | 3.946 |
| 2x4 | 8 | nonblocking_iprobe | 3 | 1.709 | 2.161 | 3.017 | 3.001 | 0.987 | 41.520 |
| 2x4 | 8 | nonblocking_irecv | 3 | 1.679 | 0.875 | 1.550 | 1.533 | 0.841 | 38.573 |
| 3x3 | 9 | blocking | 3 | 3.503 | 0.029 | 0.582 | 0.567 | 1.073 | 6.216 |
| 3x3 | 9 | nonblocking_iprobe | 3 | 2.359 | 2.688 | 2.974 | 2.961 | 1.783 | 79.351 |
| 3x3 | 9 | nonblocking_irecv | 3 | 2.250 | 1.136 | 2.464 | 2.444 | 1.843 | 45.262 |
| 2x5 | 10 | blocking | 3 | 1.693 | 0.126 | 0.068 | 0.054 | 0.347 | 4.274 |
| 2x5 | 10 | nonblocking_iprobe | 3 | 1.362 | 2.035 | 2.691 | 2.681 | 0.696 | 28.681 |
| 2x5 | 10 | nonblocking_irecv | 3 | 1.395 | 0.853 | 1.289 | 1.275 | 1.120 | 104.344 |
| 3x4 | 12 | blocking | 3 | 1.404 | 0.075 | 0.095 | 0.067 | 0.240 | 4.634 |
| 3x4 | 12 | nonblocking_iprobe | 3 | 1.456 | 2.263 | 3.017 | 2.995 | 0.785 | 33.855 |
| 3x4 | 12 | nonblocking_irecv | 3 | 1.303 | 0.780 | 1.469 | 1.460 | 0.539 | 39.041 |
| 2x7 | 14 | blocking | 3 | 1.629 | 0.145 | 0.052 | 0.043 | 0.269 | 4.081 |
| 2x7 | 14 | nonblocking_iprobe | 3 | 1.341 | 2.087 | 2.839 | 2.831 | 0.729 | 12.771 |
| 2x7 | 14 | nonblocking_irecv | 3 | 1.302 | 0.809 | 1.349 | 1.341 | 0.647 | 10.060 |
| 3x5 | 15 | blocking | 3 | 1.583 | 0.104 | 0.072 | 0.053 | 0.318 | 3.479 |
| 3x5 | 15 | nonblocking_iprobe | 3 | 1.331 | 2.061 | 2.504 | 2.495 | 0.726 | 12.916 |
| 3x5 | 15 | nonblocking_irecv | 3 | 1.514 | 0.892 | 1.374 | 1.366 | 0.829 | 25.523 |
| 4x4 | 16 | blocking | 3 | 1.721 | 0.175 | 0.091 | 0.071 | 0.324 | 5.220 |
| 4x4 | 16 | nonblocking_iprobe | 3 | 1.425 | 2.081 | 2.590 | 2.582 | 0.803 | 25.079 |
| 4x4 | 16 | nonblocking_irecv | 3 | 1.360 | 0.848 | 1.593 | 1.575 | 0.637 | 22.970 |
| 3x6 | 18 | blocking | 3 | 2.107 | 0.294 | 0.076 | 0.067 | 0.513 | 2.471 |
| 3x6 | 18 | nonblocking_iprobe | 3 | 1.469 | 2.034 | 2.720 | 2.710 | 0.740 | 11.919 |
| 3x6 | 18 | nonblocking_irecv | 3 | 1.832 | 1.216 | 1.427 | 1.416 | 1.412 | 16.492 |
| 4x5 | 20 | blocking | 3 | 1.791 | 0.220 | 0.085 | 0.074 | 0.402 | 4.593 |
| 4x5 | 20 | nonblocking_iprobe | 3 | 1.367 | 2.023 | 2.723 | 2.709 | 0.807 | 21.379 |
| 4x5 | 20 | nonblocking_irecv | 3 | 1.471 | 0.897 | 1.435 | 1.424 | 0.827 | 22.376 |
| 3x7 | 21 | blocking | 3 | 1.912 | 0.326 | 0.107 | 0.094 | 0.335 | 3.356 |
| 3x7 | 21 | nonblocking_iprobe | 3 | 1.343 | 2.035 | 2.782 | 2.775 | 0.687 | 23.223 |
| 3x7 | 21 | nonblocking_irecv | 3 | 2.501 | 1.678 | 1.537 | 1.529 | 2.521 | 68.949 |
| 4x6 | 24 | blocking | 3 | 1.948 | 0.356 | 0.450 | 0.439 | 0.462 | 4.841 |
| 4x6 | 24 | nonblocking_iprobe | 3 | 1.670 | 2.072 | 2.581 | 2.572 | 0.980 | 48.382 |
| 4x6 | 24 | nonblocking_irecv | 3 | 1.474 | 0.826 | 1.590 | 1.579 | 0.722 | 26.368 |
| 5x5 | 25 | blocking | 3 | 1.917 | 0.169 | 0.096 | 0.087 | 0.422 | 4.932 |
| 5x5 | 25 | nonblocking_iprobe | 3 | 1.539 | 2.218 | 2.655 | 2.648 | 1.122 | 53.858 |
| 5x5 | 25 | nonblocking_irecv | 3 | 1.496 | 0.806 | 1.576 | 1.568 | 0.826 | 30.843 |
| 4x7 | 28 | blocking | 3 | 2.378 | 0.305 | 0.547 | 0.535 | 0.570 | 4.909 |
| 4x7 | 28 | nonblocking_iprobe | 3 | 1.479 | 2.059 | 2.780 | 2.772 | 0.821 | 33.518 |
| 4x7 | 28 | nonblocking_irecv | 3 | 1.395 | 0.826 | 1.496 | 1.485 | 0.838 | 25.941 |
| 5x6 | 30 | blocking | 3 | 2.102 | 0.274 | 0.290 | 0.278 | 0.605 | 4.692 |
| 5x6 | 30 | nonblocking_iprobe | 3 | 1.466 | 2.067 | 2.613 | 2.606 | 1.130 | 36.554 |
| 5x6 | 30 | nonblocking_irecv | 3 | 1.516 | 0.874 | 1.439 | 1.429 | 1.136 | 47.682 |
| 4x8 | 32 | blocking | 3 | 2.709 | 0.435 | 0.251 | 0.233 | 1.053 | 5.663 |
| 4x8 | 32 | nonblocking_iprobe | 3 | 1.526 | 2.045 | 2.721 | 2.713 | 1.309 | 52.564 |
| 4x8 | 32 | nonblocking_irecv | 3 | 1.441 | 0.866 | 1.496 | 1.490 | 0.932 | 35.512 |
| 5x7 | 35 | blocking | 3 | 2.179 | 0.229 | 0.381 | 0.354 | 0.784 | 5.996 |
| 5x7 | 35 | nonblocking_iprobe | 3 | 1.579 | 2.077 | 2.557 | 2.549 | 0.973 | 52.553 |
| 5x7 | 35 | nonblocking_irecv | 3 | 1.646 | 0.942 | 1.485 | 1.479 | 1.113 | 36.382 |
| 6x6 | 36 | blocking | 3 | 1.794 | 0.116 | 0.197 | 0.187 | 0.485 | 5.159 |
| 6x6 | 36 | nonblocking_iprobe | 3 | 1.482 | 2.018 | 2.658 | 2.650 | 0.931 | 45.061 |
| 6x6 | 36 | nonblocking_irecv | 3 | 1.572 | 0.866 | 1.534 | 1.524 | 1.047 | 65.375 |
| 5x8 | 40 | blocking | 3 | 2.679 | 0.438 | 0.733 | 0.719 | 0.732 | 9.068 |
| 5x8 | 40 | nonblocking_iprobe | 3 | 1.620 | 2.092 | 2.750 | 2.743 | 1.201 | 64.270 |
| 5x8 | 40 | nonblocking_irecv | 3 | 1.567 | 0.930 | 1.558 | 1.551 | 1.025 | 54.199 |
| 6x7 | 42 | blocking | 3 | 3.096 | 0.668 | 0.405 | 0.394 | 0.902 | 5.176 |
| 6x7 | 42 | nonblocking_iprobe | 3 | 1.560 | 2.017 | 2.858 | 2.850 | 1.322 | 55.232 |
| 6x7 | 42 | nonblocking_irecv | 3 | 1.882 | 1.445 | 1.771 | 1.760 | 2.141 | 34.667 |
| 5x9 | 45 | blocking | 3 | 2.842 | 0.423 | 0.391 | 0.383 | 1.066 | 5.855 |
| 5x9 | 45 | nonblocking_iprobe | 3 | 1.566 | 2.151 | 3.227 | 3.220 | 1.314 | 72.187 |
| 5x9 | 45 | nonblocking_irecv | 3 | 1.641 | 1.049 | 1.624 | 1.615 | 1.330 | 77.137 |
| 6x8 | 48 | blocking | 3 | 2.061 | 0.258 | 0.195 | 0.185 | 1.087 | 6.725 |
| 6x8 | 48 | nonblocking_iprobe | 3 | 1.570 | 2.049 | 2.765 | 2.757 | 1.486 | 71.772 |
| 6x8 | 48 | nonblocking_irecv | 3 | 1.623 | 0.916 | 1.641 | 1.632 | 1.055 | 57.827 |
| 7x7 | 49 | blocking | 3 | 2.656 | 0.370 | 0.749 | 0.739 | 1.180 | 4.694 |
| 7x7 | 49 | nonblocking_iprobe | 3 | 1.651 | 2.168 | 2.511 | 2.505 | 1.356 | 82.145 |
| 7x7 | 49 | nonblocking_irecv | 3 | 1.746 | 1.057 | 2.404 | 2.395 | 1.237 | 64.365 |
| 6x9 | 54 | blocking | 3 | 3.974 | 0.767 | 0.511 | 0.501 | 2.927 | 8.044 |
| 6x9 | 54 | nonblocking_iprobe | 3 | 1.936 | 2.097 | 2.735 | 2.728 | 1.945 | 73.856 |
| 6x9 | 54 | nonblocking_irecv | 3 | 1.661 | 1.015 | 1.681 | 1.673 | 1.738 | 73.661 |
| 7x8 | 56 | blocking | 3 | 3.031 | 0.624 | 0.587 | 0.579 | 1.808 | 5.818 |
| 7x8 | 56 | nonblocking_iprobe | 3 | 1.570 | 2.134 | 3.188 | 3.180 | 1.578 | 95.345 |
| 7x8 | 56 | nonblocking_irecv | 3 | 1.612 | 0.998 | 1.668 | 1.661 | 1.275 | 80.975 |
| 6x10 | 60 | blocking | 3 | 2.988 | 0.517 | 0.264 | 0.253 | 1.510 | 6.888 |
| 6x10 | 60 | nonblocking_iprobe | 3 | 1.694 | 2.117 | 2.815 | 2.806 | 1.861 | 62.703 |
| 6x10 | 60 | nonblocking_irecv | 3 | 1.607 | 0.961 | 2.012 | 2.006 | 1.255 | 73.202 |
| 8x8 | 64 | blocking | 3 | 6.517 | 1.047 | 1.029 | 1.020 | 3.168 | 5.092 |
| 8x8 | 64 | nonblocking_iprobe | 3 | 1.569 | 2.110 | 2.737 | 2.731 | 1.577 | 87.214 |
| 8x8 | 64 | nonblocking_irecv | 3 | 2.551 | 1.490 | 3.825 | 3.731 | 2.022 | 28.440 |
| 7x10 | 70 | blocking | 3 | 3.538 | 0.929 | 0.504 | 0.494 | 1.495 | 10.324 |
| 7x10 | 70 | nonblocking_iprobe | 3 | 1.907 | 2.279 | 2.994 | 2.987 | 1.842 | 16.995 |
| 7x10 | 70 | nonblocking_irecv | 3 | 1.758 | 1.047 | 1.839 | 1.832 | 1.263 | 81.968 |
| 8x9 | 72 | blocking | 3 | 3.819 | 0.754 | 1.070 | 1.061 | 2.026 | 7.200 |
| 8x9 | 72 | nonblocking_iprobe | 3 | 1.917 | 2.200 | 2.820 | 2.813 | 1.538 | 72.113 |
| 8x9 | 72 | nonblocking_irecv | 3 | 2.046 | 1.282 | 2.177 | 2.168 | 1.784 | 55.607 |
| 8x10 | 80 | blocking | 3 | 4.596 | 1.159 | 0.668 | 0.659 | 2.054 | 8.679 |
| 8x10 | 80 | nonblocking_iprobe | 3 | 1.893 | 2.136 | 3.024 | 3.016 | 2.266 | 28.277 |
| 8x10 | 80 | nonblocking_irecv | 3 | 1.970 | 1.240 | 2.162 | 2.155 | 1.445 | 46.171 |
| 9x9 | 81 | blocking | 3 | 3.937 | 0.817 | 0.886 | 0.876 | 2.020 | 7.663 |
| 9x9 | 81 | nonblocking_iprobe | 3 | 1.989 | 2.487 | 3.450 | 3.441 | 2.307 | 65.356 |
| 9x9 | 81 | nonblocking_irecv | 3 | 1.836 | 1.211 | 1.823 | 1.816 | 1.623 | 16.340 |

## All line results (mean over repeats, ms)

| Size | Nodes | Version | Runs | avg_exchange_ms | avg_status_delay_ms | avg_report_rtt_ms | avg_report_comm_ms | avg_stopcheck_ms | avg_shutdown_ms |
|---|---|---|---|---|---|---|---|---|---|
| 1x2 | 2 | blocking | 3 | 0.612 | 0.005 | 0.034 | 0.028 | 0.019 | 0.947 |
| 1x2 | 2 | nonblocking_iprobe | 3 | 1.026 | 2.159 | 2.546 | 2.523 | 0.075 | 66.678 |
| 1x2 | 2 | nonblocking_irecv | 3 | 0.733 | 0.782 | 1.390 | 1.378 | 0.022 | 69.461 |
| 1x4 | 4 | blocking | 3 | 1.034 | 0.007 | 0.336 | 0.328 | 0.293 | 3.021 |
| 1x4 | 4 | nonblocking_iprobe | 3 | 1.094 | 2.014 | 2.565 | 2.547 | 0.488 | 65.271 |
| 1x4 | 4 | nonblocking_irecv | 3 | 1.105 | 0.835 | 1.380 | 1.370 | 0.518 | 98.071 |
| 1x6 | 6 | blocking | 3 | 1.201 | 0.008 | 0.029 | 0.023 | 0.342 | 3.706 |
| 1x6 | 6 | nonblocking_iprobe | 3 | 1.162 | 2.213 | 2.955 | 2.934 | 0.952 | 30.586 |
| 1x6 | 6 | nonblocking_irecv | 3 | 1.186 | 0.814 | 1.752 | 1.738 | 0.944 | 14.450 |
| 1x8 | 8 | blocking | 3 | 1.607 | 0.012 | 0.035 | 0.029 | 1.023 | 4.096 |
| 1x8 | 8 | nonblocking_iprobe | 3 | 1.403 | 2.365 | 3.539 | 3.521 | 1.315 | 56.337 |
| 1x8 | 8 | nonblocking_irecv | 3 | 1.918 | 1.071 | 1.867 | 1.855 | 1.738 | 69.373 |
| 1x10 | 10 | blocking | 3 | 1.336 | 0.267 | 1.174 | 0.711 | 1.210 | 10.999 |
| 1x10 | 10 | nonblocking_iprobe | 3 | 1.140 | 2.096 | 3.304 | 3.294 | 1.434 | 47.177 |
| 1x10 | 10 | nonblocking_irecv | 3 | 1.173 | 0.795 | 1.489 | 1.480 | 0.814 | 6.835 |
| 1x12 | 12 | blocking | 3 | 1.367 | 0.253 | 0.081 | 0.071 | 0.410 | 3.533 |
| 1x12 | 12 | nonblocking_iprobe | 3 | 1.260 | 2.260 | 3.629 | 3.621 | 1.318 | 36.202 |
| 1x12 | 12 | nonblocking_irecv | 3 | 1.921 | 2.031 | 1.405 | 1.396 | 2.049 | 13.037 |
| 1x14 | 14 | blocking | 3 | 1.208 | 0.144 | 0.068 | 0.056 | 0.417 | 3.235 |
| 1x14 | 14 | nonblocking_iprobe | 3 | 1.092 | 2.003 | 2.705 | 2.696 | 1.197 | 25.798 |
| 1x14 | 14 | nonblocking_irecv | 3 | 1.152 | 0.974 | 1.573 | 1.565 | 1.239 | 30.987 |
| 1x16 | 16 | blocking | 3 | 1.374 | 0.235 | 0.218 | 0.206 | 0.539 | 3.132 |
| 1x16 | 16 | nonblocking_iprobe | 3 | 1.245 | 2.069 | 2.658 | 2.651 | 1.389 | 36.755 |
| 1x16 | 16 | nonblocking_irecv | 3 | 1.134 | 0.861 | 1.402 | 1.394 | 1.212 | 35.351 |
| 1x18 | 18 | blocking | 3 | 1.224 | 0.096 | 0.243 | 0.232 | 0.480 | 4.491 |
| 1x18 | 18 | nonblocking_iprobe | 3 | 1.299 | 2.080 | 2.648 | 2.641 | 1.273 | 38.943 |
| 1x18 | 18 | nonblocking_irecv | 3 | 1.162 | 0.816 | 1.402 | 1.394 | 0.954 | 15.848 |
| 1x20 | 20 | blocking | 3 | 1.334 | 0.185 | 0.079 | 0.069 | 0.452 | 4.290 |
| 1x20 | 20 | nonblocking_iprobe | 3 | 1.184 | 2.027 | 2.641 | 2.635 | 1.447 | 38.770 |
| 1x20 | 20 | nonblocking_irecv | 3 | 1.170 | 0.815 | 1.383 | 1.375 | 0.968 | 21.190 |
| 1x22 | 22 | blocking | 3 | 1.544 | 0.237 | 0.181 | 0.172 | 0.796 | 4.052 |
| 1x22 | 22 | nonblocking_iprobe | 3 | 1.185 | 2.068 | 2.813 | 2.804 | 1.289 | 33.919 |
| 1x22 | 22 | nonblocking_irecv | 3 | 1.151 | 0.830 | 1.424 | 1.417 | 0.990 | 20.079 |
| 1x24 | 24 | blocking | 3 | 1.577 | 0.172 | 0.522 | 0.513 | 0.630 | 4.272 |
| 1x24 | 24 | nonblocking_iprobe | 3 | 1.136 | 2.024 | 2.649 | 2.641 | 1.540 | 41.260 |
| 1x24 | 24 | nonblocking_irecv | 3 | 1.184 | 0.807 | 1.422 | 1.416 | 1.306 | 45.863 |
| 1x26 | 26 | blocking | 3 | 1.686 | 0.258 | 0.142 | 0.135 | 0.724 | 5.441 |
| 1x26 | 26 | nonblocking_iprobe | 3 | 1.198 | 2.073 | 2.704 | 2.696 | 1.595 | 52.992 |
| 1x26 | 26 | nonblocking_irecv | 3 | 1.306 | 0.816 | 1.476 | 1.470 | 1.400 | 53.324 |
| 1x28 | 28 | blocking | 3 | 1.798 | 0.499 | 0.431 | 0.422 | 1.123 | 3.657 |
| 1x28 | 28 | nonblocking_iprobe | 3 | 1.322 | 2.130 | 2.761 | 2.754 | 1.956 | 46.942 |
| 1x28 | 28 | nonblocking_irecv | 3 | 1.282 | 0.902 | 1.435 | 1.428 | 1.276 | 43.696 |
| 1x30 | 30 | blocking | 3 | 1.616 | 0.300 | 0.469 | 0.459 | 0.831 | 3.603 |
| 1x30 | 30 | nonblocking_iprobe | 3 | 1.189 | 2.016 | 2.785 | 2.769 | 1.728 | 57.350 |
| 1x30 | 30 | nonblocking_irecv | 3 | 1.327 | 0.885 | 1.727 | 1.720 | 1.281 | 47.952 |
| 1x32 | 32 | blocking | 3 | 1.559 | 0.274 | 0.271 | 0.262 | 0.830 | 6.812 |
| 1x32 | 32 | nonblocking_iprobe | 3 | 1.428 | 2.241 | 2.906 | 2.898 | 3.415 | 75.294 |
| 1x32 | 32 | nonblocking_irecv | 3 | 1.202 | 0.852 | 1.564 | 1.557 | 1.459 | 47.791 |
| 1x34 | 34 | blocking | 3 | 1.729 | 0.310 | 0.248 | 0.240 | 0.803 | 4.077 |
| 1x34 | 34 | nonblocking_iprobe | 3 | 1.260 | 2.169 | 2.738 | 2.731 | 2.295 | 28.281 |
| 1x34 | 34 | nonblocking_irecv | 3 | 1.145 | 0.841 | 1.477 | 1.471 | 1.425 | 40.005 |
| 1x36 | 36 | blocking | 3 | 1.747 | 0.344 | 0.275 | 0.266 | 1.067 | 2.692 |
| 1x36 | 36 | nonblocking_iprobe | 3 | 1.324 | 2.083 | 2.871 | 2.864 | 1.881 | 70.932 |
| 1x36 | 36 | nonblocking_irecv | 3 | 1.249 | 1.055 | 1.523 | 1.517 | 1.412 | 56.930 |
| 1x38 | 38 | blocking | 3 | 2.330 | 0.597 | 1.114 | 1.108 | 2.205 | 4.639 |
| 1x38 | 38 | nonblocking_iprobe | 3 | 1.190 | 2.015 | 2.679 | 2.671 | 1.762 | 67.425 |
| 1x38 | 38 | nonblocking_irecv | 3 | 1.189 | 0.875 | 1.534 | 1.528 | 1.474 | 45.681 |
| 1x40 | 40 | blocking | 3 | 1.851 | 0.451 | 0.623 | 0.615 | 1.105 | 5.120 |
| 1x40 | 40 | nonblocking_iprobe | 3 | 1.248 | 2.125 | 2.809 | 2.802 | 1.978 | 39.717 |
| 1x40 | 40 | nonblocking_irecv | 3 | 1.152 | 0.848 | 1.511 | 1.506 | 1.436 | 46.505 |
| 1x42 | 42 | blocking | 3 | 1.798 | 0.376 | 0.462 | 0.455 | 1.654 | 4.614 |
| 1x42 | 42 | nonblocking_iprobe | 3 | 1.266 | 2.017 | 2.835 | 2.828 | 1.945 | 88.134 |
| 1x42 | 42 | nonblocking_irecv | 3 | 1.238 | 1.058 | 1.613 | 1.607 | 1.447 | 60.813 |
| 1x44 | 44 | blocking | 3 | 1.609 | 0.212 | 0.271 | 0.263 | 0.924 | 4.717 |
| 1x44 | 44 | nonblocking_iprobe | 3 | 1.283 | 2.249 | 2.762 | 2.755 | 2.152 | 89.657 |
| 1x44 | 44 | nonblocking_irecv | 3 | 1.359 | 1.026 | 2.159 | 2.152 | 1.758 | 72.934 |
| 1x46 | 46 | blocking | 3 | 1.764 | 0.322 | 0.557 | 0.549 | 1.250 | 4.912 |
| 1x46 | 46 | nonblocking_iprobe | 3 | 1.167 | 2.115 | 2.969 | 2.957 | 2.064 | 84.259 |
| 1x46 | 46 | nonblocking_irecv | 3 | 1.260 | 0.901 | 1.587 | 1.581 | 1.646 | 56.071 |
| 1x48 | 48 | blocking | 3 | 2.136 | 0.580 | 0.366 | 0.359 | 1.182 | 5.930 |
| 1x48 | 48 | nonblocking_iprobe | 3 | 1.351 | 2.116 | 2.784 | 2.777 | 1.917 | 81.045 |
| 1x48 | 48 | nonblocking_irecv | 3 | 1.225 | 0.938 | 1.598 | 1.592 | 1.571 | 64.175 |
| 1x50 | 50 | blocking | 3 | 1.706 | 0.321 | 0.694 | 0.686 | 1.267 | 5.588 |
| 1x50 | 50 | nonblocking_iprobe | 3 | 1.423 | 2.216 | 3.046 | 3.039 | 3.010 | 66.855 |
| 1x50 | 50 | nonblocking_irecv | 3 | 1.394 | 1.002 | 1.816 | 1.810 | 1.667 | 60.732 |
| 1x52 | 52 | blocking | 3 | 2.322 | 0.564 | 0.716 | 0.709 | 1.779 | 5.310 |
| 1x52 | 52 | nonblocking_iprobe | 3 | 1.322 | 2.095 | 2.732 | 2.726 | 2.238 | 71.496 |
| 1x52 | 52 | nonblocking_irecv | 3 | 1.450 | 1.113 | 2.558 | 2.551 | 1.967 | 38.814 |
| 1x54 | 54 | blocking | 3 | 2.885 | 0.966 | 1.549 | 1.542 | 2.968 | 5.664 |
| 1x54 | 54 | nonblocking_iprobe | 3 | 1.538 | 2.222 | 3.385 | 3.379 | 3.037 | 70.366 |
| 1x54 | 54 | nonblocking_irecv | 3 | 1.597 | 1.137 | 1.729 | 1.721 | 1.926 | 57.301 |
| 1x56 | 56 | blocking | 3 | 2.266 | 0.548 | 0.504 | 0.497 | 2.209 | 5.375 |
| 1x56 | 56 | nonblocking_iprobe | 3 | 1.240 | 2.036 | 2.809 | 2.804 | 2.218 | 98.985 |
| 1x56 | 56 | nonblocking_irecv | 3 | 1.907 | 1.351 | 2.518 | 2.512 | 3.210 | 50.447 |
| 1x58 | 58 | blocking | 3 | 2.373 | 0.619 | 0.875 | 0.867 | 1.891 | 4.864 |
| 1x58 | 58 | nonblocking_iprobe | 3 | 1.371 | 2.226 | 2.879 | 2.873 | 2.759 | 55.275 |
| 1x58 | 58 | nonblocking_irecv | 3 | 1.327 | 0.988 | 1.961 | 1.955 | 1.998 | 50.650 |
| 1x60 | 60 | blocking | 3 | 3.102 | 1.109 | 0.544 | 0.537 | 2.744 | 4.419 |
| 1x60 | 60 | nonblocking_iprobe | 3 | 1.391 | 2.138 | 2.853 | 2.847 | 2.440 | 40.523 |
| 1x60 | 60 | nonblocking_irecv | 3 | 1.421 | 1.050 | 1.817 | 1.812 | 2.118 | 62.145 |
| 1x62 | 62 | blocking | 3 | 2.605 | 0.705 | 1.207 | 1.198 | 3.249 | 6.797 |
| 1x62 | 62 | nonblocking_iprobe | 3 | 1.438 | 2.254 | 3.214 | 3.207 | 2.676 | 39.095 |
| 1x62 | 62 | nonblocking_irecv | 3 | 1.335 | 0.990 | 1.797 | 1.791 | 1.941 | 59.508 |
| 1x64 | 64 | blocking | 3 | 2.920 | 0.728 | 0.623 | 0.616 | 2.410 | 19.946 |
| 1x64 | 64 | nonblocking_iprobe | 3 | 1.411 | 2.088 | 2.974 | 2.968 | 2.472 | 45.951 |
| 1x64 | 64 | nonblocking_irecv | 3 | 1.376 | 0.918 | 1.589 | 1.584 | 1.833 | 97.378 |
