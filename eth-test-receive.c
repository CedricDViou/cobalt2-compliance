#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <omp.h>

#include "common.h"
#include "eth-test-params.h"

struct report {
  double speed_gbps;
  double loss_perc;
};

struct report receive_data(const char *hostStr, unsigned short port) {
  struct report result = { 0.0, 0.0 };

  int fd = create_udp_socket(hostStr, port, 1);
  int i,j;

  struct message buffer[MSG_BATCHSIZE];

  /* setup sendmsg/recvmmsg structures */
  struct iovec iov[MSG_BATCHSIZE];
  struct mmsghdr msgs[MSG_BATCHSIZE];

  for( i = 0; i < MSG_BATCHSIZE; i++ ) {
    iov[i].iov_base = &buffer[i];
    iov[i].iov_len  = MAX_MSGSIZE;
    msgs[i].msg_hdr.msg_name    = NULL;
    msgs[i].msg_hdr.msg_namelen = 0;
    msgs[i].msg_hdr.msg_iov     = &iov[i];
    msgs[i].msg_hdr.msg_iovlen  = 1;
    msgs[i].msg_hdr.msg_control = NULL;
    msgs[i].msg_hdr.msg_controllen = 0;
    msgs[i].msg_hdr.msg_flags   = 0;
  }

#pragma omp barrier
  size_t first_packet_nr = 0;

  /* wait for the first message */
  printf("Waiting for first UDP packet...\n");
  checkSyscall("recvmmsg()", recvmmsg(fd, &msgs[0], 1, 0, NULL));

  first_packet_nr = buffer[0].packet_nr;

  /* send/receive messages */
  printf("Receiving UDP packets...\n");

  struct timer t;
  size_t total_num_bytes = 0, total_num_msgs = 0, max_packet_nr = first_packet_nr;

  start(&t);
  for( i = 0; i < NR_BATCHES; i++ ) {
    int num_msgs;
   
    checkSyscall("recvmmsg()",
      num_msgs = recvmmsg(fd, &msgs[0], MSG_BATCHSIZE, 0, NULL));

    /* accumulate result */
    for( j = 0; j < num_msgs; j++ ) {
      total_num_bytes += msgs[j].msg_len;

      size_t packet_nr = buffer[j].packet_nr;
      if (packet_nr > max_packet_nr) max_packet_nr = packet_nr;
    }

    total_num_msgs += num_msgs;
  }
  stop(&t);

  /* report speed */
  result.speed_gbps = total_num_bytes/GBPS/duration(t);
  result.loss_perc  = 100.0 * ((max_packet_nr - first_packet_nr) - total_num_msgs) / total_num_msgs;

  printf("Received %.2f GByte over %.2f seconds. Speed: %.2f Gbit/s\n",
    total_num_bytes/GBYTE,
    duration(t),
    total_num_bytes/GBPS/duration(t));
  printf("Received %ld messages, lost %ld messages.\n",
    total_num_msgs,
    (max_packet_nr - first_packet_nr) - total_num_msgs);

  /* Teardown */
  close(fd);

  return result;
}

void usage(const char *progname) {
  printf("Usage: %s -H hostname [options]\n", progname);
  printf("       %s -?\n", progname);
  printf("\n");
  printf("  -H      Host name (or IP address) to receive on.\n");
  printf("  -P      First port number to receive on [5000].\n");
  printf("  -h      Show this help.\n");
}

int main(int argc, char **argv) {
  /* Host to receive on. */
  const char *hostStr = NULL;
  /* First port to listen on. */
  int firstPort = 5000;
  /* Number of ports to listen on. */
  const int nrPorts = NR_PORTS;

  int i, opt;

  /* parse command-line options */
  while ((opt = getopt(argc, argv, "H:P:h")) != -1) {
    switch (opt) {
    case 'H':
      hostStr = strdup(optarg);
      break;

    case 'P':
      firstPort = atoi(optarg);
      break;

    case 'h':
      usage(argv[0]);
      return EXIT_SUCCESS;

    default: /* '?' */
      usage(argv[0]);
      return EXIT_FAILURE;
    }
  }

  if (optind < argc) {
    usage(argv[0]);
    return EXIT_FAILURE;
  }

  if (!hostStr) {
    usage(argv[0]);
    return EXIT_FAILURE;
  }

  /* print configuration */
  printf("Target host: %s\n", hostStr);
  printf("First port:  %d\n", firstPort);
  printf("Port count:  %d\n", nrPorts);

  /* initialise */
  omp_set_num_threads(nrPorts);

  /* receive data on all ports in parallel */
  struct report reports[nrPorts];

#pragma omp parallel for num_threads(nrPorts)
  for ( i = 0; i < nrPorts; i++ ) {
    reports[i] = receive_data(hostStr, firstPort + i);
  }

  /* calculate and show summary */
  printf(" ----- Test results -----\n");
  struct report totals = { 0.0, 0.0 };
  const int nr_reports = sizeof reports / sizeof reports[0];
  for ( i = 0; i < nr_reports; i++ ) {
    totals.speed_gbps += reports[i].speed_gbps; /* sum */
    totals.loss_perc  += reports[i].loss_perc / nr_reports; /* average */
  }

  printf("Test version: %s\n", VERSION);
  printf("Total speed:  %.2f Gbit/s\n", totals.speed_gbps);
  printf("Average loss: %.3f%%\n", totals.loss_perc);

  return EXIT_SUCCESS;
}
