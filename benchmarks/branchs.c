#include <hdr/hdr_histogram.h>
#include <stdlib.h>
#include <x86intrin.h>

#include "../src/base.h"
#define ITERATIONS 100000000
/* See related commit message for why this test is made and what's decided. */
int main() {
  u64 start_cycles, end_cycles;
  struct hdr_histogram* branched;
  hdr_init(1, 10000000, 3, &branched);
  u32 a, b;
  u32 c = 0;
  u32 d = 5;
  u32 e = 10;
  for (u32 i = 0; i <= ITERATIONS; i++) {
    a = rand();
    b = rand();
    rmb();
    start_cycles = __rdtsc();
    rmb();
    if (a > b) {
      c = d;
    } else {
      c = e;
    }
    rmb();
    asm volatile("" : : "g"(c) : "memory");
    end_cycles = __rdtsc();
    rmb();
    hdr_record_value(branched, end_cycles - start_cycles);
  }
  u32 branched_p99_9 = hdr_value_at_percentile(branched, 99.9);
  hdr_close(branched);
  struct hdr_histogram* branchless;
  hdr_init(1, 10000000, 3, &branchless);
  u32 mask;
  for (u32 i = 0; i <= ITERATIONS; i++) {
    a = rand();
    b = rand();
    rmb();
    start_cycles = __rdtsc();
    rmb();
    mask = -(a > b);
    c = (mask & d) | (~mask & e);
    rmb();
    asm volatile("" : : "g"(c) : "memory");
    end_cycles = __rdtsc();
    rmb();
    hdr_record_value(branchless, end_cycles - start_cycles);
  }
  u32 branchless_p99_9 = hdr_value_at_percentile(branchless, 99.9);
  hdr_close(branchless);
  printf("branchless : %u branched : %u\n", branchless_p99_9, branched_p99_9);
  return 0;
}