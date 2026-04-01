#ifndef MANAGEMENT_H
#define MANAGEMENT_H

/* Single producer - single consumer lockless ring buffer
 * Used to share info at runtime without using print bloat.
 * After consumer gets the info, this info can be used to anything
 * without affecting the producer core.
 */
#include <assert.h>
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "base.h"
/* 2 MB hugepage. */
#define MGMT_SPSC_BUFFER_NUMBER 32766
/* WIP, data will be added */
struct __attribute__((aligned(64))) management {
  u64 test_data;
};
struct spsc_ring_hugepage_layout {
  u64 tail __attribute__((aligned(64)));
  u64 head __attribute__((aligned(64)));
  struct management buffer[MGMT_SPSC_BUFFER_NUMBER]
      __attribute__((aligned(64)));
  u8 reserved[2097152 - 128 -
              (sizeof(struct management) * MGMT_SPSC_BUFFER_NUMBER)];
};
static_assert((sizeof(struct spsc_ring_hugepage_layout) == 2097152),
              "Hugepage layout is unequal to 2 MB's\n");

/* management_entrypoint() - Handles SPSC lockless ring buffer between data path
 * and management.
 */
static inline void* management_entrypoint() {
  if (unlikely(sizeof(struct spsc_ring_hugepage_layout) != 2097152)) {
    return (void*)EINVAL;
  }
  void* hugepage = mmap(NULL, 2 * 1024 * 1024, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
  if (unlikely(hugepage == MAP_FAILED)) {
    int save_errno = errno;
    /* Added (long) for preventing compiler from yelling about it.
     * Just use gdb.*/
    return (void*)(long)errno;
  };
  /* mmap may act lazy when allocating it. */
  memset(hugepage, 0, 2 * 1024 * 1024);
  /* Calculating memory addresses of every buffer on SPSC ring buffer.
   * Prevents overhead on runtime with compile-time calculations. */
  struct spsc_ring_hugepage_layout* layout =
      (struct spsc_ring_hugepage_layout*)(hugepage);
  return 0;
}
#endif