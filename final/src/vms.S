@ Clean and Invalidate entire D-cache, then DSB.
@ Writes all dirty lines to memory and removes them from cache.
.global clean_invalidate_dcache
clean_invalidate_dcache:
    mov r0, #0
    mcr p15, 0, r0, c7, c14, 0 @ Clean & Invalidate entire D-cache
    mcr p15, 0, r0, c7, c10, 4 @ DSB
    bx lr

@ Clean and Invalidate D-cache line by MVA (r0 = address).
.global dcache_clean_invalidate_mva
dcache_clean_invalidate_mva:
    mcr p15, 0, r0, c7, c14, 1 @ DCCIMVAC
    bx lr

.global switch_L1_page_table
switch_L1_page_table:
    push {r4, r5, r6}
    mov r6, #0
    mcr p15, 0, r6, c7, c10, 0 @ inv whole fucking d cache
    mcr p15, 0, r6, c7, c10, 4 @ dsb
    mcr p15, 0, r6, c13, c0, 1 @ asid = 0
    mcr p15, 0, r6, c7, c5, 4 @ isb
    mcr p15, 0, r0, c2, c0, 0 @ TTBR0 = r0
    mcr p15, 0, r6, c7, c5, 4 @ isb
    mcr p15, 0, r6, c7, c10, 4 @ dsb
    mcr p15, 0, r1, c13, c0, 1 @ asid = r1
    mcr p15, 0, r6, c7, c5, 4 @ isb
    mcr p15, 0, r6, c8, c7, 0 @ TLB
    mcr p15, 0, r6, c7, c5, 4 @ isb
    mcr p15, 0, r6, c7, c5, 0 @ inv i cache
    mcr p15, 0, r6, c7, c5, 0 @ inv i cache
    mcr p15, 0, r6, c7, c5, 0 @ inv i cache
    mcr p15, 0, r6, c7, c5, 0 @ inv i cache
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    mcr p15, 0, r6, c7, c10, 4 @ dsb
    mcr p15, 0, r6, c7, c5, 6 @ btb
    mcr p15, 0, r6, c7, c10, 4 @ dsb
    mcr p15, 0, r6, c7, c5, 4 @ isb
    pop {r4, r5, r6}
    bx lr

.global enable_mmu
enable_mmu:
    push {r4, r5, r6}
    mov r6, #0
    mcr p15, 0, r6, c7, c5, 0 @ inv i cache
    mcr p15, 0, r6, c7, c5, 0 @ inv i cache
    mcr p15, 0, r6, c7, c5, 0 @ inv i cache
    mcr p15, 0, r6, c7, c5, 0 @ inv i cache
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    mcr p15, 0, r6, c7, c10, 4 @ dsb
    mcr p15, 0, r0, c2, c0, 0 @ TTBR0 = r0
    mcr p15, 0, r6, c7, c5, 4 @ isb
    mov r1, #1
    mcr p15, 0, r1, c3, c0, 0 @ Domain access: set domain 0 to client
    mcr p15, 0, r6, c7, c5, 4 @ isb
    mcr p15, 0, r6, c8, c7, 0 @ Invalidate TLB
    mcr p15, 0, r6, c7, c10, 4 @ dsb

    mrc p15, 0, r0, c1, c0, 0 @ Read SCTLR
    mcr p15, 0, r6, c7, c5, 4 @ isb
    orr r0, r0, #(1 << 0)     @ Set M bit (enable MMU)
    orr r0, r0, #(1 << 2)     @ Set C bit (enable D-cache)
    @ orr r0, r0, #(1 << 23)  @ Set armv6 MMU extensions
    orr r0, r0, #(1 << 22)  @ U bit - unaligned access support
    orr r0, r0, #(1 << 12)    @ Set I bit (enable I-cache)
    @ bic r0, r0, #2          @ clear A bit - disable alignment fault
    mcr p15, 0, r0, c1, c0, 0 @ Write SCTLR
    mcr p15, 0, r6, c7, c5, 4 @ isb
    mcr p15, 0, r6, c7, c5, 6 @ btb
    mcr p15, 0, r6, c7, c5, 4 @ isb
    pop {r4, r5, r6}
    bx lr

.global disable_mmu
disable_mmu:
    push {r4, r5, r6}
    mov r6, #0
    mcr p15, 0, r6, c7, c5, 0 @ inv i cache
    mcr p15, 0, r6, c7, c5, 0 @ inv i cache
    mcr p15, 0, r6, c7, c5, 0 @ inv i cache
    mcr p15, 0, r6, c7, c5, 0 @ inv i cache
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    mcr p15, 0, r6, c7, c14, 0 @ Clean & Invalidate entire D-cache
    mcr p15, 0, r6, c7, c10, 4 @ dsb
    mcr p15, 0, r6, c7, c5, 4 @ isb
    mrc p15, 0, r0, c1, c0, 0 @ Read SCTLR
    bic r0, r0, #1          @ Clear M bit (disable MMU)
    bic r0, r0, #(1 << 2)   @ Clear C bit (disable D-cache)
    bic r0, r0, #(1 << 12)  @ Clear I bit (disable I-cache)
    mcr p15, 0, r0, c1, c0, 0 @ Write SCTLR
    mcr p15, 0, r6, c7, c6, 0 @ Invalidate entire D-cache
    mcr p15, 0, r6, c7, c5, 4 @ isb
    mcr p15, 0, r6, c7, c5, 6 @ btb
    mcr p15, 0, r6, c7, c5, 4 @ isb
    pop {r4, r5, r6}
    bx lr
