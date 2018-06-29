# COBALT2 Compliance Tests

For compliance for COBALT2, the following tests are provided. These tests measure several
key performance figures at the application level. For compliance, it is sufficient to
reach the described treshholds, not the theoretical maxima.

The primary purpose of these tests is to give an assurance towards fitness for purpose,
that is, the absence of avoidable performance bottlenecks in unexpected setups. Examples
include the use of performance-degrading Riser cards and inefficient PCIe cards and
infrastructure.

Each test is explained in a section further down in this document on how to run it and
how to verify compliance.

* **mem-test:** Tests memory bandwidth (DRAM) performance.
* **gpu-copy:** Tests PCIe bandwidth performance towards NVIDIA GPUs.
* **eth-test:** Tests UDP bandwidth of a single 10GbE port.
* **ib-bw:** Measures the maximum InfiniBand transfer speed.

Some of these tests require compilation of provided C code (see "Building the tests" below).
The remaining tests are performed using common open source tools.

# System requirements

A single system is needed in the offered configuration, with Linux (and drivers) installed.
Some basic HPC tuning of the OS could be required to show the hardware capabilities and thus
to reach compliance. Connections between all its InfiniBand ports must be provided.

Furthermore, an additional system with sufficient capacity and connectivity to send 9 Gbit/s
to any 10GbE port on the offered system is required.

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
as part of the INCLUDES and LFLAGS directives, for the headers (`-I<path>`) and
libraries (`-L<path>`), respectively.
3. Build the tests:
```
    make
```

# Tips to increase performance (if necessary):

* Schedule the tests with real-time priority:
```
    chrt -f 1 <command>
```

# mem-test: Test DRAM performance

This test emulates all memory read/writes/copies/transposes needed by the COBALT
application on a single production node. It distributes these operations over the
available NUMA domains.

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

This test ensures that no significant PCIe bottlenecks exist between the CPUs
and the GPUs.

This test copies large amounts of data in parallel to and from all available
NVIDIA GPUs, and measures the resulting total transfer speed. It thus puts
load on all NUMA domains in which a GPU is present.

## To run:

    ./gpu-copy

## Example output:

    [...]
    ----- Test results -----
    Test version:      1.0
    Total write speed: 188.32 Gbit/s
    Total read speed:  198.98 Gbit/s
    
Note that the theoretical maximum unidirectional bandwidth is 252 Gbit/s
for 2 GPUs, if both are connected through dedicated PCI 3.0 x16 links.

## Compliance:

Both total read and write speeds must be >=180 Gbit/s.

# eth-test: Test 10GbE UDP reception

This test streams 9 Gbit/s of UDP data to a single 10GbE port. Two machines
are needed for this test: one to send and one to receive the data. The
receiving machine is the machine tested.

The sending machine needs to be sufficiently powerful to send 9 Gbit/s to
the receiving 10GbE port, using one or more NICs. When in doubt, this
can be verified with common network monitoring tools.

**Jumbo frames** must be enabled for the participating NICs.

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
    
Note that the theoretical maximum bandwidth is 9.9 Gbit/s for a 10GbE port.

## Compliance:

This test must be repeated for each 10GbE interface in the test machine.
For each interface, the total speed must be >=9.00 Gbit/s, and the average
loss 0.000%.

## Tips to increase performance (if necessary):

* Enable Jumbo frames, f.e. by:
```
    ip link set ethX mtu 9000
```
* Increase the Linux network buffers, f.e. by:
```
    sysctl -w net.core.rmem_max=16777216
    sysctl -w net.core.rmem_default=16777216
    sysctl -w net.core.wmem_max=16777216
    sysctl -w net.core.wmem_default=16777216
    sysctl -w net.core.netdev_max_backlog=250000
    sysctl -w net.ipv4.udp_mem='262144 327680 393216'
```
* Constrict the test to the CPU in the NUMA node (X=0 or 1) hosting
  the tested 10GbE card:
```
    numactl --cpubind=X --membind=X <command>
```

# ib-bw: Test InfiniBand bandwidth

This test measures the InfiniBand throughput from one device to another,
using RDMA transport. Two distinct and connected InfiniBand devices are
needed for this test.

Note that with two connected InfiniBand cards in one machine, that machine
should be capable of acting as both the sender and the receiver.

## To run:

First, on the receiving machine, start:

    ib_write_bw -d <ibdevice>

Then, on the test (sending) machine, start:

    ib_write_bw -d <other ibdevice> --report_gbits localhost

Where `<ibdevice>` is a specific InfiniBand device and `<other ibdevice>`
is another one. Their names are listed in `/sys/class/infiniband/`,
typically `mlx4_0` and `mlx4_1`. The sending and receiving devices must be
on physically distinct cards.

## Example output (on sending machine):

    [...]
    ---------------------------------------------------------------------------------------
    #bytes     #iterations    BW peak[Gb/sec]    BW average[Gb/sec]   MsgRate[Mpps]
    65536      5000             51.05              51.04              0.097356
    ---------------------------------------------------------------------------------------
    
Note that the theoretical maximum bandwidth for an FDR InfiniBand card is 54.5 Gbit/s.

## Compliance:

This test must be repeated for sending from each InfiniBand port. For each device,
the BW average must be >=50.00 Gb/sec.

