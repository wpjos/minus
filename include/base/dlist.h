#ifndef __LIST_H__
#define __LIST_H__

#include "types.h"

struct dlist_node {
	struct dlist_node *prev;
	struct dlist_node *next;
};

void dlist_add(struct dlist_node *head, struct dlist_node *node);
void dlist_del(struct dlist_node *node);
void dlist_init(struct dlist_node *node);

#define dlist_empty(head)	((head)->next == (head))

#define dlist_first_entry(head, type, member) \
    container_of((head)->next, type, member)

#define dlist_next_entry(pos, member) \
    container_of((pos)->member.next, typeof(*pos), member)

#define dlist_for_each_entry(pos, head, member)                 \
    for (pos = dlist_first_entry(head, typeof(*pos), member);   \
         &pos->member != (head);                                \
         pos = dlist_next_entry(pos, member))

#endif
