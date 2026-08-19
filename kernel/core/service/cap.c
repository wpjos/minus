#include "cap.h"
#include "string.h"
#include "printk.h"

#define CAP_TABLE_SIZE 4096

struct cap_entry {
	void            *obj;
	uint64_t        rights;
	uint32_t        generation;
	/* Index of the next free entry; valid only when !used. 0 terminates. */
	uint32_t        next_free;
	uint8_t         used;
};

static struct cap_entry g_cap_table[CAP_TABLE_SIZE];
static uint32_t g_cap_next_generation = 1;

/*
 * Free-list head: index of the first free entry, 0 when empty.
 * g_cap_free_inited distinguishes "empty" from "not built yet" so the
 * singly-linked free stack is constructed lazily on the first alloc.
 */
static uint32_t g_cap_free_head;
static int g_cap_free_inited;

static void cap_free_list_init(void)
{
	uint32_t i;

	for (i = 1; i < CAP_TABLE_SIZE - 1; i++)
		g_cap_table[i].next_free = i + 1;
	g_cap_table[CAP_TABLE_SIZE - 1].next_free = 0;
	g_cap_free_head = 1;
	g_cap_free_inited = 1;
}

static inline uint32_t cap_index(cap_t cap)
{
	return (uint32_t)(cap & 0xffffffffULL);
}

static inline uint32_t cap_generation(cap_t cap)
{
	return (uint32_t)(cap >> 32);
}

static inline cap_t cap_make(uint32_t idx, uint32_t gen)
{
	return ((uint64_t)gen << 32) | (uint64_t)idx;
}

cap_t cap_alloc(void *obj, struct cap_rights rights)
{
	uint32_t idx;
	struct cap_entry *e;
	uint32_t gen;

	if (!obj)
		return CAP_NULL;

	if (!g_cap_free_inited)
		cap_free_list_init();

	idx = g_cap_free_head;
	if (idx == 0) {
		printk("cap: table full\n");
		return CAP_NULL;
	}

	e = &g_cap_table[idx];
	g_cap_free_head = e->next_free;

	gen = g_cap_next_generation++;
	if (gen == 0)
		gen = g_cap_next_generation++;

	e->obj = obj;
	e->rights = rights.rights;
	e->generation = gen;
	e->used = 1;
	return cap_make(idx, gen);
}

void *cap_resolve(cap_t cap, struct cap_rights required)
{
	uint32_t idx = cap_index(cap);
	uint32_t gen = cap_generation(cap);
	struct cap_entry *e;

	if (cap == CAP_NULL || idx == 0 || idx >= CAP_TABLE_SIZE)
		return NULL;

	e = &g_cap_table[idx];
	if (!e->used || e->generation != gen)
		return NULL;

	if ((e->rights & required.rights) != required.rights)
		return NULL;

	return e->obj;
}

void cap_free(cap_t cap)
{
	uint32_t idx = cap_index(cap);
	uint32_t gen = cap_generation(cap);
	struct cap_entry *e;

	if (cap == CAP_NULL || idx == 0 || idx >= CAP_TABLE_SIZE)
		return;

	e = &g_cap_table[idx];
	if (!e->used || e->generation != gen)
		return;

	e->used = 0;
	e->obj = NULL;
	e->rights = 0;
	/* generation is intentionally left intact to invalidate old tokens */
	e->next_free = g_cap_free_head;
	g_cap_free_head = idx;
}
