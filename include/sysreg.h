#ifndef __SYSREG_H__
#define __SYSREG_H__

#include "types.h"

#define sys_reg_write(reg, value) \
    do { \
        uint64_t v = (uint64_t)(value); \
        __asm__ volatile("msr " #reg ", %0" : : "r"(v) : "memory"); \
    } while(0)

#define sys_reg_read(reg) ({ \
    uint64_t v; \
    __asm__ volatile("mrs %0, " #reg : "=r"(v)); \
    v; \
})

#define DEFINE_SYS_REG_RW_OPS(prefix, name, reg) \
    static inline void prefix##_set_##name(uint64_t val) { \
        sys_reg_write(reg, val); \
    } \
    static inline uint64_t prefix##_get_##name(void) { \
        return sys_reg_read(reg); \
    }

#define DEFINE_SYS_REG_RO_OPS(prefix, name, reg) \
    static inline uint64_t prefix##_get_##name(void) { \
        return sys_reg_read(reg); \
    }
#endif
