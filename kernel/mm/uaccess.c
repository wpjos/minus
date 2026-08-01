#include "uaccess.h"
#include "sched.h"
#include "page.h"
#include "task.h"
#include "vma.h"
#include "string.h"
#include "memory.h"

long copy_from_user(void *to, const void *from, size_t n)
{
	struct mm_struct *mm = current->mm;
	uint8_t *dst = to;
	uint64_t addr = (uint64_t)from;
	size_t left = n;

	if (!mm)
		return n;

	while (left > 0) {
		struct page *page;
		size_t offset, chunk;

		page = vma_find_page(mm, addr, &offset);
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

long copy_to_user(void *to, const void *from, size_t n)
{
	struct mm_struct *mm = current->mm;
	const uint8_t *src = from;
	uint64_t addr = (uint64_t)to;
	size_t left = n;

	if (!mm)
		return n;

	while (left > 0) {
		struct page *page;
		size_t offset, chunk;

		page = vma_find_page(mm, addr, &offset);
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
