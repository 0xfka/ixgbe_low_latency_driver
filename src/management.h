#ifndef MANAGEMENT_H
#define MANAGEMENT_H

/* Single producer - single consumer lockless ring buffer
 * Used to share info at runtime without using print bloat.
 * After consumer gets the info, this info can be used to anything
 * without affecting the producer core.
 */
#include <assert.h>
#include <emmintrin.h>
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "base.h"
/* 2 MB hugepage. */
#define MGMT_SPSC_BUFFER_NUMBER 16384
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
static inline bool spsc_push(struct spsc_ring_hugepage_layout* ring,
                             struct management* data) {
  if (unlikely(((ring->tail + 1) & (MGMT_SPSC_BUFFER_NUMBER - 1)) ==
               ring->head)) {
    return false;
  }
  ring->buffer[ring->tail] = *data;
  wmb();
  ring->tail = (ring->tail + 1) & (MGMT_SPSC_BUFFER_NUMBER - 1);
  return true;
}
static inline void spsc_poll(struct spsc_ring_hugepage_layout* ring) {
  while (1) {
    while (ring->head == ring->tail) {
      _mm_pause();
    }
    struct management data = ring->buffer[ring->head];
    rmb();
    LOG("data is %lu\n", data.test_data);
    usleep(1000);
    ring->head = (ring->head + 1) & (MGMT_SPSC_BUFFER_NUMBER - 1);
  }
}
/* management_entrypoint() - Handles SPSC lockless ring buffer between data path
 * and management.
 */
static inline void* management_entrypoint(void* arg) {
  struct spsc_ring_hugepage_layout* spsc =
      (struct spsc_ring_hugepage_layout*)arg;
  LOG("Thread sees ring at: %p\n", (void*)spsc);
  /* Calculating memory addresses of every buffer on SPSC ring buffer.
   * Prevents overhead on runtime with compile-time calculations. */
  spsc_poll(spsc);
  return 0;
}
#endif