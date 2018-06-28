# COBALT2 Compliance Tests

For compliance for COBALT2, the following tests are provided. These tests measure several
key performance figures at the application level. Any bottlenecks imposed by individual
hardware components or by the OS are thus revealed.

Each test is explained in a section further down in this document on how to run it and
how to verify compliance. The tests require high performance, but not maximum theoretical
performance, for compliance.

* **mem-test:** Tests memory bandwidth (DRAM) performance.
* **gpu-copy:** Tests PCIe bandwidth performance towards NVIDIA GPUs.
* **eth-test:** Tests UDP bandwidth of a single 10GbE port.
* **ib-bw:** Measures the maximum InfiniBand transfer speed.

Some of these tests require compilation of provided C code (see "Building the tests" below).
The remaining tests are performed using common open source tools.

For a machine to be compliant, all tests must show compliance under
the following system settings:

* Set the CPU frequency scaling to *performance*, f.e. by:
```
    cpupower frequency-set -g performance
``` 
* Disable Intel Turbo Boost, f.e. by:
```
    echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo
```

# Building the tests

1. You need the following libraries and tools installed:
```
    libnuma-dev       (providing numa.h and libnuma.so)
    cuda              (providing cuda.h and libcuda.so)
    libc6-dev         (providing pthread.h and libpthread.so)
    perftest          (providing ib_write_bw)
```
2. Any non-standard paths to these libraries can be added to the Makefile
as part of the INCLUDES and LFLAGS directives, for the headers and
libraries, respectively.
3. Build the tests:
```
    make
```

# Tips to increase performance (if necessary):

* Schedule the tests with real-time priority:
```
    chrt -f 1 <command>
```
* For `eth-test`, schedule the test on a specific NUMA node (X=0 or 1):
```
    numactl --cpubind=X --membind=X <command>
```
* For `eth-test`, tune the Linux network and UDP buffers, f.e.:
```
    sysctl -w net.core.rmem_max=16777216
    sysctl -w net.core.rmem_default=16777216
    sysctl -w net.core.wmem_max=16777216
    sysctl -w net.core.wmem_default=16777216
    sysctl -w net.core.netdev_max_backlog=250000
    sysctl -w net.ipv4.udp_mem='262144 327680 393216'
```

# mem-test: Test DRAM performance

This test emulates all memory read/writes/copies/transposes needed by COBALT
on a single production node. It distributes these operations over the available
NUMA domains.

## To run:

    ./mem-test

## Example output:

    [...]
    ----- Test results -----
    Test version:    1.0
    Desired speed:   621.00 Gbit/s
    Measured speed:  540.12 Gbit/s (86.98% of desired)
    Average late:    40.548%

## Compliance:

The measured speed must be >=99.75% of the desired speed.

# gpu-copy: Test PCIe bandwidth to GPUs

This test copies large amounts of data in parallel to and from all available
NVIDIA GPUs, and measures the resulting total transfer speed.

## To run:

    ./gpu-copy

## Example output:

    [...]
    ----- Test results -----
    Test version:      1.0
    Total write speed: 188.32 Gbit/s
    Total read speed:  198.98 Gbit/s

## Compliance:

Both total read and write speeds must be >=180 Gbit/s.

# eth-test: Test 10GbE UDP reception

This test streams 9 Gbit/s of UDP data to a single 10GbE port. Two machines
are needed for this test, with a 10GbE connection between them: a sending
and a receiving machine. The receiving machine is the machine tested.

## To run:

First, on the test (receiving) machine, start:

    ./eth-test-receive -H <hostname>

Then, on the sending machine, start:

    ./eth-test-send -H <hostname>

Where `<hostname>` is the DNS name or IP address of the interface of the
receiving machine to test.

## Example output (on receiving machine):

    [...]
    ----- Test results -----
    Test version: 1.0
    Total speed:  8.80 Gbit/s
    Average loss: 2.271%

## Compliance:

This test must be repeated for each 10GbE interface in the test machine.
For each interface, the total speed must be >=9.00 Gbit/s, and the average
loss 0.000%.

# ib-bw: Test InfiniBand bandwidth

This test measures the InfiniBand throughput from one device to another,
using RDMA transport. Two distinct and connected InfiniBand devices are
needed for this test. The sending machine is the machine tested.

## To run:

First, on the receiving machine, start:

    ib_write_bw -d <ibdevice>

Then, on the test (sending) machine, start:

    ib_write_bw -d <ibdevice> --report_gbits <hostname>

Where `<hostname>` is the DNS name or IP address of the receiving machine,
and `<ibdevice>` is a specific InfiniBand device to use in the test
(f.e. `mlx4_0`, `mlx4_1`, see `/sys/class/infiniband`). The sending and
receiving interfaces must be distinct.

## Example output (on sending machine):

    [...]
    ---------------------------------------------------------------------------------------
    #bytes     #iterations    BW peak[Gb/sec]    BW average[Gb/sec]   MsgRate[Mpps]
    65536      5000             51.05              51.04              0.097356
    ---------------------------------------------------------------------------------------

## Compliance:

This test must be repeated for each InfiniBand device in the test machine.
For each interface, the BW average must be >50 Gb/sec.

