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

void send_data(const char *hostStr, unsigned short port) {
  int fd = create_udp_socket(hostStr, port, 0);
  int i;

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
  printf("Sending UDP packets...\n");
  size_t packet_nr = 0;
  struct timeval first_packet_timestamp;
  gettimeofday(&first_packet_timestamp, 0);

  size_t late = 0;

  while( packet_nr < NR_BATCHES * MSG_BATCHSIZE * 2 ) { /* overshoot to allow for some loss */
    packet_nr ++;

    /* wait for deadline of next packet according to desired data rate */
    struct timeval next_packet_at = first_packet_timestamp;
    timeval_add(&next_packet_at, 1000000 * packet_nr * MAX_MSGSIZE / (SPEED_BITS_PER_SEC / 8));
    late += wait_until(next_packet_at);

    /* construct and send one packet */
    buffer[0].packet_nr = packet_nr;
    (void)sendmsg(fd, &msgs[0].msg_hdr, 0);
  }

  /* report */
  printf("Sent %ld packets, %.2f%% late.\n", packet_nr, 100.0*late/packet_nr);

  /* Teardown */
  close(fd);
}

void usage(const char *progname) {
  printf("Usage: %s -H hostname [options]\n", progname);
  printf("       %s -?\n", progname);
  printf("\n");
  printf("  -H      Host name (or IP address) to send to.\n");
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

  /* send data on all ports in parallel */
#pragma omp parallel for num_threads(nrPorts)
  for ( i = 0; i < nrPorts; i++ ) {
    send_data(hostStr, firstPort + i);
  }

  printf("Done.\n");

  return EXIT_SUCCESS;
}
