/*
 * ev_network_blocking.c
 *
 * EV Charging Network Simulation, BLOCKING version
 * The non-blocking version of the same simulation is ev_network_nonblocking.c.
 *
 * Process layout: the EV charging nodes (Task 1) are ranks 0 .. size-2, arranged in a 2D
 * Cartesian grid; the Base Station (Task 2) is the last rank, size-1.
 *
 * Task 1, EV charging node (slave_io). Before the first tick, each node measures its clock
 * offset from the Base Station (used for one-way status update delays). Every tick (100 ms)
 * each node:
 *   1. Updates its charging ports with OpenMP threads (one thread per port, shared array).
 *   2. Logs its availability (date, time, free ports) to node_<rank>.log.
 *   3. Sends its availability to the Base Station.
 *   4. Exchanges availability with its 4 neighbours using BLOCKING MPI_Send / MPI_Recv,
 *      paired in a fixed order so the blocking calls cannot deadlock.
 *   5. If congested: redirects EVs to a free neighbour, or, if every neighbour is also
 *      congested, sends an incident report to the Base Station and waits (blocking
 *      MPI_Probe + MPI_Recv) for its suggestions.
 *   6. Agrees with all other nodes whether to stop (MPI_Reduce + MPI_Bcast).
 *
 * Task 2, Base Station (master_io). Waits in a BLOCKING MPI_Recv(MPI_ANY_SOURCE, MPI_ANY_TAG)
 * for any message, answers incident reports with the nearby available nodes (information
 * update mechanism), logs everything to base_station_blocking.log, sends the terminate signal
 * when the simulation time is over, waits for every node's acknowledgement, and appends the
 * timing results to comm_timing.csv.
 *
 * Build: mpicc -fopenmp -O2 ev_network_blocking.c -o ev_network_blocking -lm
 *   (on macOS prefix with OMPI_CC=$OPENMP_CC so mpicc uses a compiler with OpenMP)
 *
 * Usage: mpirun -np (rows*cols + 1) ./ev_network_blocking [rows cols [period_ms [duration_s]]]
 *        rows cols  = grid size; omit them (or pass 0 0) to let MPI_Dims_create choose
 *        period_ms  = Base Station "predefined period" for the information update mechanism
 *        duration_s = simulation length before the Base Station sends the terminate signal
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#include <omp.h>
#include <mpi.h>

// =============================================================================
// Shared definitions: helpers, message tags, constants and message structures
// =============================================================================

// Millisecond sleep that works on Windows (Sleep) as well as Linux/macOS (usleep)
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static inline void sleep_ms(unsigned int ms) { Sleep(ms); }
#else
#include <unistd.h>
static inline void sleep_ms(unsigned int ms) { usleep(ms * 1000u); }
#endif

// rand_r is a thread-safe random number generator (each caller keeps its own seed).
// Linux and macOS provide it; Windows does not, so an equivalent version is defined here.
#ifdef _WIN32
static inline int rand_r(unsigned int *seed) {
    *seed = *seed * 1103515245u + 12345u;
    return (int)((*seed / 65536u) % 32768u);
}
#endif

// Converts grid coordinates (row i, column j) to a node rank in row-major order.
// The reverse is: row = rank / cols, column = rank % cols.
static inline int TwoDToOneD(int i, int j, int cols) { return (i * cols) + j; }

// =========================================================================
// 1. MPI Message Tags (identify the type of every message)
// =========================================================================
#define TAG_NEIGHBOR_STATUS   102  // Node <-> neighbour node: availability exchange each tick
#define TAG_INCIDENT_REPORT   200  // Node -> Base Station: congestion alert
#define TAG_REDIRECT_INFO     201  // Base Station -> node: suggested nearby nodes
#define TAG_STATUS_UPDATE     202  // Node -> Base Station: availability update each tick
#define TAG_TERMINATE         300  // Base Station -> node: stop; node -> Base Station: ack
#define TAG_CLOCK_SYNC        400  // Node <-> Base Station: clock synchronisation before the simulation

// =========================================================================
// 2. Constants
// =========================================================================
#define MAX_NEIGHBORS           4  // Left, Right, Top, Bottom in a 2D grid
#define MAX_SUGGESTED_NODES     8  // Most nodes at distance 2 from an interior grid node
#define DEFAULT_PORT_COUNT      6  // Charging ports per EV node (k in the specification)
#define CONGESTION_THRESHOLD    1  // A node is congested when free ports <= this value
#define CLOCK_SYNC_ROUNDS       5  // Request/reply rounds per node when synchronising clocks

// Default Base Station settings; both can be overridden on the command line
#define DEFAULT_REPORT_PERIOD_MS 500  // Predefined period of the information update mechanism
#define DEFAULT_SIM_DURATION_S   4    // Simulation length before the terminate signal is sent

// Result codes in the Base Station's reply
#define STATUS_NODES_AVAILABLE  1  // The reply contains one or more suggested nodes
#define STATUS_NO_NODES_AVAIL   0  // No nodes are available nearby

// Purpose of a NeighborMsg
#define MSG_TYPE_AVAIL_QUERY    1  // Asking a neighbour for its availability
#define MSG_TYPE_AVAIL_REPLY    2  // Carrying this node's availability

// =========================================================================
// 3. Message Structures
// =========================================================================

/**
 * Calendar date and time, used to stamp when a congestion incident was discovered.
 */
typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
} Timestamp;

/**
 * Availability of one neighbouring node, as included in an incident report.
 */
typedef struct {
    int rank;              // Rank of the neighbouring node
    int coords[2];         // (row, column) of the neighbour in the grid
    int free_ports;        // Number of free charging ports at the neighbour
} NeighborInfo;

/**
 * Message exchanged between adjacent EV nodes every tick.
 */
typedef struct {
    int sender_rank;       // Rank of the sending node
    int sender_coords[2];  // (row, column) of the sending node
    int free_ports;        // Free charging ports at the sending node
    int msg_type;          // MSG_TYPE_AVAIL_QUERY or MSG_TYPE_AVAIL_REPLY
} NeighborMsg;

/**
 * Congestion report sent by an EV node to the Base Station when the node and all of its
 * neighbours are congested. Contains the discovery time, the node's identity and availability,
 * every neighbour's availability, and the size of the neighbour communication.
 */
typedef struct {
    Timestamp time_discovered;              // When the congestion was detected
    int reporting_node_rank;                // Rank of the reporting node
    int reporting_node_coords[2];           // (row, column) of the reporting node
    int reporting_node_free_ports;          // Free ports at the reporting node

    int num_neighbors;                      // How many entries of neighbors[] are used
    NeighborInfo neighbors[MAX_NEIGHBORS];  // Availability of each neighbour

    int neighbor_msg_count;                 // Messages exchanged with neighbours this tick
    int total_msg_size_bytes;               // Total size of those messages (bytes)
    double neighbor_exchange_time;          // Time spent on the neighbour exchange (s)
} IncidentReportMsg;

/**
 * Availability update sent by every EV node to the Base Station once per tick. It keeps the
 * Base Station's view of the grid up to date, and guarantees that the blocking Base Station
 * receives a message at least once per tick, so it can regularly check the simulation time.
 */
typedef struct {
    int node_rank;                          // Rank of the sending node
    int free_ports;                         // Free ports at the node this tick
    int tick;                               // Node's tick counter
    double send_time;                       // Send time converted to the Base Station's clock (s),
                                            // used to measure the one-way delay
} NodeStatusMsg;

/**
 * Base Station's reply to an incident report.
 */
typedef struct {
    int target_node_rank;                   // Node that sent the report
    int status_code;                        // STATUS_NODES_AVAILABLE or STATUS_NO_NODES_AVAIL
    int num_suggested_nodes;                // How many entries of the arrays below are used
    int suggested_ranks[MAX_SUGGESTED_NODES];    // Suggested nodes
    int suggested_coords[MAX_SUGGESTED_NODES][2];// (row, column) of each suggested node
    int nbrs_reported_in_period;            // Reporter's neighbours that also reported within the period
    double bs_processing_time;              // Base Station time between receiving the report and
                                            // replying (s); the node subtracts it from its round
                                            // trip to get the pure communication time
} BaseStationResponseMsg;

/**
 * Per-node statistics, sent to the Base Station as the acknowledgement of TAG_TERMINATE.
 * All times are measured with MPI_Wtime and are used for the communication analysis (Task 3).
 */
typedef struct {
    int node_rank;                          // Rank of the node
    int coords[2];                          // (row, column) of the node
    int ticks;                              // Simulation ticks completed
    int exchanges;                          // Neighbour exchanges performed (one per tick)
    int congestion_events;                  // Ticks where free ports <= threshold
    int local_redirects;                    // Congestion solved by a free neighbour
    int reports_sent;                       // Incident reports sent to the Base Station
    int neighbor_msgs;                      // Neighbour messages sent + received
    int neighbor_bytes;                     // Total size of those messages (bytes)
    double total_exchange_time;             // Sum of neighbour exchange times (s)
    double max_exchange_time;               // Slowest neighbour exchange (s)
    double total_report_rtt;                // Sum of report -> reply round trips (s)
    double max_report_rtt;                  // Slowest round trip (s)
    double total_report_comm;               // Sum of round trips minus Base Station processing (s)
    double max_report_comm;                 // Largest of those (s)
    double total_stop_check_time;           // Sum of per-tick stop checks (MPI_Reduce + MPI_Bcast) (s)
    double max_stop_check_time;             // Slowest stop check (s)
    double clock_sync_error;                // Largest possible error of this node's clock offset (s)
} NodeSummaryMsg;

// =============================================================================
// Run label: names the Base Station log file and this run's rows in comm_timing.csv
// =============================================================================

static const char *RUN_LABEL = "blocking";
static const char *RUN_DESC = "blocking MPI_Send / MPI_Recv neighbour exchange + blocking MPI_Recv Base Station";

// =============================================================================
// Task 1: EV charging node (slave_io), runs on ranks 0 .. size-2
// =============================================================================

#define SHIFT_ROW 0  // Dimension 0: shifting along it gives the Top (lo) and Bottom (hi) neighbours
#define SHIFT_COL 1  // Dimension 1: shifting along it gives the Left (lo) and Right (hi) neighbours
#define DISP 1       // Neighbours are 1 grid cell away

#define TICK_INTERVAL_MS 100 // Length of one simulation tick (port update cycle)

// Names for the neighbour slots, in the same order as action_list[] below
static const char *NBR_NAMES[MAX_NEIGHBORS] = {"Left", "Right", "Top", "Bottom"};

/**
 * Fills ts with the current local date and time.
 */
static void get_current_timestamp(Timestamp *ts) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    ts->year   = t->tm_year + 1900;  // tm_year counts from 1900
    ts->month  = t->tm_mon + 1;      // tm_mon counts from 0
    ts->day    = t->tm_mday;
    ts->hour   = t->tm_hour;
    ts->minute = t->tm_min;
    ts->second = t->tm_sec;
}

/**
 * Simulates EVs arriving at and leaving the charging ports, and returns the number of free ports.
 *
 * ports[] is shared by all threads; each OpenMP thread updates exactly one port (element i),
 * so no two threads write the same element. Each port has its own random seed in port_seeds[],
 * so rand_r can be used safely by several threads at once. The free-port count is combined
 * with a reduction, which gives every thread a private counter and adds them up at the end.
 *
 * Per tick, each port: 30% chance it becomes free, 50% chance it becomes in use,
 * 20% chance it keeps its previous state.
 */
static int simulate_and_count_ports(int ports[], unsigned int port_seeds[], int num_ports) {
    int free_count = 0;

    #pragma omp parallel for shared(ports, port_seeds) reduction(+:free_count) schedule(static) num_threads(num_ports)
    for (int i = 0; i < num_ports; i++) {
        // Random number 0-99 from this port's own seed
        int rnd = rand_r(&port_seeds[i]) % 100;
        if (rnd < 30) {
            ports[i] = 1;  // EV departed -> port is free
        } else if (rnd < 80) {
            ports[i] = 0;  // EV arrived -> port is in use
        }

        // Count this port if it is free
        if (ports[i] == 1) {
            free_count++;
        }
    }

    return free_count;
}

/**
 * Checks, without waiting, whether the Base Station has sent the terminate signal.
 * If it has, the message is received (removed from the queue) and true is returned.
 */
static bool termination_received(MPI_Comm world_comm, int master_rank) {
    int flag = 0;
    MPI_Iprobe(master_rank, TAG_TERMINATE, world_comm, &flag, MPI_STATUS_IGNORE);
    if (flag) {
        int dummy;
        MPI_Recv(&dummy, 1, MPI_INT, master_rank, TAG_TERMINATE, world_comm, MPI_STATUS_IGNORE);
    }
    return flag;
}

/**
 * Exchanges availability with ONE neighbour using blocking MPI_Send / MPI_Recv.
 *
 * Both partners call this in the same phase, one with send_first = true and the other with
 * send_first = false. So whenever one side is in MPI_Send, the other side is in the matching
 * MPI_Recv, and the two calls can never wait on each other.
 * If partner is MPI_PROC_NULL (no neighbour at the grid edge), both calls return immediately
 * and *in is left unchanged.
 */
static void blocking_exchange(int partner, bool send_first, NeighborMsg *out, NeighborMsg *in, MPI_Comm comm) {
    MPI_Status status;
    if (send_first) {
        MPI_Send(out, sizeof(NeighborMsg), MPI_BYTE, partner, TAG_NEIGHBOR_STATUS, comm);
        MPI_Recv(in, sizeof(NeighborMsg), MPI_BYTE, partner, TAG_NEIGHBOR_STATUS, comm, &status);
    } else {
        MPI_Recv(in, sizeof(NeighborMsg), MPI_BYTE, partner, TAG_NEIGHBOR_STATUS, comm, &status);
        MPI_Send(out, sizeof(NeighborMsg), MPI_BYTE, partner, TAG_NEIGHBOR_STATUS, comm);
    }
}

/**
 * Estimates the difference between this node's MPI_Wtime clock and the Base Station's.
 *
 * MPI_Wtime is not synchronised across processes (each process starts its own clock), so a
 * send time taken on a node cannot be compared directly with a receive time taken at the
 * Base Station. The node sends a request, the Base Station replies with its current
 * MPI_Wtime, and the node assumes that reading was taken halfway through the round trip:
 *   offset = bs_time - (t_send + t_recv) / 2
 * The round with the shortest round trip is kept, because its error is the smallest: at
 * most half of that round trip, returned in *error_out.
 * Local MPI_Wtime() + offset gives the same moment on the Base Station's clock.
 */
static double sync_clock_with_base_station(MPI_Comm world_comm, int master_rank, double *error_out) {
    double best_offset = 0.0, best_rtt = -1.0;
    for (int round = 0; round < CLOCK_SYNC_ROUNDS; round++) {
        int request = round;
        double bs_time;
        double t_send = MPI_Wtime();
        MPI_Send(&request, 1, MPI_INT, master_rank, TAG_CLOCK_SYNC, world_comm);
        MPI_Recv(&bs_time, 1, MPI_DOUBLE, master_rank, TAG_CLOCK_SYNC, world_comm, MPI_STATUS_IGNORE);
        double t_recv = MPI_Wtime();
        double rtt = t_recv - t_send;
        if (best_rtt < 0.0 || rtt < best_rtt) {  // Keep the most accurate round
            best_rtt = rtt;
            best_offset = bs_time - (t_send + t_recv) / 2.0;
        }
    }
    *error_out = best_rtt / 2.0;
    return best_offset;
}

/**
 * EV charging node. Runs on every process except the last one.
 *
 * world_comm: MPI_COMM_WORLD, used to talk to the Base Station
 * comm:       communicator containing only the EV nodes, used to build the grid
 * nrows/ncols: grid size (0 0 lets MPI_Dims_create choose)
 */
int slave_io(MPI_Comm world_comm, MPI_Comm comm, int nrows, int ncols) {
    int ndims = 2, global_size, global_rank, local_size, local_rank, reorder, my_cart_rank, ierr;
    int nbr_i_lo, nbr_i_hi;  // Top and Bottom neighbour ranks
    int nbr_j_lo, nbr_j_hi;  // Left and Right neighbour ranks
    MPI_Comm comm2D;         // Communicator with the 2D Cartesian grid attached
    int dims[2], coord[2];   // Grid size, and this node's (row, column)
    int wrap_around[2];      // Whether each dimension wraps around (it does not)
    char buf[256];           // Log file name
    FILE *pFile;             // This node's availability log

    // Rank and size in MPI_COMM_WORLD (all processes) and in comm (EV nodes only)
    MPI_Comm_size(world_comm, &global_size);
    MPI_Comm_rank(world_comm, &global_rank);
    MPI_Comm_size(comm, &local_size);
    MPI_Comm_rank(comm, &local_rank);
    int master_rank = global_size - 1;  // The Base Station is the last rank

    // -------------------------------------------------------------------------
    // 1. Create the 2D Cartesian grid (MPI virtual topology)
    // -------------------------------------------------------------------------

    // Use the requested grid size, or let MPI_Dims_create pick a balanced one
    if (nrows == 0 && ncols == 0) {
        dims[0] = dims[1] = 0;
    } else {
        dims[0] = nrows;
        dims[1] = ncols;
    }
    MPI_Dims_create(local_size, ndims, dims);

    // Non-periodic grid: nodes on the edge have no neighbour on that side.
    // reorder = 0 keeps each node's rank unchanged, so Cartesian rank == world rank == Node N.
    wrap_around[0] = wrap_around[1] = 0;
    reorder = 0;
    ierr = MPI_Cart_create(comm, ndims, dims, wrap_around, reorder, &comm2D);
    if (ierr != MPI_SUCCESS)
        printf("ERROR[%d] creating CART\n", ierr);

    // This node's (row, column), its rank in the grid, and its four neighbours.
    // A missing neighbour (grid edge) is returned as MPI_PROC_NULL.
    MPI_Cart_coords(comm2D, local_rank, ndims, coord);
    MPI_Cart_rank(comm2D, coord, &my_cart_rank);
    MPI_Cart_shift(comm2D, SHIFT_ROW, DISP, &nbr_i_lo, &nbr_i_hi);
    MPI_Cart_shift(comm2D, SHIFT_COL, DISP, &nbr_j_lo, &nbr_j_hi);

    // The four neighbours in a fixed order: Left, Right, Top, Bottom
    int action_list[MAX_NEIGHBORS] = {nbr_j_lo, nbr_j_hi, nbr_i_lo, nbr_i_hi};

    // Count the neighbours that actually exist (not MPI_PROC_NULL)
    int num_valid_nbrs = 0;
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (action_list[i] != MPI_PROC_NULL) {
            num_valid_nbrs++;
        }
    }

    // Print this node's position and neighbours (MPI_PROC_NULL is printed as a negative number)
    printf("Global rank: %d. Cart rank: %d. Coord: (%d, %d). Left: %d. Right: %d. Top: %d. Bottom: %d\n",
           global_rank, my_cart_rank, coord[0], coord[1], nbr_j_lo, nbr_j_hi, nbr_i_lo, nbr_i_hi);
    fflush(stdout);

    // Open this node's availability log: one row per tick, in the format of specification Table 1
    snprintf(buf, sizeof(buf), "node_%d.log", global_rank);
    pFile = fopen(buf, "w");
    if (pFile != NULL) {
        fprintf(pFile, "Global rank: %d. Cart rank: %d. Coord: (%d, %d). Left: %d. Right: %d. Top: %d. Bottom: %d\n\n",
                global_rank, my_cart_rank, coord[0], coord[1], nbr_j_lo, nbr_j_hi, nbr_i_lo, nbr_i_hi);
        fprintf(pFile, "Year\tMonth\tDay\tHour\tMinute\tSecond\tAvailability\n");
    }

    // -------------------------------------------------------------------------
    // 2. Initialise the shared port array and the random seeds
    // -------------------------------------------------------------------------
    int ports[DEFAULT_PORT_COUNT];                 // 1 = free, 0 = in use; shared by the port threads
    unsigned int port_seeds[DEFAULT_PORT_COUNT];   // One random seed per port
    unsigned int seed = (unsigned int)(time(NULL) ^ (global_rank * 1000));  // Different for every node and run

    // All ports start free. Each port's seed is mixed with a large multiplier so that seeds of
    // neighbouring nodes and ports end up far apart; otherwise rand_r, a linear generator,
    // would produce similar sequences on neighbouring nodes.
    for (int i = 0; i < DEFAULT_PORT_COUNT; i++) {
        ports[i] = 1;
        port_seeds[i] = (seed + (unsigned int)i * 7919u + (unsigned int)global_rank * 104729u) * 2654435761u;
    }

    // Statistics sent to the Base Station at the end of the simulation
    NodeSummaryMsg summary = {0};
    summary.node_rank = global_rank;
    summary.coords[0] = coord[0];
    summary.coords[1] = coord[1];

    // Synchronise with the Base Station's clock, so that status updates can carry a send time
    // the Base Station can compare with its own receive time (one-way delay)
    double clock_error;
    double clock_offset = sync_clock_with_base_station(world_comm, master_rank, &clock_error);
    summary.clock_sync_error = clock_error;

    // All nodes start the simulation together, after the Base Station has answered every
    // node's synchronisation requests
    MPI_Barrier(comm2D);

    // -------------------------------------------------------------------------
    // 3. Main simulation loop: one iteration per tick
    // -------------------------------------------------------------------------
    int simulation_tick = 0;
    bool terminate_received = false;  // Set once this node has received the terminate signal

    while (true) {
        simulation_tick++;

        // Step 3a: update the charging ports with OpenMP threads
        int free_ports = simulate_and_count_ports(ports, port_seeds, DEFAULT_PORT_COUNT);

        // Log this tick's availability with the current date and time
        Timestamp now;
        get_current_timestamp(&now);
        if (pFile != NULL) {
            fprintf(pFile, "%04d\t%02d\t%02d\t%02d\t%02d\t%02d\t%d\n",
                    now.year, now.month, now.day, now.hour, now.minute, now.second, free_ports);
        }

        // Send this tick's availability to the Base Station. The message is only 24 bytes, so
        // MPI delivers it immediately and MPI_Send returns without waiting for the receiver.
        NodeStatusMsg status_update;
        status_update.node_rank = global_rank;
        status_update.free_ports = free_ports;
        status_update.tick = simulation_tick;
        status_update.send_time = MPI_Wtime() + clock_offset;  // Now, on the Base Station's clock
        MPI_Send(&status_update, sizeof(NodeStatusMsg), MPI_BYTE, master_rank, TAG_STATUS_UPDATE, world_comm);

        // Step 3b: exchange availability with all 4 neighbours using BLOCKING calls.
        // If every node simply sent to all neighbours and then received, nodes could end up
        // waiting on each other in a cycle (deadlock). To prevent this, nodes are paired up in
        // a fixed order, one dimension at a time (columns first, then rows), in two phases:
        // phase 0: nodes in an EVEN column pair with their Right neighbour and send first;
        // nodes in an ODD column pair with their Left neighbour and receive first.
        // phase 1: the opposite (odd columns pair Right, even columns pair Left), which
        // covers the remaining pairs.
        // The same two phases are then repeated for rows (Bottom / Top). In every phase each
        // node talks to exactly one partner that is doing the opposite operation.
        NeighborMsg my_status;
        my_status.sender_rank = global_rank;
        my_status.sender_coords[0] = coord[0];
        my_status.sender_coords[1] = coord[1];
        my_status.free_ports = free_ports;
        my_status.msg_type = MSG_TYPE_AVAIL_REPLY;

        NeighborMsg recv_data[MAX_NEIGHBORS];  // One received message per neighbour slot
        for (int i = 0; i < MAX_NEIGHBORS; i++) {
            recv_data[i].free_ports = -1; // Remains -1 if there is no neighbour in this slot
        }

        double exchange_start = MPI_Wtime();
        for (int dim = 0; dim < 2; dim++) {
            int lo = (dim == 0) ? 0 : 2;          // action_list slot: Left (0) for columns, Top (2) for rows
            int hi = lo + 1;                      // action_list slot: Right (1) for columns, Bottom (3) for rows
            int pos = (dim == 0) ? coord[1] : coord[0];  // This node's column (dim 0) or row (dim 1)
            for (int phase = 0; phase < 2; phase++) {
                if (pos % 2 == phase) {
                    // Pair with the higher neighbour (Right / Bottom) and send first
                    blocking_exchange(action_list[hi], true, &my_status, &recv_data[hi], comm2D);
                } else {
                    // Pair with the lower neighbour (Left / Top) and receive first
                    blocking_exchange(action_list[lo], false, &my_status, &recv_data[lo], comm2D);
                }
            }
        }
        double exchange_time = MPI_Wtime() - exchange_start;

        // Record the exchange: one send and one receive per existing neighbour
        summary.exchanges++;
        summary.neighbor_msgs += 2 * num_valid_nbrs;
        summary.neighbor_bytes += 2 * num_valid_nbrs * (int)sizeof(NeighborMsg);
        summary.total_exchange_time += exchange_time;
        if (exchange_time > summary.max_exchange_time) {
            summary.max_exchange_time = exchange_time;
        }

        // Step 3c: is this node congested?
        if (free_ports <= CONGESTION_THRESHOLD) {
            summary.congestion_events++;
            printf("[Node %d (%d,%d)] Congestion warning! Free ports = %d / %d (Threshold = %d).\n",
                   global_rank, coord[0], coord[1], free_ports, DEFAULT_PORT_COUNT, CONGESTION_THRESHOLD);
            fflush(stdout);

            int valid_nbr_count = 0;          // Number of existing neighbours
            int best_nbr = -1;                // Neighbour slot with the most free ports
            bool all_nbrs_congested = true;   // Stays true unless a neighbour has free capacity

            // Start building the incident report with this node's details
            IncidentReportMsg report;
            report.reporting_node_rank = global_rank;
            report.reporting_node_coords[0] = coord[0];
            report.reporting_node_coords[1] = coord[1];
            report.reporting_node_free_ports = free_ports;

            // Copy each existing neighbour's availability into the report, and find the
            // neighbour with the most free ports
            for (int i = 0; i < MAX_NEIGHBORS; i++) {
                if (action_list[i] == MPI_PROC_NULL) {
                    continue;  // No neighbour in this direction
                }

                // The neighbour's grid rank is also its world rank (Node N)
                report.neighbors[valid_nbr_count].rank = action_list[i];
                report.neighbors[valid_nbr_count].coords[0] = recv_data[i].sender_coords[0];
                report.neighbors[valid_nbr_count].coords[1] = recv_data[i].sender_coords[1];
                report.neighbors[valid_nbr_count].free_ports = recv_data[i].free_ports;

                if (recv_data[i].free_ports > CONGESTION_THRESHOLD) {
                    all_nbrs_congested = false;
                    if (best_nbr < 0 || recv_data[i].free_ports > recv_data[best_nbr].free_ports) {
                        best_nbr = i;
                    }
                }
                valid_nbr_count++;
            }

            // Neighbour communication details for the report: one send + one receive per neighbour
            report.num_neighbors = valid_nbr_count;
            report.neighbor_msg_count = 2 * valid_nbr_count;
            report.total_msg_size_bytes = 2 * valid_nbr_count * (int)sizeof(NeighborMsg);
            report.neighbor_exchange_time = exchange_time;
            get_current_timestamp(&report.time_discovered);

            // Step 3d: redirect to a free neighbour, or report to the Base Station
            if (!all_nbrs_congested) {
                // At least one neighbour has capacity: send incoming EVs there
                summary.local_redirects++;
                printf("[Node %d (%d,%d)] Redirecting incoming EVs to %s neighbour Node %d (%d free ports).\n",
                       global_rank, coord[0], coord[1], NBR_NAMES[best_nbr], action_list[best_nbr],
                       recv_data[best_nbr].free_ports);
                fflush(stdout);
            } else {
                // This node and all its neighbours are congested: alert the Base Station
                printf("[Node %d (%d,%d) -> Base Station] All neighbours congested. Transmitting incident report...\n",
                       global_rank, coord[0], coord[1]);
                fflush(stdout);

                // Send the report (blocking), then block until the Base Station answers.
                // The Base Station sends either the reply or, if it has already started shutting
                // down, only the terminate signal. MPI_Probe with MPI_ANY_TAG waits for whichever
                // arrives first and reports its tag, so the node never waits for a reply that
                // will not come. The round trip is timed with MPI_Wtime.
                BaseStationResponseMsg bs_resp;
                MPI_Status probe_status;
                double report_start = MPI_Wtime();
                MPI_Send(&report, sizeof(IncidentReportMsg), MPI_BYTE,
                         master_rank, TAG_INCIDENT_REPORT, world_comm);
                summary.reports_sent++;

                MPI_Probe(master_rank, MPI_ANY_TAG, world_comm, &probe_status);
                if (probe_status.MPI_TAG == TAG_REDIRECT_INFO) {
                    // The reply arrived: receive it
                    MPI_Recv(&bs_resp, sizeof(BaseStationResponseMsg), MPI_BYTE, master_rank,
                             TAG_REDIRECT_INFO, world_comm, MPI_STATUS_IGNORE);
                    // Round trip: report sent -> reply received
                    double rtt = MPI_Wtime() - report_start;
                    summary.total_report_rtt += rtt;
                    if (rtt > summary.max_report_rtt) {
                        summary.max_report_rtt = rtt;
                    }

                    // Communication time only: subtract the Base Station's processing time
                    double comm_time = rtt - bs_resp.bs_processing_time;
                    summary.total_report_comm += comm_time;
                    if (comm_time > summary.max_report_comm) {
                        summary.max_report_comm = comm_time;
                    }

                    if (bs_resp.status_code == STATUS_NODES_AVAILABLE) {
                        // Pass the suggested nodes on to the incoming EVs
                        printf("[Node %d (%d,%d)] Base Station suggests:", global_rank, coord[0], coord[1]);
                        for (int i = 0; i < bs_resp.num_suggested_nodes; i++) {
                            printf(" Node %d (%d,%d)", bs_resp.suggested_ranks[i],
                                   bs_resp.suggested_coords[i][0], bs_resp.suggested_coords[i][1]);
                        }
                        printf("\n");
                    } else {
                        printf("[Node %d (%d,%d)] Base Station reported NO available nodes nearby "
                               "(%d/%d neighbours also reported recently).\n",
                               global_rank, coord[0], coord[1], bs_resp.nbrs_reported_in_period, valid_nbr_count);
                    }
                    fflush(stdout);
                } else {
                    // The terminate signal arrived instead of a reply: receive it
                    int dummy;
                    MPI_Recv(&dummy, 1, MPI_INT, master_rank, TAG_TERMINATE, world_comm, MPI_STATUS_IGNORE);
                    terminate_received = true;
                }
            }
        }

        // -------------------------------------------------------------------------
        // 4. Synchronised shutdown check (every tick)
        // -------------------------------------------------------------------------
        // The terminate signal reaches nodes at slightly different times. If one node stopped
        // early, its neighbours would wait forever in their next blocking MPI_Recv. So every node
        // shares its "terminate received" flag: MPI_Reduce with MPI_MAX collects the flags at
        // grid rank 0 (result is 1 if ANY node received it), and MPI_Bcast sends that result
        // back to every node. All nodes therefore stop on the same tick.
        if (!terminate_received && termination_received(world_comm, master_rank)) {
            terminate_received = true;
        }

        int stop_local = terminate_received ? 1 : 0;  // This node's flag
        int stop_global = 0;                          // 1 if any node has the terminate signal
        double stop_check_start = MPI_Wtime();
        MPI_Reduce(&stop_local, &stop_global, 1, MPI_INT, MPI_MAX, 0, comm2D);
        MPI_Bcast(&stop_global, 1, MPI_INT, 0, comm2D);
        double stop_check_time = MPI_Wtime() - stop_check_start;
        summary.total_stop_check_time += stop_check_time;
        if (stop_check_time > summary.max_stop_check_time) {
            summary.max_stop_check_time = stop_check_time;
        }

        if (stop_global) {
            if (!terminate_received) {
                // Another node saw the signal first; the Base Station sent one to every node,
                // so receive ours now so that no message is left unread
                int dummy;
                MPI_Recv(&dummy, 1, MPI_INT, master_rank, TAG_TERMINATE, world_comm, MPI_STATUS_IGNORE);
            }
            break;
        }

        // Wait until the next tick
        sleep_ms(TICK_INTERVAL_MS);
    }

    // -------------------------------------------------------------------------
    // 5. Graceful shutdown
    // -------------------------------------------------------------------------

    // Acknowledge the terminate signal by sending this node's statistics to the Base Station
    summary.ticks = simulation_tick;
    MPI_Send(&summary, sizeof(NodeSummaryMsg), MPI_BYTE, master_rank, TAG_TERMINATE, world_comm);

    printf("[Node %d (%d,%d)] Terminate signal received after %d ticks. Acknowledged and shutting down.\n",
           global_rank, coord[0], coord[1], simulation_tick);
    fflush(stdout);

    // Close the log file and free the grid communicator
    if (pFile != NULL) {
        fclose(pFile);
    }
    MPI_Comm_free(&comm2D);
    return 0;
}

// =============================================================================
// Task 2: Base Station (master_io), runs on the last rank
// =============================================================================

/**
 * Receive buffer large enough for any message a node sends to the Base Station.
 * All are sent as MPI_BYTE, so any of them can be received into this union.
 */
typedef union {
    IncidentReportMsg report;   // TAG_INCIDENT_REPORT
    NodeStatusMsg status;       // TAG_STATUS_UPDATE
    NodeSummaryMsg summary;     // TAG_TERMINATE (acknowledgement)
} BaseStationInbox;

/**
 * Everything the Base Station knows about the grid, plus its running counters.
 */
typedef struct {
    MPI_Comm world_comm;
    int rank, num_slaves, nrows, ncols;
    int period_ms, duration_s;
    double period_s;            // Predefined period of the information update mechanism
    FILE *log_file;
    char log_filename[128];

    int *availability;          // Last known free ports per node, -1 = never heard of
    double *info_time;          // MPI_Wtime when availability[] was last updated, -1 = never
    double *last_report_time;   // MPI_Wtime of the node's last incident report, -1 = never
    NodeSummaryMsg *summaries;  // Per-node statistics from the acknowledgements
    double *shutdown_latency;   // Terminate signal -> acknowledgement, per node (s)

    int acks;                   // Nodes that acknowledged the terminate signal
    bool terminate_sent;
    double start_time, terminate_time;

    int total_alerts;           // Incident reports received
    int status_updates;         // Availability updates received
    int bs_messages;            // Messages sent or received by the Base Station
    int reported_nbr_msgs;      // Neighbour messages described in incident reports
    int suggestion_replies;     // Replies that suggested candidate nodes
    int no_node_replies;        // Replies saying no nodes are available nearby
    double total_processing;    // Sum of receive -> reply processing times (s)
    double total_status_delay;  // Sum of status update one-way delays (s)
    double max_status_delay;    // Largest status update one-way delay (s)
} BaseStation;

#define TIMING_CSV_FILENAME "comm_timing.csv"  // One row appended per run, used for Task 3 graphs

// (row, column) offsets of the four neighbours of a cell: Left, Right, Top, Bottom
static const int NBR_OFFSETS[MAX_NEIGHBORS][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};

/**
 * Formats current system time as a readable string.
 */
static void get_time_string(char *buffer, size_t max_len) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    snprintf(buffer, max_len, "%04d-%02d-%02d %02d:%02d:%02d",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);
}

/**
 * True if the event at time t (MPI_Wtime, -1 = never) happened within the predefined period.
 */
static bool within_period(double t, double now, double period_s) {
    return t >= 0.0 && (now - t) <= period_s;
}

/**
 * Prints the last known availability of every node, laid out as the grid.
 * Each cell shows (row,column), the node number and its free ports; '?' means no data yet.
 */
static void print_grid(FILE *out, const BaseStation *bs) {
    for (int i = 0; i < bs->nrows; i++) {
        for (int j = 0; j < bs->ncols; j++) {
            int idx = TwoDToOneD(i, j, bs->ncols);
            if (bs->availability[idx] < 0)
                fprintf(out, "(%d,%d)N%d:?\t", i, j, idx);
            else
                fprintf(out, "(%d,%d)N%d:%d\t", i, j, idx, bs->availability[idx]);
        }
        fprintf(out, "\n");
    }
}

/**
 * Answers the clock synchronisation requests of every EV node (CLOCK_SYNC_ROUNDS each),
 * replying to each request immediately with the Base Station's current MPI_Wtime.
 * Runs once before the simulation starts and receives only TAG_CLOCK_SYNC messages.
 */
static void bs_serve_clock_sync(MPI_Comm world_comm, int num_slaves) {
    for (int i = 0; i < num_slaves * CLOCK_SYNC_ROUNDS; i++) {
        int request;
        MPI_Status status;
        MPI_Recv(&request, 1, MPI_INT, MPI_ANY_SOURCE, TAG_CLOCK_SYNC, world_comm, &status);
        double now = MPI_Wtime();
        MPI_Send(&now, 1, MPI_DOUBLE, status.MPI_SOURCE, TAG_CLOCK_SYNC, world_comm);
    }
}

/**
 * Prepares the Base Station: works out the grid size, opens the log file, allocates the
 * per-node arrays, synchronises clocks with the EV nodes and starts the simulation clock.
 */
static void bs_init(BaseStation *bs, MPI_Comm world_comm, int nrows, int ncols, int period_ms, int duration_s) {
    int global_size, dims[2];
    char time_buf[64];

    *bs = (BaseStation){0};  // All counters start at zero
    bs->world_comm = world_comm;
    MPI_Comm_size(world_comm, &global_size);
    MPI_Comm_rank(world_comm, &bs->rank);
    bs->num_slaves = global_size - 1;  // Every process except the Base Station is an EV node

    // If no grid size was given, compute the same shape the EV nodes compute with MPI_Dims_create
    if (nrows == 0 && ncols == 0) {
        dims[0] = dims[1] = 0;
        MPI_Dims_create(bs->num_slaves, 2, dims);
        nrows = dims[0];
        ncols = dims[1];
    }
    bs->nrows = nrows;
    bs->ncols = ncols;
    bs->period_ms = period_ms;
    bs->duration_s = duration_s;
    bs->period_s = period_ms / 1000.0;  // MPI_Wtime works in seconds

    // -------------------------------------------------------------------------
    // Open Base Station Log File (named after the run label, e.g. base_station_blocking.log)
    // -------------------------------------------------------------------------
    snprintf(bs->log_filename, sizeof(bs->log_filename), "base_station_%s.log", RUN_LABEL);
    bs->log_file = fopen(bs->log_filename, "w");
    if (!bs->log_file) {
        perror("Base Station: Failed to open log file");
        MPI_Abort(world_comm, 1);
    }

    get_time_string(time_buf, sizeof(time_buf));
    fprintf(bs->log_file, "========================================================================\n");
    fprintf(bs->log_file, "EV Charging Network Base Station Log\n");
    fprintf(bs->log_file, "Log Initialized at: %s\n", time_buf);
    fprintf(bs->log_file, "Version: %s - %s\n", RUN_LABEL, RUN_DESC);
    fprintf(bs->log_file, "Grid Topology Size: %d x %d (%d EV Charging Nodes), Base Station = rank %d\n",
            nrows, ncols, bs->num_slaves, bs->rank);
    fprintf(bs->log_file, "Predefined Period: %d ms | Simulation Duration: %d s\n", period_ms, duration_s);
    fprintf(bs->log_file, "========================================================================\n\n");
    fflush(bs->log_file);

    printf("Root Rank: %d. Comm Size: %d: Grid Dimension = [%d x %d]\n", bs->rank, global_size, nrows, ncols);
    printf("[Base Station | Rank %d] Version %s. Listening (period %d ms, duration %d s). Output log: %s\n",
           bs->rank, RUN_LABEL, period_ms, duration_s, bs->log_filename);
    fflush(stdout);

    // -------------------------------------------------------------------------
    // Per-node arrays, indexed by node rank (0 .. num_slaves-1)
    // -------------------------------------------------------------------------
    bs->availability = (int *)malloc(bs->num_slaves * sizeof(int));
    bs->info_time = (double *)malloc(bs->num_slaves * sizeof(double));
    bs->last_report_time = (double *)malloc(bs->num_slaves * sizeof(double));
    for (int i = 0; i < bs->num_slaves; i++) {
        bs->availability[i] = -1;      // -1 = no availability data yet
        bs->info_time[i] = -1.0;       // -1 = never updated
        bs->last_report_time[i] = -1.0;  // -1 = never sent an incident report
    }
    bs->summaries = (NodeSummaryMsg *)calloc(bs->num_slaves, sizeof(NodeSummaryMsg));
    bs->shutdown_latency = (double *)calloc(bs->num_slaves, sizeof(double));

    // Let every node measure its clock offset before the simulation starts
    bs_serve_clock_sync(world_comm, bs->num_slaves);
    bs->start_time = MPI_Wtime();  // Simulation clock starts now
}

/**
 * Returns true while at least one node has not yet acknowledged the terminate signal.
 */
static bool bs_running(const BaseStation *bs) {
    return bs->acks < bs->num_slaves;
}

/**
 * Sends the terminate signal to every EV node once the simulation duration has passed.
 * Does nothing if the duration has not passed yet, or if the signal was already sent.
 */
static void bs_check_duration(BaseStation *bs) {
    if (bs->terminate_sent || MPI_Wtime() - bs->start_time < bs->duration_s) {
        return;
    }

    printf("\n[Base Station | Rank %d] Simulation window completed. Sending terminate signal...\n", bs->rank);
    fflush(stdout);

    // One terminate message to each node (ranks 0 .. num_slaves-1); the time is recorded to
    // measure how long each node takes to acknowledge
    int send_Val = TAG_TERMINATE;
    bs->terminate_time = MPI_Wtime();
    for (int i = 0; i < bs->num_slaves; i++) {
        MPI_Send(&send_Val, 1, MPI_INT, i, TAG_TERMINATE, bs->world_comm);
        bs->bs_messages++;
    }
    bs->terminate_sent = true;
}

/**
 * Handles one incident report: updates the Base Station's knowledge, applies the
 * information update mechanism, replies to the reporting node (unless shutting down)
 * and logs everything.
 */
static void handle_incident_report(BaseStation *bs, int source, const IncidentReportMsg *report) {
    char time_buf[64];
    double t_recv = MPI_Wtime();  // Time the report was received; start of the processing time
    bs->total_alerts++;                                  // One more congestion alert
    bs->reported_nbr_msgs += report->neighbor_msg_count; // Neighbour messages the node described

    int reporter = report->reporting_node_rank;
    int ncols = bs->ncols;
    FILE *log_file = bs->log_file;

    // -----------------------------------------------------------------
    // Update knowledge: the report carries the reporter's and its neighbours' availability
    // -----------------------------------------------------------------
    bs->last_report_time[reporter] = t_recv;  // Used later to check "reported within the period"
    bs->availability[reporter] = report->reporting_node_free_ports;
    bs->info_time[reporter] = t_recv;
    for (int i = 0; i < report->num_neighbors; i++) {
        bs->availability[report->neighbors[i].rank] = report->neighbors[i].free_ports;
        bs->info_time[report->neighbors[i].rank] = t_recv;
    }

    // Print the alert on the terminal (and note if it arrived too late to be answered)
    get_time_string(time_buf, sizeof(time_buf));
    printf("[Base Station] Alert #%d received from Node %d (Coords: %d,%d) at %s%s\n",
           bs->total_alerts, reporter, report->reporting_node_coords[0], report->reporting_node_coords[1],
           time_buf, bs->terminate_sent ? " (after shutdown started, not answered)" : "");
    fflush(stdout);

    // -----------------------------------------------------------------
    // Information update mechanism (specification Task 2)
    // A neighbour that also reported within the period is congested together with all of
    // its own neighbours. A neighbour that did NOT report still has a free neighbour of
    // its own, so those distance-2 nodes are the candidates.
    // -----------------------------------------------------------------
    bool nbr_reported[MAX_NEIGHBORS];       // Per neighbour: reported within the period?
    int nbrs_reported = 0;                  // How many neighbours reported within the period
    int candidates[MAX_SUGGESTED_NODES];    // Distance-2 nodes to suggest
    int candidate_count = 0;

    for (int i = 0; i < report->num_neighbors; i++) {
        int n = report->neighbors[i].rank;

        // Did neighbour n also send an incident report within the predefined period?
        nbr_reported[i] = within_period(bs->last_report_time[n], t_recv, bs->period_s);
        if (nbr_reported[i]) {
            // This neighbour and its own neighbours are congested: nothing to suggest through it
            nbrs_reported++;
            continue;
        }

        // Convert neighbour n's rank to (row, column), then visit its own four neighbours
        int ni = n / ncols, nj = n % ncols;
        for (int k = 0; k < MAX_NEIGHBORS; k++) {
            int ci = ni + NBR_OFFSETS[k][0];
            int cj = nj + NBR_OFFSETS[k][1];
            if (ci < 0 || ci >= bs->nrows || cj < 0 || cj >= ncols) continue;  // Outside the grid

            int c = TwoDToOneD(ci, cj, ncols);
            if (c == reporter) continue; // In a grid, a neighbour's neighbour is the reporter or 2 away

            // Skip nodes recently known to be congested
            if (within_period(bs->info_time[c], t_recv, bs->period_s) &&
                bs->availability[c] <= CONGESTION_THRESHOLD) continue;

            // Two non-reporting neighbours can share a distance-2 node; add it only once
            bool duplicate = false;
            for (int m = 0; m < candidate_count; m++) {
                if (candidates[m] == c) duplicate = true;
            }
            if (!duplicate && candidate_count < MAX_SUGGESTED_NODES) {
                candidates[candidate_count++] = c;
            }
        }
    }

    // Build the reply: the candidates found above, or "no nodes available" if there are none
    BaseStationResponseMsg response;
    response.target_node_rank = reporter;
    response.status_code = (candidate_count > 0) ? STATUS_NODES_AVAILABLE : STATUS_NO_NODES_AVAIL;
    response.num_suggested_nodes = candidate_count;
    response.nbrs_reported_in_period = nbrs_reported;
    for (int i = 0; i < candidate_count; i++) {
        // Rank and (row, column) of each suggested node
        response.suggested_ranks[i] = candidates[i];
        response.suggested_coords[i][0] = candidates[i] / ncols;
        response.suggested_coords[i][1] = candidates[i] % ncols;
    }
    // Processing time, sent back so the node can separate it from the communication time
    response.bs_processing_time = MPI_Wtime() - t_recv;
    bs->total_processing += response.bs_processing_time;

    // After the terminate signal the node stops waiting for a reply, so none is sent
    if (!bs->terminate_sent) {
        MPI_Send(&response, sizeof(BaseStationResponseMsg), MPI_BYTE,
                 source, TAG_REDIRECT_INFO, bs->world_comm);
        bs->bs_messages++;
        // Count which kind of reply was sent
        if (candidate_count > 0)
            bs->suggestion_replies++;
        else
            bs->no_node_replies++;
    }

    // -----------------------------------------------------------------
    // Write the report and the decision to the log file
    // -----------------------------------------------------------------

    // When the congestion was discovered (node's clock) and when the report was received
    fprintf(log_file, "------------------------------------------------------------------------\n");
    fprintf(log_file, "[INCIDENT REPORT #%d] Timestamp: %04d-%02d-%02d %02d:%02d:%02d (received %s)\n",
            bs->total_alerts,
            report->time_discovered.year, report->time_discovered.month, report->time_discovered.day,
            report->time_discovered.hour, report->time_discovered.minute, report->time_discovered.second,
            time_buf);
    // Who reported, and its availability
    fprintf(log_file, "Reporting Node: %d | Coordinates: (%d,%d) | Free Ports: %d / %d\n",
            reporter, report->reporting_node_coords[0], report->reporting_node_coords[1],
            report->reporting_node_free_ports, DEFAULT_PORT_COUNT);
    // Size and duration of the node's neighbour communication
    fprintf(log_file, "Neighbour Communications: %d messages exchanged (%d bytes total) in %.3f ms\n",
            report->neighbor_msg_count, report->total_msg_size_bytes, report->neighbor_exchange_time * 1000.0);
    // Each neighbour's availability, and whether it also reported within the period
    fprintf(log_file, "Neighbour Statuses (period %.0f ms):\n", bs->period_s * 1000.0);
    for (int i = 0; i < report->num_neighbors; i++) {
        fprintf(log_file, "  - Neighbour Node %d at (%d,%d): %d free ports, %s\n",
                report->neighbors[i].rank, report->neighbors[i].coords[0],
                report->neighbors[i].coords[1], report->neighbors[i].free_ports,
                nbr_reported[i] ? "also reported within period" : "no report within period");
    }

    // The decision: which nodes were suggested, or why none were
    fprintf(log_file, "Base Station Decision: ");
    if (candidate_count > 0) {
        fprintf(log_file, "%d/%d neighbours reported within period; suggesting %d node(s) at Manhattan Distance 2:\n",
                nbrs_reported, report->num_neighbors, candidate_count);
        for (int i = 0; i < candidate_count; i++) {
            int c = candidates[i];
            // Explain what the Base Station knew about each suggested node
            if (bs->availability[c] < 0)
                fprintf(log_file, "  -> Suggested Node %d at Coordinates (%d,%d), no availability data yet\n",
                        c, c / ncols, c % ncols);
            else if (within_period(bs->info_time[c], t_recv, bs->period_s))
                fprintf(log_file, "  -> Suggested Node %d at Coordinates (%d,%d), %d free ports (recent)\n",
                        c, c / ncols, c % ncols, bs->availability[c]);
            else
                fprintf(log_file, "  -> Suggested Node %d at Coordinates (%d,%d), last known %d free ports "
                                  "%.0f ms ago (older than period, assumed recovered)\n",
                        c, c / ncols, c % ncols, bs->availability[c], (t_recv - bs->info_time[c]) * 1000.0);
        }
    } else if (nbrs_reported == report->num_neighbors) {
        // Every neighbour reported within the period
        fprintf(log_file, "All %d neighbours reported within period; no nodes available nearby.\n",
                report->num_neighbors);
    } else {
        // Some neighbours did not report, but all their distance-2 nodes are known to be congested
        fprintf(log_file, "%d/%d neighbours reported within period, but every distance-2 node is "
                          "recently known to be congested; no nodes available nearby.\n",
                nbrs_reported, report->num_neighbors);
    }
    fprintf(log_file, "Base Station processing time: %.3f ms\n", response.bs_processing_time * 1000.0);
    if (bs->terminate_sent) {
        fprintf(log_file, "Note: received after the terminate signal; no reply sent.\n");
    }
    fprintf(log_file, "------------------------------------------------------------------------\n\n");
    fflush(log_file);  // Write to disk now, so the log is complete even if the program stops early
}

/**
 * Handles one message that the receive loop in master_io() has already received. The tag tells which
 * kind of message it is, and therefore which member of the union holds the data.
 */
static void bs_handle_message(BaseStation *bs, int source, int tag, const BaseStationInbox *msg) {
    bs->bs_messages++;  // Count every message received
    switch (tag) {
        case TAG_INCIDENT_REPORT: {
            // Congestion alert: decide on suggestions, reply and log
            handle_incident_report(bs, source, &msg->report);
            break;
        }
        case TAG_STATUS_UPDATE: {
            // Fresh availability for one node
            double now = MPI_Wtime();
            bs->status_updates++;
            bs->availability[msg->status.node_rank] = msg->status.free_ports;
            bs->info_time[msg->status.node_rank] = now;
            // One-way delay: receive time minus send time, both on the Base Station's clock.
            // Includes the time until the Base Station picked the message up.
            double delay = now - msg->status.send_time;
            bs->total_status_delay += delay;
            if (delay > bs->max_status_delay) bs->max_status_delay = delay;
            break;
        }
        case TAG_TERMINATE: {
            // Acknowledgement carrying the node's statistics. Each node sends it last, and MPI
            // keeps messages from one sender in order, so nothing from that node is left behind.
            bs->summaries[source] = msg->summary;
            // Terminate signal -> acknowledgement; includes finishing the node's current tick
            bs->shutdown_latency[source] = MPI_Wtime() - bs->terminate_time;
            bs->acks++;
            break;
        }
        default: {
            // Unknown message type: report it and ignore it
            printf("Master. Error! Invalid Tag %d received from rank %d\n", tag, source);
            break;
        }
    }
}

/**
 * Called after every node has acknowledged: writes the summary to the log and the CSV,
 * prints the final grid, and frees all memory.
 */
static void bs_finish(BaseStation *bs) {
    char time_buf[64];
    FILE *log_file = bs->log_file;
    int num_slaves = bs->num_slaves;
    double simulation_time = MPI_Wtime() - bs->start_time;  // Total run time in seconds

    // -------------------------------------------------------------------------
    // Final Summary (all nodes have acknowledged, so no messages are pending)
    // -------------------------------------------------------------------------
    // Totals and maxima over all nodes, computed from the acknowledgement statistics
    int total_ticks = 0, total_exchanges = 0, total_reports = 0, total_nbr_msgs = 0, total_nbr_bytes = 0;
    double sum_exchange = 0.0, max_exchange = 0.0, sum_rtt = 0.0, max_rtt = 0.0;
    double sum_comm = 0.0, max_comm = 0.0, sum_stopcheck = 0.0, max_stopcheck = 0.0;
    double sum_shutdown = 0.0, max_shutdown = 0.0, max_clock_err = 0.0;

    // Overall results of the run
    get_time_string(time_buf, sizeof(time_buf));
    fprintf(log_file, "\n========================================================================\n");
    fprintf(log_file, "SIMULATION SUMMARY REPORT\n");
    fprintf(log_file, "Shutdown Time: %s\n", time_buf);
    fprintf(log_file, "Version: %s\n", RUN_LABEL);
    fprintf(log_file, "Simulation Time: %.3f s (terminate signal after %d s)\n", simulation_time, bs->duration_s);
    fprintf(log_file, "Predefined Period: %d ms\n", bs->period_ms);
    fprintf(log_file, "Total Congestion Alerts Detected: %d\n", bs->total_alerts);
    fprintf(log_file, "Replies: %d with suggested nodes, %d with no nodes available nearby\n",
            bs->suggestion_replies, bs->no_node_replies);
    fprintf(log_file, "Availability Updates Received: %d\n", bs->status_updates);
    fprintf(log_file, "Base Station Messages (sent + received): %d\n", bs->bs_messages);
    fprintf(log_file, "Neighbour Messages Described in Reports: %d\n\n", bs->reported_nbr_msgs);

    // One table row per node, using the statistics each node sent with its acknowledgement
    fprintf(log_file, "Per-node statistics (from termination acknowledgements):\n");
    fprintf(log_file, "Node\tCoords\tTicks\tCongest\tLocalRedir\tReports\tNbrMsgs\tNbrBytes\t"
                      "AvgExch(ms)\tMaxExch(ms)\tAvgRTT(ms)\tMaxRTT(ms)\tAvgStopChk(ms)\tShutdown(ms)\n");
    for (int i = 0; i < num_slaves; i++) {
        NodeSummaryMsg *s = &bs->summaries[i];

        // This node's averages (guarding against division by zero)
        double avg_exchange = s->exchanges > 0 ? s->total_exchange_time / s->exchanges : 0.0;
        double avg_rtt = s->reports_sent > 0 ? s->total_report_rtt / s->reports_sent : 0.0;
        double avg_stopcheck = s->ticks > 0 ? s->total_stop_check_time / s->ticks : 0.0;
        fprintf(log_file, "%d\t(%d,%d)\t%d\t%d\t%d\t\t%d\t%d\t%d\t\t%.3f\t\t%.3f\t\t%.3f\t\t%.3f\t\t%.3f\t\t%.3f\n",
                s->node_rank, s->coords[0], s->coords[1], s->ticks, s->congestion_events,
                s->local_redirects, s->reports_sent, s->neighbor_msgs, s->neighbor_bytes,
                avg_exchange * 1000.0, s->max_exchange_time * 1000.0, avg_rtt * 1000.0,
                s->max_report_rtt * 1000.0, avg_stopcheck * 1000.0, bs->shutdown_latency[i] * 1000.0);

        // Add this node to the totals and keep the largest values seen
        total_ticks += s->ticks;
        total_exchanges += s->exchanges;
        total_reports += s->reports_sent;
        total_nbr_msgs += s->neighbor_msgs;
        total_nbr_bytes += s->neighbor_bytes;
        sum_exchange += s->total_exchange_time;
        sum_rtt += s->total_report_rtt;
        sum_comm += s->total_report_comm;
        sum_stopcheck += s->total_stop_check_time;
        sum_shutdown += bs->shutdown_latency[i];
        if (s->max_exchange_time > max_exchange) max_exchange = s->max_exchange_time;
        if (s->max_report_rtt > max_rtt) max_rtt = s->max_report_rtt;
        if (s->max_report_comm > max_comm) max_comm = s->max_report_comm;
        if (s->max_stop_check_time > max_stopcheck) max_stopcheck = s->max_stop_check_time;
        if (bs->shutdown_latency[i] > max_shutdown) max_shutdown = bs->shutdown_latency[i];
        if (s->clock_sync_error > max_clock_err) max_clock_err = s->clock_sync_error;
    }

    // Averages in milliseconds. Round trips are only measured for reports that were answered.
    int answered = bs->suggestion_replies + bs->no_node_replies;
    double avg_exchange_ms = total_exchanges > 0 ? sum_exchange / total_exchanges * 1000.0 : 0.0;
    double avg_rtt_ms = answered > 0 ? sum_rtt / answered * 1000.0 : 0.0;
    double avg_comm_ms = answered > 0 ? sum_comm / answered * 1000.0 : 0.0;
    double avg_proc_ms = bs->total_alerts > 0 ? bs->total_processing / bs->total_alerts * 1000.0 : 0.0;
    double avg_stopcheck_ms = total_ticks > 0 ? sum_stopcheck / total_ticks * 1000.0 : 0.0;
    double avg_shutdown_ms = num_slaves > 0 ? sum_shutdown / num_slaves * 1000.0 : 0.0;
    double avg_status_ms = bs->status_updates > 0 ? bs->total_status_delay / bs->status_updates * 1000.0 : 0.0;

    // Message counts (nodes may send a report just before shutdown that is never answered)
    fprintf(log_file, "\nIncident reports: %d sent by nodes, %d received by the Base Station\n",
            total_reports, bs->total_alerts);
    fprintf(log_file, "Total Messages/Events: %d (Base Station %d + Node-to-Node %d, %d bytes)\n",
            bs->bs_messages + total_nbr_msgs, bs->bs_messages, total_nbr_msgs, total_nbr_bytes);
    // Communication delay for each type of message (Task 3 analysis)
    fprintf(log_file, "Communication delay per message type:\n");
    fprintf(log_file, "  Neighbour status exchange:                       avg %.3f ms, max %.3f ms (%d exchanges)\n",
            avg_exchange_ms, max_exchange * 1000.0, total_exchanges);
    fprintf(log_file, "  Incident report -> redirect reply round trip:    avg %.3f ms, max %.3f ms (%d answered)\n",
            avg_rtt_ms, max_rtt * 1000.0, answered);
    fprintf(log_file, "    of which communication (minus BS processing):  avg %.3f ms, max %.3f ms\n",
            avg_comm_ms, max_comm * 1000.0);
    fprintf(log_file, "    of which Base Station processing:             avg %.3f ms\n", avg_proc_ms);
    fprintf(log_file, "  Stop check (MPI_Reduce + MPI_Bcast per tick):   avg %.3f ms, max %.3f ms\n",
            avg_stopcheck_ms, max_stopcheck * 1000.0);
    fprintf(log_file, "  Terminate signal -> acknowledgement:             avg %.3f ms, max %.3f ms\n",
            avg_shutdown_ms, max_shutdown * 1000.0);
    fprintf(log_file, "  Status update (node -> Base Station, one way):   avg %.3f ms, max %.3f ms (%d updates)\n",
            avg_status_ms, bs->max_status_delay * 1000.0, bs->status_updates);
    fprintf(log_file, "    clock synchronisation error: at most %.3f ms\n", max_clock_err * 1000.0);
    // Final view of the grid, then confirm the graceful shutdown
    fprintf(log_file, "\nLast known availability grid ('?' = no data):\n");
    print_grid(log_file, bs);
    fprintf(log_file, "\nSystem Status: All %d nodes acknowledged termination. Graceful Shutdown Completed.\n",
            num_slaves);
    fprintf(log_file, "========================================================================\n");
    fclose(log_file);

    // Append one row of results to the CSV (opened in append mode), so runs with different
    // grid sizes and run labels build up a single table for the Task 3 graphs
    FILE *csv = fopen(TIMING_CSV_FILENAME, "a");
    if (csv != NULL) {
        // Write the column header only if the file is still empty
        fseek(csv, 0, SEEK_END);
        if (ftell(csv) == 0) {
            fprintf(csv, "version,rows,cols,nodes,period_ms,sim_time_s,alerts,"
                         "suggest_replies,no_node_replies,status_updates,bs_messages,neighbor_msgs,"
                         "avg_exchange_ms,max_exchange_ms,avg_report_rtt_ms,max_report_rtt_ms,"
                         "avg_report_comm_ms,max_report_comm_ms,avg_bs_proc_ms,avg_stopcheck_ms,"
                         "max_stopcheck_ms,avg_shutdown_ms,max_shutdown_ms,"
                         "avg_status_delay_ms,max_status_delay_ms,max_clock_err_ms\n");
        }
        // One row: run label, grid size, settings, counts, then delays in milliseconds
        fprintf(csv, "%s,%d,%d,%d,%d,%.3f,%d,%d,%d,%d,%d,%d,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n",
                RUN_LABEL, bs->nrows, bs->ncols, num_slaves, bs->period_ms, simulation_time,
                bs->total_alerts, bs->suggestion_replies, bs->no_node_replies, bs->status_updates,
                bs->bs_messages, total_nbr_msgs,
                avg_exchange_ms, max_exchange * 1000.0, avg_rtt_ms, max_rtt * 1000.0,
                avg_comm_ms, max_comm * 1000.0, avg_proc_ms, avg_stopcheck_ms, max_stopcheck * 1000.0,
                avg_shutdown_ms, max_shutdown * 1000.0,
                avg_status_ms, bs->max_status_delay * 1000.0, max_clock_err * 1000.0);
        fclose(csv);
    }

    // Show the final grid on the terminal as well
    printf("\nLast known availability grid ('?' = no data):\n");
    print_grid(stdout, bs);
    printf("All Slaves terminated. Log saved to '%s'. Base Station finished.\n", bs->log_filename);
    fflush(stdout);

    // Free every array allocated in bs_init
    free(bs->availability);
    free(bs->info_time);
    free(bs->last_report_time);
    free(bs->summaries);
    free(bs->shutdown_latency);
}

// This is the master (Base Station)
int master_io(MPI_Comm world_comm, MPI_Comm comm, int nrows, int ncols, int period_ms, int duration_s) {
    BaseStation bs;
    BaseStationInbox inbox;
    MPI_Status status;
    (void)comm; // The master is alone in its split communicator

    // Open the log, set up the per-node arrays and start the clock
    bs_init(&bs, world_comm, nrows, ncols, period_ms, duration_s);

    // Listen until every node has acknowledged the terminate signal
    while (bs_running(&bs)) {
        // Send the terminate signal once the simulation duration has passed
        bs_check_duration(&bs);

        // Blocks until any node sends a report, an availability update or an acknowledgement
        MPI_Recv(&inbox, sizeof(BaseStationInbox), MPI_BYTE, MPI_ANY_SOURCE, MPI_ANY_TAG,
                 world_comm, &status);
        // status tells who sent the message and which kind it is (tag)
        bs_handle_message(&bs, status.MPI_SOURCE, status.MPI_TAG, &inbox);
    }

    // Every node has acknowledged: write the summary and free memory
    bs_finish(&bs);
    return 0;
}

// =============================================================================
// Program entry point
// =============================================================================

int main(int argc, char **argv)
{
    int rank, size, provided;
    int nrows = 0, ncols = 0;                    // 0 x 0 means "let MPI choose the grid shape"
    int period_ms = DEFAULT_REPORT_PERIOD_MS;    // Information update period (ms)
    int duration_s = DEFAULT_SIM_DURATION_S;     // Simulation length (s)
    MPI_Comm new_comm;                           // Communicator after the master/slave split

    // Initialise MPI with thread support. Only the main thread of each process calls MPI
    // (the OpenMP port threads never do), so MPI_THREAD_FUNNELED is the level required.
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // Stop if the MPI library cannot provide the requested thread level
    if (provided < MPI_THREAD_FUNNELED) {
        if (rank == 0)
            printf("ERROR: MPI library only provides thread level %d; MPI_THREAD_FUNNELED is required\n",
                   provided);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    // Read the optional command-line arguments: rows cols [period_ms [duration_s]]
    if (argc >= 3) {
        nrows = atoi(argv[1]);
        ncols = atoi(argv[2]);
        if (argc >= 4) period_ms = atoi(argv[3]);
        if (argc >= 5) duration_s = atoi(argv[4]);

        // The grid needs exactly rows * cols EV nodes plus one Base Station process
        if (!(nrows == 0 && ncols == 0) && (nrows * ncols) != (size - 1)) {
            if (rank == 0)
                printf("ERROR: nrows*ncols=%d * %d = %d != %d (1 process is reserved for the Base Station)\n",
                       nrows, ncols, nrows * ncols, size - 1);
            MPI_Finalize();
            return 0;
        }

        // A negative period or a non-positive duration makes no sense
        if (period_ms < 0 || duration_s <= 0) {
            if (rank == 0)
                printf("ERROR: period_ms must be >= 0 and duration_s must be > 0\n");
            MPI_Finalize();
            return 0;
        }
    }

    // At least one EV node and one Base Station are needed
    if (size < 2) {
        if (rank == 0)
            printf("ERROR: at least 2 processes are required (1 EV node + 1 Base Station)\n");
        MPI_Finalize();
        return 0;
    }

    // Split MPI_COMM_WORLD by colour: the last rank gets colour 1 (Base Station), every other
    // rank gets colour 0 (EV nodes). Key 0 keeps the original rank order inside each group.
    MPI_Comm_split(MPI_COMM_WORLD, rank == (size - 1), 0, &new_comm);

    // Each process runs its role. MPI_COMM_WORLD is passed as well, because the nodes and the
    // Base Station are in different groups and can only talk to each other through it.
    if (rank == size - 1)
        master_io(MPI_COMM_WORLD, new_comm, nrows, ncols, period_ms, duration_s);
    else
        slave_io(MPI_COMM_WORLD, new_comm, nrows, ncols);

    // Release the split communicator and shut MPI down
    MPI_Comm_free(&new_comm);
    MPI_Finalize();
    return 0;
}
