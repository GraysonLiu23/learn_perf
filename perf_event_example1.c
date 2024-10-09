#include <inttypes.h>
#include <linux/perf_event.h> /* Definition of PERF_* constants */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h> /* Definition of SYS_* constants */
#include <unistd.h>

// The function to counting through (called in main)
void code_to_measure() {
  int sum = 0;
  for (int i = 0; i < 1000000000; ++i) {
    sum += 1;
  }
}

// Executes perf_event_open syscall and makes sure it is successful or exit
static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                            int cpu, int group_fd, unsigned long flags) {
  int fd;
  fd = syscall(SYS_perf_event_open, hw_event, pid, cpu, group_fd, flags);
  if (fd == -1) {
    fprintf(stderr, "Error creating event");
    exit(EXIT_FAILURE);
  }

  return fd;
}

int main() {
  int fd;
  uint64_t val;
  struct perf_event_attr pe;

  // Configure the event to count
  memset(&pe, 0, sizeof(struct perf_event_attr));
  pe.type = PERF_TYPE_HARDWARE;
  pe.size = sizeof(struct perf_event_attr);
  pe.config = PERF_COUNT_HW_CPU_CYCLES;
  pe.disabled = 1;
  pe.exclude_kernel = 1; // Do not measure instructions executed in the kernel
  pe.exclude_hv = 1;     // Do not measure instructions executed in a hypervisor

  // Create the event
  fd = perf_event_open(&pe, 0, -1, -1, 0);

  // Reset counters and start counting
  ioctl(fd, PERF_EVENT_IOC_RESET, 0);
  ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
  // Example code to count through
  code_to_measure();
  // Stop counting
  ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);

  // Read and print result
  read(fd, &val, sizeof(val));
  printf("Instructions retired: %" PRIu64 "\n", val);

  // Clean up file descriptor
  close(fd);

  return 0;
}