#ifndef BASE_H
#define BASE_H
#include <stdint.h>
#include <stdbool.h>

#define CORE_DATAPATH 0
#define CORE_MANAGEMENT 1
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define mb() asm volatile("mfence" ::: "memory")
#define rmb() asm volatile("lfence" ::: "memory")
#define wmb() asm volatile("sfence" ::: "memory")
#define barrier() asm volatile("" ::: "memory")

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif