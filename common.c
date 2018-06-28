#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <numa.h>

#include "common.h"

void checkSyscall(const char *funcname, int result) {
  if (result < 0) {
    printf("%s failed: %s\n", funcname, strerror(errno));
    exit(EXIT_FAILURE);
  }
}

int create_udp_socket( const char *hostStr, unsigned short port, int receive ) {
  struct addrinfo hints, *ai;
  int result;

  char portStr[10];

  snprintf(portStr, sizeof portStr, "%d", port);

  hints.ai_family   = AF_INET; /* IPv4 */
  hints.ai_flags    = AI_NUMERICSERV; /* numerical ports only */
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;
  if ((result = getaddrinfo(hostStr, portStr, &hints, &ai)) != 0) {
    printf("getaddrinfo(\"%s\") failed: %s\n", hostStr, gai_strerror(result));
    exit(EXIT_FAILURE);
  }

  int fd;

  checkSyscall("socket()",
    fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol));

  if (receive) {
    const int on = 1;

    checkSyscall("setsockopt(SO_REUSEADDR)",
      setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on));

    checkSyscall("bind()",
      bind(fd, ai->ai_addr, ai->ai_addrlen));
  } else {
    checkSyscall("connect()",
      connect(fd, ai->ai_addr, ai->ai_addrlen));
  }

  freeaddrinfo(ai);

  return fd;
}

void start(struct timer *t) { gettimeofday(&t->begin, 0); }
void stop(struct timer *t)  { gettimeofday(&t->end, 0); }

double duration(const struct timer t) {
  return (double)(t.end.tv_sec - t.begin.tv_sec) + (double)(t.end.tv_usec - t.begin.tv_usec) / 1e6;
}

void timeval_add(struct timeval *t, size_t usec) {
  t->tv_sec += usec / 1000000;
  usec = usec % 1000000;

  if ((t->tv_usec += usec) >= 1000000) {
    t->tv_usec -= 1000000;
    t->tv_sec++;
  }
}

struct timeval timeval_diff(const struct timeval from, const struct timeval to) {
  struct timeval diff;

  diff.tv_sec = to.tv_sec - from.tv_sec;
  if (to.tv_usec >= from.tv_usec) {
    diff.tv_usec = to.tv_usec - from.tv_usec;
  } else {
    diff.tv_sec--;
    diff.tv_usec = 1000000 + to.tv_usec - from.tv_usec;
  }

  return diff;
}

int wait_until(const struct timeval t) {
  struct timeval now;
  gettimeofday(&now, 0);

  /* return 1 if deadline already passed (too late) */
  if( now.tv_sec > t.tv_sec ) return 1;
  if( now.tv_sec == t.tv_sec && now.tv_usec > t.tv_usec ) return 1;

  /* sleep until the deadline */
  struct timeval diff;
  diff = timeval_diff(now, t);
  select(0, NULL, NULL, NULL, &diff);

  /* success */
  return 0;
}

int nrNodes()
{
  return numa_max_node() + 1;
}

void setNodeAffinity(unsigned node)
{
  if (numa_available() == -1) {
    printf("ERROR: NUMA library functions not available (numa_available() returned -1).\n");
    exit(EXIT_FAILURE);
  }
  
  if (node > numa_max_possible_node()) {
    printf("ERROR: Requested to run on NUMA node %d but found no nodes beyond %d.\n", node, numa_max_possible_node());
    exit(EXIT_FAILURE);
  }

  /* force node + memory binding for future allocations */
  struct bitmask *numa_node = numa_allocate_nodemask();
  numa_bitmask_clearall(numa_node);
  numa_bitmask_setbit(numa_node, node);
  numa_bind(numa_node); /* sets both CPU and memory binding */

  /* only allow allocation on this node in case the numa_alloc_* functions are used */
  numa_set_strict(1);

  /* migrate currently used memory to our node */
  /* numa_migrate_pages(0, numa_all_nodes_ptr, numa_node); */

  /* cleanup */
  numa_bitmask_free(numa_node);
}
