#include "kuser.h"
#include "mem.h"
#include "cstr.h"
#include "pictl.h"

extern uint32_t __kuser_section_start__[];
extern uint32_t __kuser_section_end__[];

uint32_t *kuser_init()
{
    printk("Kuser section from %x to %x\n", (uint32_t)__kuser_section_start__, (uint32_t)__kuser_section_end__);
    uint32_t *kuser_page = kalloc(4096);
    memcpy(kuser_page, __kuser_section_start__, (uint32_t)__kuser_section_end__ - (uint32_t)__kuser_section_start__);
    return kuser_page;
}

void kuser_verify()
{
    uint32_t *kuser_addr = (uint32_t *)0xFFFF0000;
    if (*kuser_addr != 0xeafffffe)
    {
        printk("Kuser verification failed! Expected NOP at 0xffff0000, found %x\n", *kuser_addr);
        rpi_reboot();
    }
    else
    {
        printk("Kuser verification succeeded! NOP found at 0xffff0000\n");
    }
}

#include <stdint.h>

enum
{
    SWI_KUSER_MB = 0xA0,
    SWI_KUSER_CMPXCHG = 0xC0,
    SWI_KUSER_GET_TLS = 0xE0,
};


static inline uint32_t irq_save_disable(void)
{
    uint32_t cpsr;
    __asm__ volatile(
        "mrs %0, cpsr\n"
        "orr r12, %0, #(1<<7)\n" // set I bit (mask IRQ)
        "msr cpsr_c, r12\n"
        : "=r"(cpsr)
        :
        : "r12", "memory", "cc");
    return cpsr;
}

static inline void irq_restore(uint32_t cpsr)
{
    __asm__ volatile(
        "msr cpsr_c, %0\n"
        :
        : "r"(cpsr)
        : "memory", "cc");
}


static inline void dmb_armv6(void)
{
    __asm__ volatile(
        "mov r12, #0\n"
        "mcr p15, 0, r12, c7, c10, 5\n"
        :
        :
        : "r12", "memory");
}


void kuser_cmpxchg(saved_context_t *ctx)
{
    uint32_t oldv = ctx->regs[0];
    uint32_t newv = ctx->regs[1];
    volatile uint32_t *ptr = (volatile uint32_t *)(uintptr_t)ctx->regs[2];

    // Validate user pointer
    if ((uint32_t)ptr < 0x10000 || (uint32_t)ptr > 0xc0000000) {
        ctx->regs[0] = 1; // fail
        return;
    }

    uint32_t cpsr = irq_save_disable();

    uint32_t cur = *ptr;
    if (cur == oldv)
    {
        *ptr = newv;
        ctx->regs[0] = 0;
    }
    else
    {
        ctx->regs[0] = 1;
    }

    dmb_armv6();

    irq_restore(cpsr);
}


void kuser_memory_barrier(saved_context_t *ctx)
{
    dmb_armv6();
    ctx->regs[0] = 0;
}

static inline uintptr_t kernel_get_current_tls_base(void)
{
    uintptr_t tls;
    __asm__ volatile(
        "mrc p15, 0, %0, c13, c0, 3\n"
        : "=r"(tls)
        :
        : "memory");
    return tls;
}

void kuser_get_tls(saved_context_t *ctx)
{
    ctx->regs[0] = (uint32_t)kernel_get_current_tls_base();
}