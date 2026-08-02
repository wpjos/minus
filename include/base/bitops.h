#ifndef __BITOPS_H__
#define __BITOPS_H__

#include "types.h"

/*
 * Minimal bitmap operations. All operate on arrays of unsigned long.
 * Since AArch64 uses 64-bit longs, each word covers 64 bits.
 */

static inline int test_bit(int nr, const volatile unsigned long *addr)
{
	return 1UL & (addr[nr >> 6] >> (nr & 63));
}

static inline void set_bit(int nr, volatile unsigned long *addr)
{
	addr[nr >> 6] |= 1UL << (nr & 63);
}

static inline void clear_bit(int nr, volatile unsigned long *addr)
{
	addr[nr >> 6] &= ~(1UL << (nr & 63));
}

/*
 * Find first zero bit in a bitmap of @size bits.
 * Returns the bit index, or @size if all bits are set.
 */
int find_first_zero_bit(const unsigned long *addr, unsigned int size);

static inline unsigned long div_round_up(unsigned long n, unsigned long d)
{
	return (n + d - 1) / d;
}

static inline unsigned long round_up(unsigned long x, unsigned long y)
{
	return div_round_up(x, y) * y;
}

static inline unsigned long round_down(unsigned long x, unsigned long y)
{
	return (x / y) * y;
}

#define BITS_PER_LONG		(sizeof(unsigned long) * BITS_PER_BYTE)
#define BITS_TO_LONGS(nr)	div_round_up(nr, BITS_PER_LONG)

#endif /* __BITOPS_H__ */
