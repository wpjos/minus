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
	uintptr_t addr = (uintptr_t)from;
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
	uintptr_t addr = (uintptr_t)to;
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

long strncpy_from_user(char *dst, const char *src, size_t n)
{
	struct mm_struct *mm = current->mm;
	uintptr_t addr = (uintptr_t)src;
	size_t left = n;

	if (!mm || n == 0)
		return n;

	while (left > 1) {
		struct page *page;
		size_t offset, chunk;
		size_t i;

		page = vma_find_page(mm, addr, &offset);
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
