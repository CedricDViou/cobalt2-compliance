#ifndef __COMMON__
#define __COMMON__

#include <sys/time.h>
#include <stddef.h>

/* Version of this test suite */
#define VERSION       "1.0"

/* Number of bytes in a gigabyte, used when measuring data sizes. */
#define GBYTE         (1024.0*1024.0*1024.0)

/* Number of bytes in a gigabit, used when measuring transfer speeds. */
#define GBPS          (1000.0*1000.0*1000.0/8)

/* Bail with an error if the result of a syscall is failure (result < 0). */
void checkSyscall(const char *funcname, int result);

/* Return a file descriptor connecting to or from hostStr:portStr. */
int create_udp_socket( const char *hostStr, unsigned short port, int receive );

/* Timer to record a time span. */
struct timer {
  struct timeval begin, end;
};

void start(struct timer *t);
void stop(struct timer *t);

/* Return the duration between start(&t) and stop(&t), in seconds. */
double duration(const struct timer t);

/* Add `usec` microseconds to `t'. */
void timeval_add(struct timeval *t, const size_t usec);

/* Return `to' - `from'. */
struct timeval timeval_diff(const struct timeval from, const struct timeval to);

/* Sleep until moment `t'.
 *
 * Returns 1 if t already passed, 0 otherwise.
 */
int wait_until(const struct timeval t);

/*
 * Return the number of NUMA nodes available.
 */
int nrNodes();

/*
 * Bind this thread to run on a specific NUMA node.
 */
void setNodeAffinity(unsigned cpuNr);

#endif
