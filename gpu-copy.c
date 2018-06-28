#include <cuda.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

#include "common.h"

/* report and bail if the exit code from a CUDA function ("result") is not succesful */
void checkCuCall(const char *funcname, CUresult result) {
  if (result != CUDA_SUCCESS) {
    printf("Call %s failed with error code %d (see cuda.h)\n", funcname, result);
    exit(EXIT_FAILURE);
  }
}

#define ITERATIONS 10

struct report {
  double write_speed_gbps;
  double read_speed_gbps;
};

struct report test_device( int deviceNr ) {
  struct report result;

  CUdevice device;
  size_t globalMemSize;

  CUcontext context;
  size_t bufferSize;
  void *hostMem;
  CUdeviceptr devMem;

  struct timer t;
  int i;

  /* initialise */
  checkCuCall("cuDeviceGet",         cuDeviceGet(&device, deviceNr));
  checkCuCall("cuDeviceTotalMem",    cuDeviceTotalMem(&globalMemSize, device));
  printf("[device %d] Total memory size: %.2f GByte RAM\n", deviceNr, globalMemSize/GBYTE);

  checkCuCall("cuCtxCreate",         cuCtxCreate(&context, 0, device));

  printf("[device %d] Allocating memory...\n", deviceNr);

  bufferSize = globalMemSize/1.1; /* allocate just about all device memory */
  checkCuCall("cuMemAlloc",          cuMemAlloc(&devMem, bufferSize));
  checkCuCall("cuMemHostAlloc",      cuMemHostAlloc(&hostMem, bufferSize, 0));
  memset(hostMem, 1, bufferSize);

  printf("[device %d] Waiting for other threads...\n", deviceNr);

  /* GPU write test */
#pragma omp barrier
  printf("[device %d] Writing %.2f GByte to GPU, %d times...\n", deviceNr, bufferSize/GBYTE, ITERATIONS);
  start(&t);
  for( i = 0; i < ITERATIONS; i++ ) {
    checkCuCall("cuMemcpyHtoD",        cuMemcpyHtoD(devMem, hostMem, bufferSize));
  }
  stop(&t);

  result.write_speed_gbps = bufferSize*ITERATIONS/GBPS/duration(t);
  printf("[device %d] Writing took %.2fs (average over %d runs), resulting in %.2f Gbit/s\n",
    deviceNr,
    duration(t)/ITERATIONS,
    ITERATIONS,
    result.write_speed_gbps);

  /* GPU read test */
#pragma omp barrier
  printf("[device %d] Reading %.2f GByte from GPU, %d times...\n", deviceNr, bufferSize/GBYTE, ITERATIONS);
  start(&t);
  for( i = 0; i < ITERATIONS; i++ ) {
    checkCuCall("cuMemcpyDtoH",   cuMemcpyDtoH(hostMem, devMem, bufferSize));
  }
  stop(&t);

  result.read_speed_gbps = bufferSize*ITERATIONS/GBPS/duration(t);
  printf("[device %d] Reading took %.2fs (average over %d runs), resulting in %.2f Gbit/s\n",
    deviceNr,
    duration(t)/ITERATIONS,
    ITERATIONS,
    result.read_speed_gbps);

  /* Teardown */
#pragma omp barrier
  printf("[device %d] Cleaning up...\n", deviceNr);

  checkCuCall("cuMemFree",           cuMemFree(devMem));
  checkCuCall("cuMemFreeHost",       cuMemFreeHost(hostMem));
  checkCuCall("cuCtxDestroy",        cuCtxDestroy(context));

  return result;
}

int main() {
  int nrDevices;
  int deviceNr;
  int i;

  /* initialise */
  printf("Initialising GPU...\n");

  checkCuCall("cuInit",              cuInit(0));
  checkCuCall("cuDeviceGetCount",    cuDeviceGetCount(&nrDevices));

  omp_set_num_threads(nrDevices);

  /* test all devices in parallel */
  struct report reports[nrDevices];

#pragma omp parallel for num_threads(nrDevices)
  for( deviceNr = 0; deviceNr < nrDevices; deviceNr++ ) {
    reports[deviceNr] = test_device(deviceNr);
  }

  /* calculate and show summary */
  printf(" ----- Test results -----\n");
  struct report totals = { 0.0, 0.0 };
  const int nr_reports = sizeof reports / sizeof reports[0];
  for ( i = 0; i < nr_reports; i++ ) {
    totals.write_speed_gbps += reports[i].write_speed_gbps; /* sum */
    totals.read_speed_gbps  += reports[i].read_speed_gbps; /* sum */
  }

  printf("Test version:      %s\n", VERSION);
  printf("Total write speed: %.2f Gbit/s\n", totals.write_speed_gbps);
  printf("Total read speed:  %.2f Gbit/s\n", totals.read_speed_gbps);

  return EXIT_SUCCESS;
}
