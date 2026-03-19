#ifndef VM_H
#define VM_H
#include "mem.h"

#define USER_SPACE_L1_INDEX_START 1024
#define USER_SPACE_L1_INDEX_END 1040
#define DCACHE_LINE_SIZE 32

extern void clean_invalidate_dcache(void);
extern void dcache_clean_invalidate_mva(void *addr);

static inline void clean_dcache_range(uintptr_t addr, size_t size)
{
    uintptr_t start = addr & ~(DCACHE_LINE_SIZE - 1);
    uintptr_t end = (addr + size + DCACHE_LINE_SIZE - 1) & ~(DCACHE_LINE_SIZE - 1);

    for (uintptr_t p = start; p < end; p += DCACHE_LINE_SIZE)
        dcache_clean_mva((void *)p);
}

uint32_t *mmu_init();
uint32_t *make_initial_L1_table();
void switch_L1_page_table(uint32_t pt_base, uint32_t context_id);
static inline void clean_dcache_ptes(void *pt_base, uint32_t size)
{
    uint32_t start = (uint32_t)pt_base & ~31;
    uint32_t end = (uint32_t)pt_base + size;

    for (uint32_t p = start; p < end; p += 32)
        dcache_clean_mva((void *)p);
    dsb();
}
inline void invalidate_for_l2_entry_swap(uint32_t l2_base, uint32_t va);
inline void code_onload_cache_sync(void *addr, uint32_t size);

uint32_t *fork_page_table(uint32_t *l1_table);
uint32_t *make_sure_user_space(uint32_t *l1_table_root, uint32_t user_va_start, uint32_t size);
void free_L1_page_table(uint32_t *l1_table);
static inline void set_tls_base(void *tls)
{
    asm volatile(
        "mcr p15, 0, %0, c13, c0, 3"
        :
        : "r"(tls)
        : "memory");
}

static inline void code_onload_cache_sync(void *addr, uint32_t size)
{
    dsb();
    clean_dcache_range((uintptr_t)addr, size);
    dsb();
    invalidate_icache();
    isb();
    flush_branch_predictor();
    isb();
}

#endif