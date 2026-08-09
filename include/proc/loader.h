#ifndef __LOADER_H__
#define __LOADER_H__

#define USER_LOAD_BASE	0x400000UL
#define USER_STACK_TOP	0x80000000UL

void run_user_init(const char *path);

#endif /* __LOADER_H__ */
