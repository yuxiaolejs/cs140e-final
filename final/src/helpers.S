@ These are asm helpers for sched
.text
.align 2
.global set_svc_mode_splr
.type set_svc_mode_splr, %function

set_svc_mode_splr:
    mrs     r2, cpsr
    bic     r3, r2, #0x1f
    orr     r3, r3, #0x13
    msr     cpsr_c, r3

    mov     sp, r0
    mov     lr, r1

    msr     cpsr_c, r2
    bx      lr

.global init_irq_stack_pointer
init_irq_stack_pointer:
    mrs     r0, cpsr
    orr     r0, r0, #0x80
    msr     cpsr_c, r0

    bic     r0, r0, #0x1f
    orr     r0, r0, #0x12
    msr     cpsr_c, r0

    ldr     sp, =0x9100000

    bic     r0, r0, #0x1f
    orr     r0, r0, #0x13
    msr     cpsr_c, r0
    bx      lr

.global init_all_stacks
init_all_stacks:
    @ msr cpsr_c, #0xD2      // IRQ
    @ ldr sp, =0x9100000

    msr     cpsr_c, #0xD1 // FIQ
    ldr     sp, =0x90F0000

    msr     cpsr_c, #0xD7 // ABT
    ldr     sp, =0x90E0000

    msr     cpsr_c, #0xDB // UND
    ldr     sp, =0x90D0000

    msr     cpsr_c, #0xD3 // SVC
    @ ldr sp, =0x90C0000

    bx      lr

.global irq_timer_interrupt
irq_timer_interrupt:
    sub     lr, lr, #4
    ldr     sp, =0x9100000

    stmfd   sp!, {r0-r12, lr}
    mrs     r3, spsr
    stmfd   sp!, {r3}
    sub     sp, sp, #8
    mov     r0, sp
    stmia   r0, {sp, lr}^
    nop
    mov     r0, sp

    // Below we prepare r1, r2, r3 to be irq status registers
    ldr     r1, =0x2000B200 @ IRQ Basic Pending
    ldr     r1, [r1]
    ldr     r2, =0x2000B204 @ IRQ Pending 1
    ldr     r2, [r2]
    ldr     r3, =0x2000B208 @ IRQ Pending 2
    ldr     r3, [r3]

    bl      generic_irq_handler

    @ Unreachable - use general drop to user

.global swi_interrupt
swi_interrupt:
    ldr     sp, =0x90C0000
    stmfd   sp!, {r0-r12, lr}
    mrs     r3, spsr
    stmfd   sp!, {r3}
    sub     sp, sp, #8
    mov     r0, sp
    stmia   r0, {sp, lr}^
    nop
    mov     r0, sp

    bl      swi_int_c
    @ Unreachable - use general drop to user

.global auto_generic_return
auto_generic_return:
    @ Check if svc or irq mode to return appropriately
    mrs     r3, cpsr
    and     r3, r3, #0x1f
    cmp     r3, #0x13
    beq     svc_generic_return
    b       irq_generic_return

.global irq_generic_return
irq_generic_return:
    ldr     r4, =0x2000B40C @ IRQ Enable Clear
    mov     r5, #1
    str     r5, [r4]

    mov     sp, r0
    ldmia   r0, {sp, lr}^
    nop
    add     sp, sp, #8
    ldmfd   sp!, {r3}
    msr     spsr_cxsf, r3
    ldmfd   sp!, {r0-r12, lr}

    subs    pc, lr, #0

.global svc_generic_return
svc_generic_return: @ Use this to return from SVC mode (svc_generic_return(ctx);)
    mov     sp, r0
    ldmia   r0, {sp, lr}^
    nop
    add     sp, sp, #8
    ldmfd   sp!, {r3}
    msr     spsr_cxsf, r3
    ldmfd   sp!, {r0-r12, lr}

    movs    pc, lr

.global data_abort_interrupt
data_abort_interrupt:
    stmfd   sp!, {r0-r12, lr}
    mrs     r3, spsr
    stmfd   sp!, {r3}
    sub     sp, sp, #8
    mov     r0, sp
    stmia   r0, {sp, lr}^
    nop
    mov     r0, sp

    bl      data_abort_int_c

    mov     r0, sp
    ldmia   r0, {sp, lr}^
    nop
    add     sp, sp, #8
    ldmfd   sp!, {r3}
    msr     spsr_cxsf, r3
    ldmfd   sp!, {r0-r12, lr}

    movs    pc, lr

.global prefetch_abort_interrupt
prefetch_abort_interrupt:
    sub     lr, lr, #4 @ Prefetch abort: LR = fault addr + 4
    stmfd   sp!, {r0-r12, lr}
    mrs     r3, spsr
    stmfd   sp!, {r3}
    sub     sp, sp, #8
    mov     r0, sp
    stmia   r0, {sp, lr}^
    nop
    mov     r0, sp

    bl      prefetch_abort_int_c

    @ If we return (shouldn't normally), restore and return
    mov     r0, sp
    ldmia   r0, {sp, lr}^
    nop
    add     sp, sp, #8
    ldmfd   sp!, {r3}
    msr     spsr_cxsf, r3
    ldmfd   sp!, {r0-r12, lr}
    movs    pc, lr

.global undefined_interrupt
undefined_interrupt:
    stmfd   sp!, {r0-r12, lr}
    mrs     r3, spsr
    stmfd   sp!, {r3}
    sub     sp, sp, #8
    mov     r0, sp
    stmia   r0, {sp, lr}^
    nop
    mov     r0, sp

    bl      undefined_int_c

    @ If we return (shouldn't normally), restore and return
    mov     r0, sp
    ldmia   r0, {sp, lr}^
    nop
    add     sp, sp, #8
    ldmfd   sp!, {r3}
    msr     spsr_cxsf, r3
    ldmfd   sp!, {r0-r12, lr}
    movs    pc, lr

.global switch_to_user_mode_asm
switch_to_user_mode_asm:
    push    {r0, r1, r2, r3, lr}

    mov     r0, sp

    mrs     r2, cpsr
    bic     r3, r2, #0x1f
    orr     r3, r3, #0x1f
    msr     cpsr_c, r3

    mov     sp, r0

    bic     r3, r3, #0x1f
    orr     r3, r3, #0x10
    msr     cpsr_c, r3

    pop     {r0, r1, r2, r3, pc}

.global _force_user_exit
_force_user_exit:
    swi     1


.globl drop_to_user                                                                                  
drop_to_user:                                                                                      
    /* r0 = user_sp, r1 = user_pc (standard calling convention) */

    /* enter SYS mode to set sp_usr */
    mrs    r2, cpsr
    bic    r2, r2, #0x1f
    orr    r2, r2, #0x1f
    msr    cpsr_c, r2
    mov    sp, r0

    /* back to SVC */
    mrs    r2, cpsr
    bic    r2, r2, #0x1f
    orr    r2, r2, #0x13
    msr    cpsr_c, r2

    /* prepare SPSR for user mode */
    mrs    r3, cpsr
    bic    r3, r3, #0x1f
    orr    r3, r3, #0x10
    msr    spsr_cxsf, r3

    /* r0 = 0 per ELF ABI */
    mov    lr, r1
    mov    r0, #0

    /* exception return to user */
    movs   pc, lr
