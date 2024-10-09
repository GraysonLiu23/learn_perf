#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

// The function we are interested in counting through (see main)
void code_to_measure(){
  int sum = 0;
  for(int i = 0; i < 1000000000; ++i){
    sum += 1;
  }
}

int main() {
  uint64_t syscnt_freq = 0;
  uint64_t syscnt_before, syscnt_after;

  // Get frequency of the system counter
  asm volatile("mrs %0, cntfrq_el0" : "=r" (syscnt_freq));

  // Read system counter
  asm volatile("mrs %0, cntvct_el0" : "=r" (syscnt_before));

  // This is what we are counting through
  code_to_measure();

  // Read system counter
  asm volatile("mrs %0, cntvct_el0" : "=r" (syscnt_after));

  // Calculate results and print to stdout
  uint64_t syscnt_ticks = syscnt_after - syscnt_before;
  printf("System counter ticks: %"PRIu64"\n", syscnt_ticks);
  printf("System counter freq (Hz): %"PRIu64"\n", syscnt_freq);

  return 0;
}
