#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <linux/perf_event.h> /* Definition of PERF_* constants and struct perf_event_attr */
#include <sys/ioctl.h>   /* ioctl function */
#include <sys/syscall.h> /* Definition of SYS_* constants */
#include <sys/types.h>
#include <unistd.h> /* syscall function */

#include <inttypes.h>

#define MAX_DEVICES 100
#define MAX_PATH_LENGTH 256

// Executes perf_event_open syscall and makes sure it is successful or exit
static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                            int cpu, int group_fd, unsigned long flags) {
    int fd;
    fd = syscall(SYS_perf_event_open, hw_event, pid, cpu, group_fd, flags);
    if (fd == -1) {
        perror("Error creating event");
        exit(EXIT_FAILURE);
    }
    printf("Success creating event\n");
    return fd;
}

// Helper function to setup a perf event structure perf_event_attr
void configure_dmc620_event(struct perf_event_attr *pe, uint32_t type,
                            uint event, uint clkdiv2, uint mask, uint match) {
    memset(pe, 0, sizeof(struct perf_event_attr));

    pe->type = type;
    pe->size = sizeof(struct perf_event_attr);

    pe->config = mask;   // mask -> config:0-44
    pe->config1 = match; // match -> config1:0-44
    pe->config2 =
        (clkdiv2 << 9) |
        (event << 3); // clkdiv2 -> config2:9, event -> config2:3-8, incr
                      // -> config2:1-2, invert -> config2:0
    pe->disabled = 1;
    pe->inherit = 1;
}

int main() {
    // Search arm_dmc620 devices
    char *devices[MAX_DEVICES];
    int types[MAX_DEVICES];
    int device_count = 0;

    const char *base_dir = "/sys/bus/event_source/devices/";
    DIR *dir = opendir(base_dir);
    if (!dir) {
        perror("opendir");
        return EXIT_FAILURE;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "arm_dmc620", strlen("arm_dmc620")) == 0) {
            devices[device_count] = strdup(entry->d_name);
            if (!devices[device_count]) {
                perror("strdup");
                closedir(dir);
                return EXIT_FAILURE;
            }
            device_count++;
            if (device_count >= MAX_DEVICES) {
                fprintf(stderr, "Too many devices found\n");
                break;
            }
        }
    }
    closedir(dir);

    for (int i = 0; i < device_count; i++) {
        char type_path[MAX_PATH_LENGTH];
        snprintf(type_path, sizeof(type_path), "%s%s/type", base_dir,
                 devices[i]);

        FILE *type_file = fopen(type_path, "r");
        if (!type_file) {
            perror("fopen");
            return EXIT_FAILURE;
        }

        char type_str[32];
        if (fgets(type_str, sizeof(type_str), type_file) == NULL) {
            perror("fgets");
            fclose(type_file);
            return EXIT_FAILURE;
        }
        fclose(type_file);

        types[i] = atoi(type_str);
    }

    struct perf_event_attr *pe =
        malloc(2 * device_count * sizeof(struct perf_event_attr));
    if (!pe) {
        perror("malloc");
        return EXIT_FAILURE;
    }
    long *fd = malloc(2 * device_count * sizeof(long));
    uint64_t *pe_val = malloc(2 * device_count * sizeof(uint64_t));

    // Configure the group of PMUs to count
    for (int i = 0; i < device_count; i++) {
        configure_dmc620_event(&pe[2 * i], types[i], 0x12, 0x1, 0x1, 0x0);
        configure_dmc620_event(&pe[2 * i + 1], types[i], 0x12, 0x1, 0x1, 0x1);
        fd[2 * i] = perf_event_open(&pe[2 * i], -1, 0, -1, 0);
        fd[2 * i + 1] = perf_event_open(&pe[2 * i + 1], -1, 0, -1, 0);
        // pid=-1 and cpu=0 -> system-wide -C 0
    }

    // Reset counters
    for (int i = 0; i < 2 * device_count; i++)
        ioctl(fd[i], PERF_EVENT_IOC_RESET, 0);

    // Start counting
    for (int i = 0; i < 2 * device_count; i++)
        ioctl(fd[i], PERF_EVENT_IOC_ENABLE, 0);

    sleep(5);

    // Stop all counters
    for (int i = 0; i < 2 * device_count; i++)
        ioctl(fd[i], PERF_EVENT_IOC_DISABLE, 0);

    // Read and print result
    uint64_t total_data = 0;
    for (int i = 0; i < device_count; i++) {
        read(fd[2 * i], &pe_val[2 * i], sizeof(uint64_t));
        total_data += pe_val[2 * i];
        read(fd[2 * i + 1], &pe_val[2 * i + 1], sizeof(uint64_t));
        total_data += pe_val[2 * i + 1];
        printf("Device: %s, Type: %d, Read Count: %" PRIu64
               ", Write Count: %" PRIu64 "\n",
               devices[i], types[i], pe_val[2 * i], pe_val[2 * i + 1]);
    }
    printf("Total data: %" PRIu64 "Byte \n", total_data * 64);

    // Close counter file descriptors
    for (int i = 0; i < device_count; i++) {
        close(fd[2 * i]);
        close(fd[2 * i + 1]);
    }

    for (int i = 0; i < device_count; i++) {
        free(devices[i]);
    }

    free(pe);
    free(fd);
    free(pe_val);

    return EXIT_SUCCESS;
}