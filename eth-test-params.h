#ifndef __ETH_TEST_PARAMS__
#define __ETH_TEST_PARAMS__

#include <stddef.h>

/* How many batches of UDP messages to send/receive. */
#define NR_BATCHES    512

/* Size of each batch, in messages. */
#define MSG_BATCHSIZE 128

/* Size of each message. */
#define MAX_MSGSIZE   8900 /* must fit in a Jumbo frame, after adding headers */

/* Desired speed per data stream. */
#define SPEED_BITS_PER_SEC (3UL*1000*1000*1000/4)


/* Number of ports to listen on.
 *
 * With 3 Gbit/s/antenna field, we can receive 3 antenna field on a 10GbE port.
 * Each antenna field uses 4 data streams, and thus 4 ports. This leads to 12 ports per interface.
 */
#define NR_PORTS      12

struct message {
    size_t packet_nr; /* incrementing packet number, used to detect loss. */
    char payload[MAX_MSGSIZE - sizeof(size_t)];
};

#endif

