#include "vm.h"
#include "kuser.h"
uint32_t *l1_table_phys;

// some good cache ops
inline void invalidate_for_l2_entry_swap(uint32_t l2_base, uint32_t va)
{
    uint32_t l2_idx = (va >> 12) & 0xFF;
    void *pte = (void *)(l2_base + l2_idx * 4);

    clean_dcache_ptes(pte, 4);
    clean_dcache_range((void *)(va & ~0xFFF), 4096);
    dsb();

    flush_tlb();
    dsb();
    isb();
}

uint32_t make_L2_page_on_the_fly_and_make_valid(uint32_t VA)
{
    // Here we don't know the page table root (since we are on the fly)
    VA &= ~0xFFF; // align to page
    uint32_t L1_base;
    asm volatile("mrc p15, 0, %0, c2, c0, 0" : "=r"(L1_base));
    uint32_t *L1_table = (uint32_t *)L1_base;

    return (uint32_t)make_sure_user_space(L1_table, VA, 4096);
}

// L1: Section[31:20] | NS[19] | 0[18] | SBZ[17:15] | TEX[14:12] | AP[11:10] | P[9] | Domain[8:5] | 0[4] | C[3] | B[2] | 2[1:0]
// L1: Coarse[31:10] | P[9] | Domain[8:5] | SBZ[4] | NS[3] | SBZ[2] | 1[1:0]
// C = Cacheable, B = Bufferable, P = not supported by 1176
// AP = 0b11 kernel only,
/*
 * APX/AP permission encoding (ARMv6) // not found in the fucking manual
 *
 * APX AP | Supervisor permissions | User permissions | Access type
 * -------+------------------------+------------------+-------------------------------
 *  0  00 | No access              | No access        | All accesses generate a
 *        |                        |                  | permission fault
 *  0  01 | Read/write             | No access        | Supervisor access only
 *  0  10 | Read/write             | Read only        | User writes generate
 *        |                        |                  | permission faults
 *  0  11 | Read/write             | Read/write       | Full access
 *  1  00 | DO NOT USE             | DO NOT USE       | reserved
 *  1  01 | Read only              | No access        | Supervisor read only
 *  1  10 | Read only              | Read only        | Supervisor/User read only
 *  1  11 | Read only              | Read only        | Supervisor/User read only
 */
uint32_t make_L2_page_entry(uint32_t pa, uint32_t perms, uint8_t cacheable, uint8_t bufferable)
{
    return (pa & 0xFFFFF000) | (perms & 0xFF) << 4 | (cacheable & 1) << 3 | (bufferable & 1) << 2 | 0b10; // Small page
}

uint32_t *make_initial_L1_table()
{
    l1_table_phys = kalloc(16384);
    for (int i = 0; i < 512; i++)
        l1_table_phys[i] = (i << 20) | (0b01 << 10) | 0b0010; // Kernel space identity map, cacheable, bufferable, domain 0
    for (int i = 512; i < 3072; i++) // ends 0xc0000000
        l1_table_phys[i] = (i << 20) | (0b01 << 10) | 0b0010; // Possible IO Kernel space identity map, cacheable, bufferable, domain 0
    for (int i = 3072; i < 4095; i++)// starts 0xc0000000, ends 0xfff00000
        l1_table_phys[i] = (i << 20) | (0b01 << 10) | 0b1110; // Kernel space identity map, cacheable, bufferable, domain 0
    // last section will be needed for kuser
    uint32_t last_l2_table_addr = kalloc(1024);
    l1_table_phys[4095] = ((uint32_t)last_l2_table_addr & 0xFFFFFC00) | 0b01; // point over
    uint32_t *last_l2_table = (uint32_t *)last_l2_table_addr;
    uint32_t *kuser_page_4k = kuser_init();
    for (int j = 0; j < 256; j++)
    {
        uint32_t va = (4095 << 20) | (j << 12);
        if (va == 0xFFFF0000)
            last_l2_table[j] = make_L2_page_entry((uint32_t)kuser_page_4k, 0b11111111, 1, 1); // Kernel id map
        else
            last_l2_table[j] = make_L2_page_entry(va, 0b01010101, 1, 1); // Kernel id map
    }
    printk("Handing out initial L1 page table at %x\n", l1_table_phys);
    return l1_table_phys;
}

void free_L1_page_table(uint32_t *l1_table)
{
    // clean_invalidate_dcache(); // nuke approach - works but heavy

    for (int i = 0; i < 4095; i++)
    {
        uint32_t l1_entry = l1_table[i];
        if ((l1_entry & 0b11) == 0b01)
        {
            // Coarse page table
            uint32_t l2_base = l1_entry & 0xFFFFFC00;
            uint32_t *l2_table = (uint32_t *)l2_base;
            // printk("Freeing L2 page table at %x\n", l2_base);
            for (int j = 0; j < 256; j++)
            {
                uint32_t l2_entry = l2_table[j];
                // printk("Freeing L2 entry %x: %x\n", j, l2_entry);
                if ((l2_entry & 0b11) == 0b10 && (l2_entry & 0xFFFFF000) < 0xfff00000) // do not free kuser page
                {
                    // Small page
                    uint32_t pa = l2_entry & 0xFFFFF000;
                    // Flush by user VA - these are the VAs the cache lines
                    // were dirtied through. Page table is still active here.
                    uint32_t va = (i << 20) | (j << 12);
                    for (uint32_t off = 0; off < 4096; off += 32)
                        dcache_clean_invalidate_mva((void *)(va + off));
                    dsb();
                    kfree((void *)pa);
                }
            }
            kfree((void *)l2_base);
        }
    }
    kfree((void *)l1_table);
}

uint32_t *make_sure_user_space(uint32_t *l1_table_root, uint32_t user_va_start, uint32_t size)
{
    uint32_t start_va = user_va_start & ~0xFFF;                // align down to 4k
    uint32_t end_va = (user_va_start + size + 0xFFF) & ~0xFFF; // align up to 4k

    for (uint32_t va = start_va; va < end_va; va += 0x1000)
    {
        uint32_t l1_index = (va >> 20) & 0xFFF;
        uint32_t l1_entry = l1_table_root[l1_index];
        uint32_t l2_index = (va >> 12) & 0xFF;
        if ((l1_entry & 0b11) != 0b01)
        {
            // Create L2 page table
            uint32_t *l2_table = kalloc(1024);
            memset(l2_table, 0, 1024);
            l1_table_root[l1_index] = ((uint32_t)l2_table & 0xFFFFFC00) | 0b01; // Point to L2 table
            // Flush L1 PTE to memory so hardware page walker sees it
            clean_dcache_ptes(&l1_table_root[l1_index], sizeof(uint32_t));
            dsb();
            flush_tlb();
            dsb();
            isb();
            l1_entry = l1_table_root[l1_index];
        }
        // ok we have l2 table, find it and update the entry
        uint32_t l2_base = l1_entry & 0xFFFFFC00;
        uint32_t *l2_table = (uint32_t *)l2_base;
        // to avoid memory leak, first check if the entry is already valid
        if ((l2_table[l2_index] & 0b11) == 0b00)
        {
            // Allocate a new page
            uint32_t *new_page = kalloc(4096);
            memset(new_page, 0, 4096);                                                     // Zero-initialize (required by POSIX for brk)
            l2_table[l2_index] = make_L2_page_entry((uint32_t)new_page, 0b11111111, 1, 1); // User space, read/write, cacheable, bufferable
            invalidate_for_l2_entry_swap(l2_base, va);
        }
        else
        {
            // OK, this page is valid, double check permissions
            printk("+++page for VA %x already exists, checking perms\n", va);
            l2_table[l2_index] &= ~(0xFF << 4);      // clear old perms
            l2_table[l2_index] |= (0b11111111 << 4); // set to full access
            invalidate_for_l2_entry_swap(l2_base, va);
        }
    }
    return l1_table_root;
}

uint32_t *fork_page_table(uint32_t *l1_table)
{
    // Ensure all pending writes are visible before copying
    dsb();
    asm volatile("mcr p15, 0, %0, c7, c10, 0" : : "r"(0)); // Clean entire D-cache
    dsb();

    // new_l1
    uint32_t *new_l1 = kalloc(16384);
    memcpy(new_l1, l1_table, 16384);

    for (int i = 0; i < 4095; i++)
    {
        if (i >= 512 && i < 516)
            continue; // skip MMIO
        uint32_t l1_entry = new_l1[i];
        if ((l1_entry & 0b11) == 0b01)
        {
            // Coarse page table, need to copy
            uint32_t l2_base = l1_entry & 0xFFFFFC00;
            uint32_t *l2_table = (uint32_t *)l2_base;
            uint32_t *new_l2_table = kalloc(1024);
            memcpy(new_l2_table, l2_table, 1024);
            for (int j = 0; j < 256; j++)
            {
                uint32_t l2_entry = new_l2_table[j];
                if ((l2_entry & 0b11) == 0b10)
                {
                    // Small page
                    uint32_t pa = l2_entry & 0xFFFFF000;
                    uint32_t *old_page = (uint32_t *)pa;
                    uint32_t *new_page = kalloc(4096);
                    memcpy(new_page, old_page, 4096);
                    new_l2_table[j] = make_L2_page_entry((uint32_t)new_page, 0b11111111, 1, 1); // User space, read/write, cacheable, bufferable
                }
            }
            // Update L1 entry to point to new L2 table
            new_l1[i] = ((uint32_t)new_l2_table & 0xFFFFFC00) | (l1_entry & 0x3FF);
        }
        else if ((l1_entry & 0b11) == 0b10)
        {
            // Section mapping - should not happen for user space, so no copy
        }
    }
    return new_l1;
}

uint32_t *mmu_init()
{
    uint32_t *l1_table = make_initial_L1_table();
    enable_mmu(l1_table);
    return l1_table;
}
