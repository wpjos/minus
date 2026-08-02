#include "bitops.h"

int find_first_zero_bit(const unsigned long *addr, unsigned int size)
{
	unsigned int i;
	unsigned int words = (size + BITS_PER_LONG - 1) / BITS_PER_LONG;

	for (i = 0; i < words; i++) {
		unsigned long word = addr[i];
		if (word == ~0UL)
			continue;

		unsigned int b;
		for (b = 0; b < BITS_PER_LONG; b++) {
			unsigned int bit = i * BITS_PER_LONG + b;
			if (bit >= size)
				return size;
			if (!(word & (1UL << b)))
				return bit;
		}
	}
	return size;
}
