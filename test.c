#include <linux/perf_event.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

//目前perf_event_open在glibc中没有封装，需要手工封装一下
int perf_event_open(struct perf_event_attr *attr, pid_t pid, int cpu,
                    int group_fd, unsigned long flags) {
  return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

//每次read()得到的结构体
struct read_format {
  //计数器数量（为2）
  uint64_t nr;
  //两个计数器的值
  uint64_t values[2];
};

int main() {
  struct perf_event_attr attr;
  memset(&attr, 0, sizeof(struct perf_event_attr));
  attr.size = sizeof(struct perf_event_attr);
  //监测硬件
  attr.type = PERF_TYPE_HARDWARE;
  //监测指令数
  attr.config = PERF_COUNT_HW_INSTRUCTIONS;
  //初始状态为禁用
  attr.disabled = 1;
  //每次读取一个组
  attr.read_format = PERF_FORMAT_GROUP;
  //创建perf文件描述符，其中pid=0,cpu=-1表示监测当前进程，不论运行在那个cpu上
  int fd = perf_event_open(&attr, 0, -1, -1, 0);
  if (fd < 0) {
    perror("Cannot open perf fd!");
    return 1;
  }
  //接下来创建第二个计数器
  memset(&attr, 0, sizeof(struct perf_event_attr));
  attr.size = sizeof(struct perf_event_attr);
  //监测
  attr.type = PERF_TYPE_HARDWARE;
  //监测时钟周期数
  attr.config = PERF_COUNT_HW_CPU_CYCLES;
  //初始状态为禁用
  attr.disabled = 1;
  //创建perf文件描述符
  int fd2 = perf_event_open(&attr, 0, -1, fd, 0);
  if (fd2 < 0) {
    perror("Cannot open perf fd2!");
    return 1;
  }
  //启用（开始计数），注意PERF_IOC_FLAG_GROUP标志
  ioctl(fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
  while (1) {
    struct read_format aread;
    //读取最新的计数值，每次读取一个结构体
    read(fd, &aread, sizeof(struct read_format));
    printf("instructions=%ld,cycles=%ld\n", aread.values[0], aread.values[1]);
    sleep(1);
  }
}