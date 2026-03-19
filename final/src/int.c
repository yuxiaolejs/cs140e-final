#include "types.h"
#include "regs.h"
#include "int.h"
#include "cstr.h"
#include "sched.h"
#include "pictl.h"
#include "sched.h"
#include "flags.h"
#include "vm.h"
#include "kuser.h"
#include "keyboard.h"
#include "tty.h"
#include "crc32.h"
#include "krep.h"

static uint32_t debugger_states[32];

extern volatile uint32_t _interrupt_table[];
extern rpi_thread_t *cur;

void _reset_int(uint32_t lr)
{
    printk("Unhandled reset interrupt handler\n");
    printk("LR: %x\n", lr);
    rpi_reboot();
}

void generic_irq_handler(saved_context_t *ctx, uint32_t irq_basic_pending, uint32_t irq_pending1, uint32_t irq_pending2)
{
    // Only save context if we interrupted user mode
    // If we interrupted SVC mode, ctx->user_sp/lr contain SVC's banked regs, not user's!
    if ((ctx->cpsr & 0x1f) == 0x10)
        cur->ctx = *ctx;
    if (irq_basic_pending & (1 << 0))
    {
        // printk("IRQ!!!!\n");
        // Timer IRQ
        tty_system_process_tty(); // process any pending tty input/output

        // Try to wake up
        tick_try_to_wake_up();

        // Finally do scheduling
        if ((ctx->cpsr & 0x1f) == 0x10)
            cs_do_sched(ctx); // only schedule if in user mode
    }
    else if (GET32(ADD_IRQ_PENDING2) & (1 << 17))
    {
        // GPIO IRQ
        for (int i = 0; i < 32; i++)
        {
            if (GET32(ADD_GPEDS) & (1 << i))
            {
                printk("GPIO Pin %d triggered!\n", i);
            }
        }

        PUT32(ADD_GPEDS, 0xFFFFFFFF);
        if (GET32(ADD_IRQ_PENDING2) & (1 << 17))
        {
            printk("GPIO20 event not cleared!\n");
            rpi_reboot();
        }
    }
    auto_generic_return(ctx);
}

// Helper to read IFSR (Instruction Fault Status Register)
static inline uint32_t read_ifsr(void)
{
    uint32_t val;
    asm volatile("mrc p15, 0, %0, c5, c0, 1" : "=r"(val));
    return val;
}

// Helper to read IFAR (Instruction Fault Address Register) - ARM11 specific
static inline uint32_t read_ifar(void)
{
    uint32_t val;
    asm volatile("mrc p15, 0, %0, c6, c0, 2" : "=r"(val));
    return val;
}

void prefetch_abort_int_c(saved_context_t *ctx)
{
    uint32_t ifsr = read_ifsr();
    uint32_t ifar = read_ifar();
  
    force_printk("\n========== PREFETCH ABORT ==========\n");
    force_printk("PC: %x, IFSR: %x, IFAR: %x\n", ctx->resume_pc, ifsr, ifar);
    force_printk("SP: %x, LR: %x\n", ctx->user_sp, ctx->user_lr);
    force_printk("=====================================\n");
    rpi_reboot();
}

void undefined_int_c(saved_context_t *ctx)
{
    force_printk("\n========== UNDEFINED INSTRUCTION ==========\n");
    force_printk("PC: %x\n", ctx->resume_pc);

    uint32_t *pc = (uint32_t *)ctx->resume_pc;
    if ((uint32_t)pc >= 0x10000 && (uint32_t)pc < 0xc0000000)
    {
        force_printk("Instruction: %x\n", *pc);
    }
    force_printk("SP: %x, LR: %x\n", ctx->user_sp, ctx->user_lr);
    force_printk("SPSR: %x\n", ctx->cpsr);
    force_printk("============================================\n");
    rpi_reboot();
}

void swi_int_c(saved_context_t *ctx)
{
    if (DEBUG_INT_HANDLE)
    {
        uint32_t my_sp;
        asm volatile("mov %0, sp" : "=r"(my_sp));
        printk("[SWI] Current SP: %x\n", my_sp);
        dump_saved_context(ctx);
        printk("Curr Resume PC: %x\n", ctx->resume_pc);
    }

    uint32_t swi_number = *(uint32_t *)(ctx->resume_pc - 4) & 0x00FFFFFF;
    if (DEBUG_INT_HANDLE)
        printk("In SWI handler, swi_number=%d\n", swi_number);
    switch (swi_number)
    {
    case 0:
        posix_syscall(ctx);
        break;
    case 1:
        cs_do_sched(ctx);
        break;
    case 2:
        rpi_exit(ctx);
        break;
    case 3:
        ctx->regs[0] = kernel_replace_file((const char *)ctx->regs[0], ctx->regs[1]);
        break;
    case 0xA0:
        kuser_memory_barrier(ctx);
        break;
    case 0xA1:
        kuser_cmpxchg(ctx);
        break;
    case 0xA2:
        kuser_get_tls(ctx);
        break;
    default:
        printk("Unknown SWI number: %d\n", swi_number);
        break;
    }
    auto_generic_return(ctx);
}

void dump_page_table_entry_for_VA(uint32_t va)
{
    // this is debug!
    uint32_t L1_base;
    asm volatile("mrc p15, 0, %0, c2, c0, 0" : "=r"(L1_base));
    uint32_t *L1_table = (uint32_t *)L1_base;
    uint32_t L1_index = (va >> 20) & 0xFFF;
    uint32_t L1_entry = L1_table[L1_index];
    printk("L1 entry for VA %x at index %x, address %x: %x\n", va, L1_index, (uint32_t)&L1_table[L1_index], L1_entry);
    if ((L1_entry & 0b11) == 0b01)
    {
        // Coarse page table
        uint32_t L2_base = L1_entry & 0xFFFFFC00;
        uint32_t *L2_table = (uint32_t *)L2_base;
        uint32_t L2_index = (va >> 12) & 0xFF;
        uint32_t L2_entry = L2_table[L2_index];
        printk("L2 entry for VA %x at index %x, address %x: %x\n", va, L2_index, (uint32_t)&L2_table[L2_index], L2_entry);
    }
}

void data_abort_int_c(saved_context_t *ctx)
{
    // printk("In Data Abort handler\n");
    // dump_saved_context(ctx);
    uint32_t FAR, DFST;
    asm volatile("mrc p15, 0, %0, c6, c0, 0" : "=r"(FAR));  // Fault Address Register
    asm volatile("mrc p15, 0, %0, c5, c0, 0" : "=r"(DFST)); // Data Fault Status Register

    // Check if this is stack growth from user mode
    bool from_user = ((ctx->cpsr & 0x1F) == 0x10);
    bool is_write = DFST & (1 << 11) || 1;
    bool is_xlate = ((DFST & 0xF) == 0x7) || 1;

    if (!(from_user && is_write && is_xlate))
    {
        force_printk("Data abort: PC=%x, FAR=%x, DFSR=%x\n", ctx->resume_pc, FAR, DFST);
        rpi_reboot();
    }

    uint32_t far = FAR;
    uint32_t sp = ctx->user_sp;

    if (!(far <= sp + 128 && far >= sp - 128))
    {
        printk("Data abort: FAR=%x not near SP=%x\n", far, sp);
        printk("Conditions: far <= sp: %d, far >= sp - 128: %d\n", far <= sp, far >= sp - 128);
        force_printk("Curr Resume PC: %x\n", ctx->resume_pc);
        force_printk("Fault instruction: %x\n", ctx->resume_pc - 8);
        force_printk("FAR: %x\n", FAR);
        force_printk("DFSR: %b\n", DFST);
        dump_page_table_entry_for_VA(FAR);
        rpi_reboot();
    }
    // Okay, we are growing the stack
    // printk("Data abort looks like stack growth, FAR=%x, SP=%x\n", far, sp);
    uint32_t new_page = make_L2_page_on_the_fly_and_make_valid(far);
    // printk("Allocated new page at %x for stack growth at VA %x\n", (uint32_t)new_page, far);
    // Now adjust the resume_pc to try again
    ctx->resume_pc -= 8;
    // printk("Adjusted resume PC to %x\n", ctx->resume_pc);
    return;
}

void interrupts_table_init(void)
{
    asm volatile("mcr p15, 0, %0, c12, c0, 0" : : "r"(_interrupt_table) : "memory");
}
void set_irq_enabled(bool enabled)
{
    if (enabled)
        write_cpsr(read_cpsr() & ~(1 << 7)); // Enable IRQ interrupts
    else
        write_cpsr(read_cpsr() | (1 << 7)); // Disable IRQ interrupts
}
