#include "mm.h"
#include "fdt.h"
#include "printk.h"

#define EARLY_MM_ALIGN		8UL
#define MIN_BLOCK_SIZE		32UL

#define ALIGN_UP(x, a)		(((x) + ((a) - 1)) & ~((a) - 1))
#define ALIGN_DOWN(x, a)	((x) & ~((a) - 1))

/*
 * Early memory allocator — embedded free list
 *
 * Each memory block (free or allocated) starts with a size field.
 * The size field records the total block size including itself (8 bytes).
 *
 * Allocated block layout:
 *   [uint64_t size] [user data ...]
 *                    ^-- returned pointer
 *
 * Free block layout:
 *   [uint64_t size] [free_block *next] [unused ...]
 *
 * Free blocks are linked in an address-sorted singly-linked list.
 * This makes coalescing on free O(1) amortized (check prev/next neighbors).
 *
 * Unlike the old fixed-array design, metadata lives inside the memory blocks
 * themselves, so the number of allocations is limited only by physical memory,
 * not by a static array size.
 */
struct free_block {
	uint64_t size;            /* total block size including this field */
	struct free_block *next;  /* free list link (only valid when free) */
};

/* Free list head (sorted by address) */
static struct free_block *free_list;

/* Stats */
static uint64_t total_size;
static uint64_t used_size;

/*
 * Add a memory region to the allocator
 */
static void early_mm_add_region(uint64_t base, uint64_t size)
{
	uint64_t aligned_base = ALIGN_UP(base, EARLY_MM_ALIGN);
	uint64_t aligned_end = ALIGN_DOWN(base + size, EARLY_MM_ALIGN);
	uint64_t aligned_size = aligned_end - aligned_base;

	if (aligned_size < MIN_BLOCK_SIZE)
		return;

	struct free_block *blk = (struct free_block *)(uintptr_t)aligned_base;
	blk->size = aligned_size;
	blk->next = free_list;
	free_list = blk;

	total_size += aligned_size;
}

/*
 * Sort the free list by address and coalesce adjacent blocks.
 * Called once after all regions are added during init.
 */
static void sort_and_coalesce(void)
{
	/* Insertion sort by address (few regions expected) */
	struct free_block *sorted = NULL;
	struct free_block *cur = free_list;

	while (cur) {
		struct free_block *next = cur->next;

		if (!sorted || cur < sorted) {
			cur->next = sorted;
			sorted = cur;
		} else {
			struct free_block *p = sorted;
			while (p->next && p->next < cur)
				p = p->next;
			cur->next = p->next;
			p->next = cur;
		}
		cur = next;
	}

	free_list = sorted;

	/* Coalesce adjacent blocks */
	cur = free_list;
	while (cur && cur->next) {
		if ((uint8_t *)cur + cur->size == (uint8_t *)cur->next) {
			cur->size += cur->next->size;
			cur->next = cur->next->next;
			/* Don't advance — check if merged block is also adjacent */
		} else {
			cur = cur->next;
		}
	}
}

/*
 * FDT helpers: parse memory nodes from device tree
 */
static void early_mm_fdt_register_one(const fdt32_t *reg, uint64_t len,
				      int address_cells, int size_cells)
{
	uint64_t i = 0, j;

	while (i * sizeof(fdt32_t) < len) {
		uint64_t addr = 0, size = 0;

		for (j = i + address_cells; i < j; i++)
			addr = (addr << 32) + fdt32_to_cpu(reg[i]);
		for (j = i + size_cells; i < j; i++)
			size = (size << 32) + fdt32_to_cpu(reg[i]);

		early_mm_add_region(addr, size);
	}
}

static void early_mm_fdt_register_all(void *fdt, int root)
{
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
		early_mm_fdt_register_one(reg, prop_len, address_cells, size_cells);
	}
}

void early_mm_init(void)
{
	void *fdt = fdt_base();

	if (!fdt)
		return;

	int root = fdt_path_offset(fdt, "/");
	if (root < 0)
		return;

	early_mm_fdt_register_all(fdt, root);
	sort_and_coalesce();

	printk("early_mm: %d MB available\n", (int)(total_size / (1024 * 1024)));
}

/*
 * Allocate a block of the given size (8-byte aligned)
 * Uses first-fit search on the sorted free list
 */
void *early_mm_alloc(uint64_t size)
{
	struct free_block *prev, *blk;

	if (size == 0)
		return NULL;

	size = ALIGN_UP(size, EARLY_MM_ALIGN);
	uint64_t total = size + sizeof(uint64_t);

	/* First fit */
	prev = NULL;
	blk = free_list;
	while (blk) {
		if (blk->size >= total)
			break;
		prev = blk;
		blk = blk->next;
	}

	if (!blk)
		return NULL;

	/* Split if remainder is large enough */
	if (blk->size >= total + MIN_BLOCK_SIZE) {
		struct free_block *rem = (struct free_block *)((uint8_t *)blk + total);
		rem->size = blk->size - total;
		rem->next = blk->next;

		if (prev)
			prev->next = rem;
		else
			free_list = rem;

		blk->size = total;
	} else {
		/* Use entire block */
		total = blk->size;
		if (prev)
			prev->next = blk->next;
		else
			free_list = blk->next;
	}

	used_size += total;
	return (void *)((uint8_t *)blk + sizeof(uint64_t));
}

/*
 * Free a previously allocated block
 * Inserts back into sorted free list and coalesces with neighbors
 */
void early_mm_free(void *ptr)
{
	struct free_block *blk, *prev, *ins;

	if (!ptr)
		return;

	blk = (struct free_block *)((uint8_t *)ptr - sizeof(uint64_t));
	used_size -= blk->size;

	/* Insert into sorted free list */
	prev = NULL;
	ins = free_list;
	while (ins && ins < blk) {
		prev = ins;
		ins = ins->next;
	}

	blk->next = ins;
	if (prev)
		prev->next = blk;
	else
		free_list = blk;

	/* Coalesce with next neighbor */
	if (blk->next && (uint8_t *)blk + blk->size == (uint8_t *)blk->next) {
		blk->size += blk->next->size;
		blk->next = blk->next->next;
	}

	/* Coalesce with previous neighbor */
	if (prev && (uint8_t *)prev + prev->size == (uint8_t *)blk) {
		prev->size += blk->size;
		prev->next = blk->next;
	}
}
