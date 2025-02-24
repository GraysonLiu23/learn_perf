#include <inttypes.h>
#include <linux/perf_event.h> /* Definition of PERF_* constants */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h> /* Definition of SYS_* constants */
#include <unistd.h>
#define TOTAL_EVENTS 6

// The function to counting through (called in main)
void code_to_measure() {
    int sum = 0;
    for (int i = 0; i < 2000000000; ++i) {
        sum += 1;
    }
}

// Executes perf_event_open syscall and makes sure it is successful or exit
static long perf_event_open(struct perf_event_attr *hw_event, 
                            pid_t pid,
                            int cpu, 
                            int group_fd, 
                            unsigned long flags) {
    int fd;
    fd = syscall(SYS_perf_event_open, hw_event, pid, cpu, group_fd, flags);
    if (fd == -1) {
        fprintf(stderr, "Error creating event");
        exit(EXIT_FAILURE);
    }
    printf("Success creating event\n");
    return fd;
}

// Helper function to setup a perf event structure (perf_event_attr; 
// see man perf_open_event)
void configure_event(struct perf_event_attr *pe, 
                     uint32_t type,
                     uint64_t config,
                     int is_group_leader
                     ) {
    memset(pe, 0, sizeof(struct perf_event_attr));
    pe->type = type;
    pe->size = sizeof(struct perf_event_attr);
    pe->config = config;
    if (is_group_leader)
      pe->read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
    // PERF_FORMAT_GROUP​  Allows all counter values in an event group to be read with one read.
    // ​PERF_FORMAT_ID​  Adds a 64-bit unique value that corresponds to the event group.
    pe->disabled = is_group_leader;
    // 	When creating an event group, typically the group leader is initialized with disabled set to 1 and 
    // any child events are initialized with disabled set to 0. Despite disabled being 0, 
    // the child events will not start until the group leader is enabled.
    pe->exclude_kernel = 1;
    pe->exclude_hv = 1;
}

// Format of event data to read
// Note: This format changes depending on perf_event_attr.read_format
// See `man perf_event_open` to understand how this structure can be different
// depending on event config This read_format structure corresponds to when
// PERF_FORMAT_GROUP & PERF_FORMAT_ID are set
struct read_format {
    uint64_t nr;
    struct {
        uint64_t value;
        uint64_t id;
    } values[TOTAL_EVENTS];
};

int main() {
    long fd[TOTAL_EVENTS]; // fd[0] will be the group leader file descriptor
    uint64_t id[TOTAL_EVENTS]; // event ids for file descriptors
    uint64_t pe_val[TOTAL_EVENTS]; // Counter value array corresponding to fd/id array.
    
    struct perf_event_attr pe[TOTAL_EVENTS]; // Configuration structure for perf
                                             // events (see man perf_event_open)
    struct read_format counter_results;

    // Configure the group of PMUs to count
    configure_event(&pe[0], PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES, 1);
    configure_event(&pe[1], PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS, 0);
    configure_event(&pe[2], PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_FRONTEND, 0);
    configure_event(&pe[3], PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_BACKEND, 0);
    configure_event(&pe[4], PERF_TYPE_RAW, 0x70, 0); // Count of speculative loads (see Arm PMU docs)
    configure_event(&pe[5], PERF_TYPE_RAW, 0x71, 0); // Count of speculative stores (see Arm PMU docs)

  // Create event group leader
  fd[0] = perf_event_open(&pe[0], 0, -1, -1, 0);
  ioctl(fd[0], PERF_EVENT_IOC_ID, &id[0]);    // 获取已创建的性能事件 perf_event 对象的唯一标识符
  // Let's create the rest of the events while using fd[0] as the group leader
  for (int i = 1; i < TOTAL_EVENTS; i++) {
    fd[i] = perf_event_open(&pe[i], 0, -1, fd[0], 0);
    ioctl(fd[i], PERF_EVENT_IOC_ID, &id[i]);
  }
  printf("Success obtain perf_event ids.\n");

  // Reset counters and start counting; Since fd[0] is leader, this resets and
  // enables all counters PERF_IOC_FLAG_GROUP required for the ioctl to act on
  // the group of file descriptors
  ioctl(fd[0], PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
  printf("counter reset.\n");
  ioctl(fd[0], PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);

  printf("measured function start.\n");

  // Example code to count through
  code_to_measure();

  printf("measured function end.\n");

  // Stop all counters
  ioctl(fd[0], PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
  printf("counter disabled.\n");

  // Read the group of counters and print result
  read(fd[0], &counter_results, sizeof(struct read_format));
  printf("Num events captured: %" PRIu64 "\n", counter_results.nr);
  for (uint64_t i = 0; i < counter_results.nr; i++) {
    for (int j = 0; j < TOTAL_EVENTS; j++) {
      if (counter_results.values[i].id == id[j]) {
        pe_val[i] = counter_results.values[i].value;
      }
    }
  }
  printf("CPU cycles: %" PRIu64 "\n", pe_val[0]);
  printf("Instructions retired: %" PRIu64 "\n", pe_val[1]);
  printf("Frontend stall cycles: %" PRIu64 "\n", pe_val[2]);
  printf("Backend stall cycles: %" PRIu64 "\n", pe_val[3]);
  printf("Loads executed speculatively: %" PRIu64 "\n", pe_val[4]);
  printf("Stores executed speculatively: %" PRIu64 "\n", pe_val[5]);

  // Close counter file descriptors
  for (int i = 0; i < TOTAL_EVENTS; i++) {
    close(fd[i]);
  }

  return 0;
}