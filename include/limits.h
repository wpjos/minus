#ifndef __LIMITS_H__
#define __LIMITS_H__

#define INT_MAX     (((1ULL << 31) - 1ULL))
#define UINT_MAX    ((1ULL << 32) - 1ULL)

// 如果需要 8/16/32/64 位限制
#define INT8_MAX    ((1 << 7) - 1)
#define INT16_MAX   ((1 << 15) - 1)
#define INT32_MAX   (((1ULL << 31) - 1ULL))
#define INT64_MAX   (((1ULL << 63) - 1ULL))

#define UINT8_MAX   ((1U << 8) - 1U)
#define UINT16_MAX  ((1U << 16) - 1U)
#define UINT32_MAX  ((1ULL << 32) - 1ULL)
#define UINT64_MAX  ((1ULL << 64) - 1ULL)

#endif
