#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <numa.h>
#include <assert.h>
#include <omp.h>
#include <pthread.h>

#include "common.h"

/* Total number of packets to process */
#define NR_PACKETS                      (1024UL*1024)

/* Total number of stations (antenna fields) to simulate */
#define NR_STATIONS       18 /* 3 * number of 10GbE interfaces */

struct timeval now() {
  struct timeval result;
  gettimeofday(&result, 0);

  return result;
}

/* wait until the deadline for a specific packet has been reached. */
int wait_for_packet(const struct timeval first_packet_timestamp, size_t offset, double rate_bps) {
  struct timeval next_packet_at = first_packet_timestamp;
  timeval_add(&next_packet_at, 1000000 * offset / (rate_bps / 8));
  return wait_until(next_packet_at);
}

typedef enum { READ, WRITE, COPY, TRANSPOSE } transfer_t;

struct report {
  double desired_speed_gbps;
  double speed_gbps;
  double late_perc;
  int    nr_operations; /* read/write = 1, copy = 2 */
};

pthread_barrier_t start_barrier;
volatile int done = 0;

/* read/write/copy an amount of data with a fixed rate. */
struct report dram_test(transfer_t operation, const char *desc, size_t nr_bytes, size_t block_size, double gbits_per_sec) {
  struct report result;

  /* Setup */
  printf("Initialising %s...\n", desc);

  struct timer t;

  int *packet_input_buffer;
  int *packet_output_buffer;
  const size_t nr_elements = block_size / sizeof *packet_input_buffer;

  packet_input_buffer = malloc(block_size);
  packet_output_buffer = malloc(block_size);

  memset(packet_input_buffer, 42, block_size);
  memset(packet_output_buffer, 42, block_size);

  /* All threads must process at the same time. */
  pthread_barrier_wait(&start_barrier); /* can't use omp barrier, as we want all loops/teams to participate */
  printf("Starting %s...\n", desc);

  size_t offset = 0;
  struct timeval first_packet_timestamp = now();

  size_t late = 0;

  start(&t);
  while( !done && offset < nr_bytes ) {
    offset += block_size;

    /* wait for deadline of next packet according to desired data rate */
    late += wait_for_packet(first_packet_timestamp, offset, gbits_per_sec * 1000 * 1000 * 1000) * block_size;

    /* process one block of data */
    switch (operation) {
      int i, sum = 0;
      case READ:
        for( i = 0; i < nr_elements; i++ ) {
          sum += packet_input_buffer[i];
        }
        break;

      case WRITE:
        memset(packet_output_buffer, 42, block_size);
        break;

      case COPY:
        memcpy(packet_output_buffer, packet_input_buffer, block_size);
        break;

      case TRANSPOSE:
        #define XPOSE_CHUNK       9000 /* size of data chunks to reorder, in samples */
        for( i = 0; i < nr_elements / XPOSE_CHUNK; i++ ) {
          memcpy(&packet_output_buffer[i*XPOSE_CHUNK], &packet_input_buffer[(i * 7) % (nr_elements/XPOSE_CHUNK) * XPOSE_CHUNK], sizeof *packet_input_buffer * XPOSE_CHUNK);
        }
        break;
    }
  }
  stop(&t);
  done = 1; /* let other threads bail early to measure only overlapping speeds */

  /* Report */
  result.desired_speed_gbps = gbits_per_sec;
  result.speed_gbps = offset / duration(t) / GBPS;
  result.late_perc = 100.0 * late / offset;
  result.nr_operations = operation == READ || operation == WRITE ? 1 : 2;

  printf("%-5s (%s): Ran for %.2fs at %.2f Gbit/s, %.2f%% late.\n", 
    operation == READ ? "Read" :
    operation == WRITE ? "Write" :
    operation == COPY ? "Copy" :
    operation == TRANSPOSE ? "Xpose" :
    "???",
    desc,
    duration(t),
    result.speed_gbps,
    result.late_perc);

  /* Teardown */
  free(packet_output_buffer);
  free(packet_input_buffer);

  return result;
}

int main(int argc, char **argv) {
  #define NR_STEPS          10 /* Must match number of parallel sections below */

  /* initialise */
  pthread_barrier_init(&start_barrier, NULL, NR_STATIONS * NR_STEPS);

  omp_set_nested(1);
  omp_set_num_threads(NR_STATIONS * NR_STEPS);

  printf("Detected %d NUMA nodes.\n", nrNodes());
  printf("Using %d threads.\n", omp_get_max_threads());

  /* schedule all data transfers in parallel. */
  int station;
  struct report reports[NR_STATIONS][NR_STEPS];

  #pragma omp parallel for num_threads(NR_STATIONS)
  for (station = 0; station < NR_STATIONS; station++) {
    /* Evenly divide stations among the NUMA domains. */
    setNodeAffinity(station % nrNodes());

    #pragma omp parallel sections num_threads(NR_STEPS)
    {
      /* ----- Station data is received in chunks of 128 UDP packets, using recvmmsg */
      #define UDP_BUFFER_SIZE                 128

      /* NIC -> DRAM */
      #pragma omp section
      { reports[station][0] = dram_test( WRITE, "station input (NIC -> DRAM)", NR_PACKETS * 9000, UDP_BUFFER_SIZE * 9000, 3.0 ); }

      /* kernel -> user space */
      #pragma omp section
      { reports[station][1] = dram_test( COPY, "station input (kernel -> user)", NR_PACKETS * 9000, UDP_BUFFER_SIZE * 9000, 3.0 ); }

      /* user space -> MPI input buffer */
      #pragma omp section
      { reports[station][2] = dram_test( TRANSPOSE, "station input (user -> IB staging)", NR_PACKETS * 9000, UDP_BUFFER_SIZE * 9000, 3.0 ); }

      /* ----- We now switch to processing blocks of ~1s */
      #define PROCESSING_BUFFER_SIZE          1024

      /* MPI exchange */
      #pragma omp section
      { reports[station][3] = dram_test( COPY, "station input (IB exchange)", NR_PACKETS * 9000, PROCESSING_BUFFER_SIZE * 9000, 3.0 ); }

      /* Stage to GPU */
      #pragma omp section
      { reports[station][4] = dram_test( TRANSPOSE, "station input (GPU staging)", NR_PACKETS * 9000, PROCESSING_BUFFER_SIZE * 9000, 3.0 ); }

      /* DRAM -> GPU */
      #pragma omp section
      { reports[station][5] = dram_test( READ, "station input (DRAM -> GPU)", NR_PACKETS * 9000, PROCESSING_BUFFER_SIZE * 9000, 3.0 ); }

      /* ----- Emulate a minimum reduction of the data volume by this factor */
      #define REDUCTION_FACTOR                2

      /* GPU -> DRAM */
      #pragma omp section
      { reports[station][6] = dram_test( WRITE, "processing output (GPU -> DRAM)", NR_PACKETS * 9000 / REDUCTION_FACTOR, PROCESSING_BUFFER_SIZE * 9000, 3.0 / REDUCTION_FACTOR); }

      /* Stage output (BF mode) */
      #pragma omp section
      { reports[station][7] = dram_test( COPY, "processing output (BF staging)", NR_PACKETS * 9000 / REDUCTION_FACTOR, PROCESSING_BUFFER_SIZE * 9000, 3.0 / REDUCTION_FACTOR); }

      /* Copy output to TCP buffer (BF mode) */
      #pragma omp section
      { reports[station][8] = dram_test( COPY, "processing output (user -> kernel)", NR_PACKETS * 9000 / REDUCTION_FACTOR, PROCESSING_BUFFER_SIZE * 9000, 3.0 / REDUCTION_FACTOR); }

      /* DRAM -> NIC */
      #pragma omp section
      { reports[station][9] = dram_test( READ, "processing output (DRAM -> NIC)", NR_PACKETS * 9000 / REDUCTION_FACTOR, PROCESSING_BUFFER_SIZE * 9000, 3.0 / REDUCTION_FACTOR); }
    }
  }

  /* calculate and show summary */
  printf(" ----- Test results -----\n");
  struct report totals = { 0.0, 0.0, 0.0, 0 };
  const int nr_reports = sizeof reports / sizeof reports[0][0];
  for ( station = 0; station < NR_STATIONS; station++ ) {
    int i;

    for ( i = 0; i < NR_STEPS; i++ ) {
      totals.desired_speed_gbps += reports[station][i].desired_speed_gbps * reports[station][i].nr_operations; /* sum */
      totals.speed_gbps += reports[station][i].speed_gbps * reports[station][i].nr_operations; /* sum */
      totals.late_perc  += reports[station][i].late_perc / nr_reports; /* average */
    }
  }

  printf("Test version:    %s\n", VERSION);
  printf("Desired speed:   %.2f Gbit/s\n", totals.desired_speed_gbps);
  printf("Measured speed:  %.2f Gbit/s (%.2f%% of desired)\n", totals.speed_gbps, 100.0 * totals.speed_gbps / totals.desired_speed_gbps);
  printf("Average late:    %.3f%%\n", totals.late_perc);

  /* teardown */
  pthread_barrier_destroy(&start_barrier);

  return EXIT_SUCCESS;
}

