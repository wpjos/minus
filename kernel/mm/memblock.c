#include "mm.h"
#include "page.h"
#include "fdt.h"
#include "printk.h"
#include "stdbool.h"
#include "buddy.h"
#include "mmu.h"

#define EARLY_MM_ALIGN		8UL
#define MEMBLOCK_MAX_MEMORY	16
#define MEMBLOCK_MAX_RESERVED	16

/*
 * Linux-style memblock: physical memory and reserved regions are tracked
 * in two small arrays.  The allocator is a simple bump allocator over the
 * memory[] array after reserved regions have been carved out.
 *
 * No free list, no coalescing, no sorting.  Early allocations are expected
 * to live until the buddy/page allocator takes over.
 */
struct memblock_region {
	uint64_t base;
	uint64_t size;
};

struct memblock {
	struct memblock_region memory[MEMBLOCK_MAX_MEMORY];
	int memory_cnt;
	struct memblock_region reserved[MEMBLOCK_MAX_RESERVED];
	int reserved_cnt;
	bool initialized;
};

static struct memblock memblock;
static uint64_t memory_start = ~0ULL;
static uint64_t memory_end;

static void memblock_add_memory(uint64_t base, uint64_t size)
{
	if (memblock.memory_cnt >= MEMBLOCK_MAX_MEMORY || size == 0)
		return;
	memblock.memory[memblock.memory_cnt].base = base;
	memblock.memory[memblock.memory_cnt].size = size;
	memblock.memory_cnt++;

	if (base < memory_start) {
		memory_start = base;
	}
	if (base + size > memory_end) {
		memory_end = base + size;
	}
}

static void memblock_add_reserved(uint64_t base, uint64_t size)
{
	if (memblock.reserved_cnt >= MEMBLOCK_MAX_RESERVED || size == 0)
		return;
	memblock.reserved[memblock.reserved_cnt].base = base;
	memblock.reserved[memblock.reserved_cnt].size = size;
	memblock.reserved_cnt++;
}

/*
 * Carve [rbase, rend) out of the memory[] array.
 * Handles remove / trim start / trim end / split.
 */
static void memblock_carve(uint64_t rbase, uint64_t rend)
{
	int i;

	if (rbase >= rend)
		return;

	for (i = 0; i < memblock.memory_cnt; i++) {
		struct memblock_region *m = &memblock.memory[i];
		uint64_t mbase = m->base;
		uint64_t mend = mbase + m->size;

		/* No overlap */
		if (rend <= mbase || rbase >= mend)
			continue;

		if (rbase <= mbase && rend >= mend) {
			/* Reserved covers the whole region: remove it */
			*m = memblock.memory[--memblock.memory_cnt];
			i--;
		} else if (rbase <= mbase && rend < mend) {
			/* Overlaps start: trim from left */
			m->base = rend;
			m->size = mend - rend;
		} else if (rbase > mbase && rend >= mend) {
			/* Overlaps end: trim from right */
			m->size = rbase - mbase;
		} else {
			/* Reserved sits in the middle: split */
			uint64_t left_size = rbase - mbase;
			uint64_t right_size = mend - rend;

			m->size = left_size;

			if (right_size > 0 &&
			    memblock.memory_cnt < MEMBLOCK_MAX_MEMORY) {
				memblock.memory[memblock.memory_cnt].base = rend;
				memblock.memory[memblock.memory_cnt].size = right_size;
				memblock.memory_cnt++;
			}
		}
	}
}

/*
 * Carve all reserved regions from memory[] before enabling the allocator.
 */
static void memblock_carve_reserved(void)
{
	int i;

	for (i = 0; i < memblock.reserved_cnt; i++) {
		uint64_t rbase = memblock.reserved[i].base;
		uint64_t rend = rbase + memblock.reserved[i].size;
		memblock_carve(rbase, rend);
	}
}

/*
 * FDT helpers: parse memory nodes from device tree
 */
static void memblock_fdt_register_one(const fdt32_t *reg, uint64_t len,
				      int address_cells, int size_cells)
{
	uint64_t i = 0, j;

	while (i * sizeof(fdt32_t) < len) {
		uint64_t addr = 0, size = 0;

		for (j = i + address_cells; i < j; i++)
			addr = (addr << 32) + fdt32_to_cpu(reg[i]);
		for (j = i + size_cells; i < j; i++)
			size = (size << 32) + fdt32_to_cpu(reg[i]);

		memblock_add_memory(addr, size);
	}
}

static void memblock_fdt_register_all(void)
{
	void *fdt = fdt_base();
	int root = fdt_path_offset(fdt, "/");
	int address_cells = fdt_address_cells(fdt, root);
	int size_cells = fdt_size_cells(fdt, root);
	int node, prop_len;

	fdt_for_each_node(node, fdt) {
		const char *type = fdt_getprop(fdt, node, "device_type", NULL);
		if (!type || type[0] != 'm')
			continue;
		const fdt32_t *reg = fdt_getprop(fdt, node, "reg", &prop_len);
		if (!reg)
			continue;
		memblock_fdt_register_one(reg, prop_len, address_cells, size_cells);
	}
}

static void memblock_stat(void)
{
	int total_size = 0;
	for (int i = 0; i < memblock.memory_cnt; i++) {
		total_size += memblock.memory[i].size;
	}

	printk("early_mm: %d MB available\n", total_size / (1024 * 1024));
}


void memblock_init(void)
{
	extern char __image_end[];
	struct memblock_region reserves[] = {
		{0, __VA_PA__(__image_end)},
	};
	int nr = sizeof(reserves) / sizeof(struct memblock_region);
	for (int i = 0; i < nr; i++) {
		early_mm_reserve(reserves[i].base, reserves[i].size);
	}
	memblock_fdt_register_all();
	memblock_carve_reserved();
	memblock.initialized = true;
	memblock_stat();
}

/*
 * Reserve a physical memory region so it will not be allocated.
 * May be called before memblock_init() (recorded in memblock.reserved[])
 * or after (carved directly out of memory[]).
 */
void early_mm_reserve(uint64_t base, uint64_t size)
{
	uint64_t rbase = ALIGN_UP(base, EARLY_MM_ALIGN);
	uint64_t rend = ALIGN_DOWN(base + size, EARLY_MM_ALIGN);
	uint64_t rsize;

	if (rend <= rbase)
		return;

	rsize = rend - rbase;

	if (!memblock.initialized)
		memblock_add_reserved(rbase, rsize);
	else
		memblock_carve(rbase, rend);
}

/*
 * Allocate a block of the given size aligned to @align.
 * Simple bump allocator over the memory[] regions.
 */
void *memblock_alloc_aligned(uint64_t size, uint64_t align)
{
	int i;
	uint64_t alloc_size;

	if (size == 0)
		return NULL;

	alloc_size = ALIGN_UP(size, align);

	for (i = 0; i < memblock.memory_cnt; i++) {
		struct memblock_region *m = &memblock.memory[i];
		uint64_t base = ALIGN_UP(m->base, align);
		uint64_t end = m->base + m->size;

		/* Avoid returning a NULL pointer for RAM that starts at PA 0. */
		if (base == 0)
			base = align;

		if (base + alloc_size > end)
			continue;

		/* Bump: shrink the region from the front */
		m->base = base + alloc_size;
		m->size = end - m->base;
		return (void *)(uintptr_t)base;
	}

	return NULL;
}

void memblock_free_to_buddy(void)
{
	int cnt = memblock.memory_cnt;
	struct memblock_region *m;

	for (int i = 0; i < cnt; i++) {
		m = &memblock.memory[i];
		buddy_free_pages_range(m->base, m->size);
	}
}

void memblock_map_all(uint64_t *pgd)
{
	int cnt = memblock.memory_cnt;
	struct memblock_region *m;

	for (int i = 0; i < cnt; i++) {
		m = &memblock.memory[i];
		early_mmu_map(pgd, __PA_VA__(m->base), m->base, m->size,
			      MMU_REGION_NORMAL);
	}
}

uint64_t memblock_mem_start(void)
{
	return memory_start;
}

uint64_t memblock_mem_end(void)
{
	return memory_end;
}
