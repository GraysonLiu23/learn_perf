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
  printf("Success creating event\n");
  return fd;
}

int main() {
  int fd;
  uint64_t val;
  struct perf_event_attr pe;

  // Configure the event to count
  // 设置 perf_event_attr 结构体
  memset(&pe, 0, sizeof(struct perf_event_attr));
  pe.type = PERF_TYPE_HARDWARE;    // 硬件泛化性能事件 generalized hardware event
  pe.size = sizeof(struct perf_event_attr);    // 固定写法
  pe.config = PERF_COUNT_HW_CPU_CYCLES;    // 如果是 PERF_TYPE_HARDWARE，config 设为某一个 PERF_COUNT_HW
  pe.disabled = 1;    // 计数器开始时是被禁用的，之后可以通过 ioctl 启用
  pe.exclude_kernel = 1; // Do not measure instructions executed in the kernel
  pe.exclude_hv = 1;     // Do not measure instructions executed in a hypervisor

  // Create the event
  fd = perf_event_open(&pe, 0, -1, -1, 0);
  // 参数1 perf_event_attr 结构体
  // 参数2 pid and 参数3 cpu pid==0 and cpu==-1 仅测量本进程，可以在任意 CPU 上
  // 参数4 group_fd group_fd==-1 是group leader，应当最先被创建
  // 参数5 flags 不加 flag

  // Reset counters and start counting
  ioctl(fd, PERF_EVENT_IOC_RESET, 0);
  printf("counter reset.\n");
  ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
  printf("measured function start.\n");
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