#ifndef __ENDIAN_H__
#define __ENDIAN_H__

#include "types.h"

/*
 * AArch64 is little-endian. Define identity casts for on-disk little-endian
 * values so the same code reads naturally whether or not swap is needed.
 */

static inline uint16_t le16_to_cpu(__le16 x)
{
	return (uint16_t)x;
}

static inline uint32_t le32_to_cpu(__le32 x)
{
	return (uint32_t)x;
}

static inline uint64_t le64_to_cpu(__le64 x)
{
	return (uint64_t)x;
}

static inline __le16 cpu_to_le16(uint16_t x)
{
	return (__le16)x;
}

static inline __le32 cpu_to_le32(uint32_t x)
{
	return (__le32)x;
}

static inline __le64 cpu_to_le64(uint64_t x)
{
	return (__le64)x;
}

static inline uint16_t be16_to_cpu(uint16_t x)
{
	return ((x & 0xff) << 8) | ((x >> 8) & 0xff);
}

static inline uint32_t be32_to_cpu(uint32_t x)
{
	return ((uint32_t)be16_to_cpu(x & 0xffff) << 16) |
	       be16_to_cpu(x >> 16);
}

static inline uint64_t be64_to_cpu(uint64_t x)
{
	return ((uint64_t)be32_to_cpu(x & 0xffffffffULL) << 32) |
	       be32_to_cpu(x >> 32);
}

#endif /* __ENDIAN_H__ */
