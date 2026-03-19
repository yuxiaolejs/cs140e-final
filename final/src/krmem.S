.global krep_mem_move_start
krep_mem_move_start:
    @ this is basically memcpy, but jump to the new address instead of returning, no need to follow call convention since we never return
    @ r0 = target_addr, r1 = new_code, r2 = code_size
    mov     r7, #0x40
    orr     r7, r7, #0x5000
    orr     r7, r7, #0x210000
    orr     r7, r7, #0x20000000
    mov     r6, #1024
    mov     r8, #64 @ ascii for @
    lsrs    r2, r2, #2
loop:
    subs    r6, r6, #1
    bne     mov_
    str     r8, [r7]@ print @ for progress
    mov     r6, #1024
mov_:
    ldr     r12, [r1], #4
    str     r12, [r0], #4
    subs    r2, r2, #1
    bne     loop
    mov     r8, #65 @ ascii for A
    str     r8, [r7] @ print A for copy done
    @ Zero BSS region: r0 already points past the copied data
    @ Zero 64KB (0x4000 words) to cover BSS + alignment padding
    mov     r12, #0
    mov     r2, #0x4000
zero_bss:
    str     r12, [r0], #4
    subs    r2, r2, #1
    bne     zero_bss
    mov     r8, #66 @ ascii for B
    str     r8, [r7] @ print B for BSS zeroed
    @ Invalidate I-cache so CPU fetches fresh instructions from new kernel
    mov     r12, #0
    mcr     p15, 0, r12, c7, c5, 0  @ invalidate entire I-cache
    mcr     p15, 0, r12, c7, c5, 0  @ again for safety
    mcr     p15, 0, r12, c7, c10, 4 @ DSB
    mcr     p15, 0, r12, c7, c5, 4  @ ISB (prefetch flush)
    mcr     p15, 0, r12, c7, c5, 6  @ flush branch predictor
    mcr     p15, 0, r12, c7, c5, 4  @ ISB
    mov     pc, r3
.global krep_mem_move_end
krep_mem_move_end:
    b       krep_mem_move_end
