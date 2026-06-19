#ifndef __STDBOOL_H__
#define __STDBOOL_H__

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
// C23 及以上版本，bool 是关键字，不需要定义
#define bool bool
#else
// C23 之前的版本，需要定义 bool
typedef _Bool bool;
#define true 1
#define false 0
#endif

#endif
