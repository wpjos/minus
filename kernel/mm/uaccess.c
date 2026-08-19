#include "uaccess.h"
#include "page.h"
#include "vspace.h"
#include "string.h"
#include "memory.h"
#include "cap.h"
#include "switch_notify.h"

/*
 * Per-CPU mirror of the running thread's address space, fed by core's
 * switch notification bus.  Plain statics on this single-CPU build;
 * become per-CPU variables under SMP.  Early-boot context (idle thread)
 * arrives as a NULL envelope and mirrors as NULL / CAP_NULL, and the
 * copy paths below fail soft exactly like a failed resolve did before.
 *
 * The same callback owns address-space ACTIVATION: mm is where vspace
 * objects live, so TTBR0 flips here, inside the switch, right before
 * the new thread's registers are restored - no one else in the kernel
 * switches page tables, and core's switcher never calls into mm.
 *
 * Keeping the resolved vspace pointer here means the copy paths - which
 * run on every syscall - do no service round-trip and no cap_resolve.
 */
static struct vspace *cur_vspace;
static cap_t cur_vspace_cap;

static void uaccess_switch_notify(const struct switch_envelope *prev,
				  const struct switch_envelope *next)
{
	struct vspace *next_vs;

	cur_vspace_cap = next ? next->vspace_cap : CAP_NULL;
	next_vs = cur_vspace_cap != CAP_NULL
		? (struct vspace *)cap_resolve(cur_vspace_cap,
					       cap_rights(CAP_R_MAP))
		: NULL;

	if (next_vs != cur_vspace)
		switch_vspace(cur_vspace, next_vs);
	cur_vspace = next_vs;
}

struct vspace *mm_current_vspace(void)
{
	return cur_vspace;
}

cap_t mm_current_vspace_cap(void)
{
	return cur_vspace_cap;
}

int uaccess_init(void)
{
	return switch_notifier_register(uaccess_switch_notify);
}

long copy_from_user(void *to, user_addr_t from, size_t n)
{
	struct vspace *vs = cur_vspace;
	uint8_t *dst = to;
	uintptr_t addr = (uintptr_t)from;
	size_t left = n;

	if (!vs)
		return n;

	while (left > 0) {
		struct page *page;
		size_t offset, chunk;

		page = vspace_find_page(vs, addr, &offset);
		if (!page)
			return left;

		chunk = PAGE_SIZE - offset;
		if (chunk > left)
			chunk = left;

		memcpy(dst, (uint8_t *)page_to_virt(page) + offset, chunk);

		addr += chunk;
		dst += chunk;
		left -= chunk;
	}

	return 0;
}

long copy_to_user(user_addr_t to, const void *from, size_t n)
{
	struct vspace *vs = cur_vspace;
	const uint8_t *src = from;
	uintptr_t addr = (uintptr_t)to;
	size_t left = n;

	if (!vs)
		return n;

	while (left > 0) {
		struct page *page;
		size_t offset, chunk;

		page = vspace_find_page(vs, addr, &offset);
		if (!page)
			return left;

		chunk = PAGE_SIZE - offset;
		if (chunk > left)
			chunk = left;

		memcpy((uint8_t *)page_to_virt(page) + offset, src, chunk);

		addr += chunk;
		src += chunk;
		left -= chunk;
	}

	return 0;
}

long strncpy_from_user(char *dst, user_addr_t src, size_t n)
{
	struct vspace *vs = cur_vspace;
	uintptr_t addr = (uintptr_t)src;
	size_t left = n;

	if (!vs || n == 0)
		return n;

	while (left > 1) {
		struct page *page;
		size_t offset, chunk;
		size_t i;

		page = vspace_find_page(vs, addr, &offset);
		if (!page)
			break;

		chunk = PAGE_SIZE - offset;
		if (chunk > left - 1)
			chunk = left - 1;

		for (i = 0; i < chunk; i++) {
			char c = ((char *)page_to_virt(page))[offset + i];
			*dst++ = c;
			addr++;
			left--;
			if (c == '\0')
				return 0;
		}
	}

	*dst = '\0';
	return left;
}
