CFLAGS   = -Wall -O3 -fopenmp
#CFLAGS   = -Wall -g3 -fopenmp
INCLUDES = -I/usr/local/cuda/include
LFLAGS   = -lnuma -lpthread
TARGETS  = gpu-copy eth-test-send eth-test-receive mem-test


.PHONY:	  all clean

all:      $(TARGETS)

clean:
	  $(RM) *.o $(TARGETS)

.c.o:
	  $(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

gpu-copy: common.o gpu-copy.o
	  $(CC) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LFLAGS) -lcuda

eth-test-receive: common.o eth-test-receive.o
	  $(CC) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LFLAGS)

eth-test-send: common.o eth-test-send.o
	  $(CC) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LFLAGS)

mem-test: common.o mem-test.o
	  $(CC) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LFLAGS)
