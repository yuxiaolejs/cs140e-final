#include "sched.h"
#include "cstr.h"
#include "pictl.h"
#include "flags.h"
#include "vm.h"
#include "ll.h"
#include "tty.h"

// Assembly functions for returning to user mode
extern void svc_generic_return(saved_context_t *ctx);
extern void auto_generic_return(saved_context_t *ctx);

static rpi_thread_t root_t;
rpi_thread_fifo *ready_q = 0;
rpi_sleeping_thread_fifo *sleeping_q = 0;
rpi_thread_t *cur = &root_t;

#define PREEMPTIVE_KERNEL 1

#define INIT_TSTACK_ADDR 0x1100000
#define INCR_TSTACK_ADDR 0x4000
#define ARM_MODE_USR 0x10
#define ARM_MODE_SVC 0x13
#define ARM_MODE_IRQ 0x12
#define ARM_MODE_FIQ 0x11
#define ARM_MODE_ABT 0x17
#define ARM_MODE_UND 0x1B
#define ARM_MODE_SYS 0x1F

uint32_t last_stack_addr = INIT_TSTACK_ADDR;
uint32_t tid = 2;

static rpi_thread_t *t_pop()
{
    if (ready_q)
    {
        rpi_thread_t *t = ready_q->t;
        rpi_thread_fifo *old = ready_q;
        ready_q = ready_q->next;
        kfree(old);
        return t;
    }
    return 0;
}
static void t_push(rpi_thread_t *x)
{
    rpi_thread_fifo *o = kalloc(sizeof(rpi_thread_fifo));
    o->next = 0;
    o->t = x;
    if (!ready_q)
    {
        if (DEBUG_SCHED)
            printk("Ready q now has stuff\n\n");
        ready_q = o;
        return;
    }
    rpi_thread_fifo *c = ready_q;
    while (c->next)
        c = c->next;
    c->next = o;
}

static rpi_sleeping_thread_t *s_pop()
{
    if (ready_q)
    {
        rpi_sleeping_thread_t *t = sleeping_q->s;
        rpi_sleeping_thread_fifo *old = sleeping_q;
        sleeping_q = sleeping_q->next;
        kfree(old);
        return t;
    }
    return 0;
}
static void s_push(rpi_sleeping_thread_t *x)
{
    rpi_sleeping_thread_fifo *o = kalloc(sizeof(rpi_sleeping_thread_fifo));
    o->next = 0;
    o->s = x;
    if (!sleeping_q)
    {
        sleeping_q = o;
        return;
    }
    rpi_sleeping_thread_fifo *c = sleeping_q;
    while (c->next)
        c = c->next;
    c->next = o;
}

void dump_saved_context(saved_context_t *sc)
{
    printk("  ctx.resume_pc: %x\n", sc->resume_pc);
    printk("  ctx.user_sp  : %x\n", sc->user_sp);
    printk("  ctx.user_lr  : %x\n", sc->user_lr);
    printk("  ctx.cpsr     : %x\n", sc->cpsr);
    for (int i = 0; i < 13; i++)
    {
        printk("  R%d    : %x\n", i, sc->regs[i]);
    }
}

void cs_do_sched(saved_context_t *saved_sp)
{
    uint32_t lr;
    asm volatile("mov %0, lr" : "=r"(lr));
    if (DEBUG_SCHED)
        printk("=== In scheduler ===, from TID=%d, caller LR=%x\n", cur->tid, lr);
    if (DEBUG_SCHED > 1)
        printk("++Before ctx sw from %d\n", cur->tid);

    // Only copy if saved_sp is not already &cur->ctx (e.g., from IRQ handler)
    if (saved_sp != &cur->ctx)
        cur->ctx = *saved_sp;
    rpi_thread_t *next = t_pop();
    if (next && !next->exited && next->ready) // FIXME: force no scheduling for now to test syscall sleep
    {
        if (DEBUG_SCHED > 1)
            dump_saved_context(saved_sp);
        if (DEBUG_SCHED)
            printk("Switching from TID=%d to TID=%d, new cpsr=%x\n", cur->tid, next->tid, next->ctx.cpsr);
        if (!cur->exited && cur->ready)
            t_push(cur);
        if (DEBUG_SCHED)
            printk("Page table used by current TID=%d: %x\n", cur->tid, (uint32_t)cur->l1_pt);
        cur = next;
        if (DEBUG_SCHED)
            printk("Page table switch to %x for TID=%d\n", (uint32_t)cur->l1_pt, cur->tid);
        // invalidate buffers
        switch_L1_page_table((uint32_t)cur->l1_pt, cur->tid);
        // Restore TLS base for the new thread
        asm volatile("mcr p15, 0, %0, c13, c0, 3" : : "r"(cur->tls_base) : "memory");
        if (DEBUG_SCHED)
            printk(" -- resume PC: %x, user_lr: %x\n", cur->ctx.resume_pc, cur->ctx.user_lr);
        if (DEBUG_SCHED > 1)
            printk("--After ctx sw to %d\n", cur->tid);
        if (DEBUG_SCHED > 1)
            dump_saved_context(&cur->ctx);
    }
    else
    {
        if (DEBUG_SCHED)
            printk("Scheduler found no thread to switch to, continuing with TID=%d\n", cur->tid);
        if (DEBUG_SCHED)
            printk(" -- resume PC: %x, user_lr: %x\n", cur->ctx.resume_pc, cur->ctx.user_lr);
        if (DEBUG_SCHED > 1)
            dump_saved_context(&cur->ctx);
        if (cur->exited || !cur->ready)
        {
            if (DEBUG_SCHED)
                printk("But current thread TID=%d has exited or is not ready, checking for sleeping stuff\n", cur->tid);
            while (sleeping_q && !ready_q)
            {
                // printk("There are sleeping threads, going to sleep until next timer tick\n");
                asm volatile("cpsie i");
                asm volatile("wfi");
            }
            // either we have ready threads now, or none at all
            if (ready_q)
            {
                // here we need to turn off IRQs to avoid race condition
                asm volatile("cpsid i");
                if (DEBUG_SCHED)
                    printk("There are still threads ready, next tick we run them\n");
                cs_do_sched(saved_sp);
                return;
            }
            printk("Kernel panic - not syncing: Attempted to kill init!\n");
            rpi_reboot();
            while (1)
                ;
        }
    }
    if (DEBUG_SCHED)
        printk("=== Leaving scheduler, now running TID=%d ===\n", cur->tid);
    auto_generic_return(&cur->ctx);
}

void cs_svc_sleep(uint32_t ticks)
{
    // you know what, let's do this wrong implementation first
    // we will need to save the kernel stack to switch in middle of svc
    // so we don't do it now, sleepq is not functional yet

    cur->ready = false;
    rpi_sleeping_thread_t *s = kalloc(sizeof(rpi_sleeping_thread_t));
    s->t = cur;
    s->after_how_many_ticks = ticks;
    s_push(s);
    if (DEBUG_SCHED)
        printk("Sleeping and resume pc will be %x\n", cur->ctx.resume_pc);
    cs_do_sched(&cur->ctx);

    // last resort: if still not ready, just wait for next interrupt
    if (!cur->ready)
    {
        printk("Thread TID=%d still not ready after sleep, waiting for interrupt\n", cur->tid);
        asm volatile("cpsie i");
        asm volatile("wfi");
    }
}

void tick_try_to_wake_up()
{
    // should be called every timer tick, try to wake up sleeping threads
    // printk("Tick: checking sleeping threads to wake up\n");
    rpi_sleeping_thread_fifo *prev = 0;
    rpi_sleeping_thread_fifo *curr = sleeping_q;
    while (curr)
    {
        curr->s->after_how_many_ticks--;
        if (curr->s->after_how_many_ticks == 0)
        {
            // wake up
            curr->s->t->ready = true;
            // printk("Waking up and resume pc will be %x\n", curr->s->t->ctx.resume_pc);
            if (DEBUG_SCHED)
                printk("Waking up thread TID=%d from sleep\n", curr->s->t->tid);
            if (DEBUG_SCHED)
                dump_saved_context(&curr->s->t->ctx);
            t_push(curr->s->t);
            // remove from sleeping queue
            if (prev)
                prev->next = curr->next;
            else
                sleeping_q = curr->next;
            rpi_sleeping_thread_fifo *to_free = curr;
            curr = curr->next;
            kfree(to_free->s);
            kfree(to_free);
            continue;
        }
        prev = curr;
        curr = curr->next;
    }
}

int see_if_pid_still_exists(uint32_t pid)
{
    // check current thread
    if (cur->tid == pid && !cur->exited)
        return 1;
    // check ready queue
    rpi_thread_fifo *rq = ready_q;
    while (rq)
    {
        if (rq->t->tid == pid && !rq->t->exited)
            return 1;
        rq = rq->next;
    }
    // check sleeping queue
    rpi_sleeping_thread_fifo *sq = sleeping_q;
    while (sq)
    {
        if (sq->s->t->tid == pid && !sq->s->t->exited)
            return 1;
        sq = sq->next;
    }
    return 0; // not found
}

int number_of_childs(uint32_t ppid)
{
    int count = 0;
    // check current thread
    if (cur->ppid == ppid && !cur->exited)
        count++;
    // check ready queue
    rpi_thread_fifo *rq = ready_q;
    while (rq)
    {
        if (rq->t->ppid == ppid && !rq->t->exited)
            count++;
        rq = rq->next;
    }
    // check sleeping queue
    rpi_sleeping_thread_fifo *sq = sleeping_q;
    while (sq)
    {
        if (sq->s->t->ppid == ppid && !sq->s->t->exited)
            count++;
        sq = sq->next;
    }
    return count;
}

__attribute__((naked)) static inline void debug_current()
{
    uint32_t lr, sp, pc, cpsr;
    asm volatile("mov %0, lr" : "=r"(lr));
    asm volatile("mov %0, sp" : "=r"(sp));
    asm volatile("mrs %0, cpsr" : "=r"(cpsr));
    asm volatile("adr %0, ." : "=r"(pc));
    uint32_t r0, r1, r2, r3, r4;
    asm volatile("mov %0, r0" : "=r"(r0));
    asm volatile("mov %0, r1" : "=r"(r1));
    asm volatile("mov %0, r2" : "=r"(r2));
    asm volatile("mov %0, r3" : "=r"(r3));
    asm volatile("mov %0, r4" : "=r"(r4));
    printk("In debug_current:\n");
    printk("  Current thread TID=%d\n", cur->tid);
    printk("  R0:   %x\n", r0);
    printk("  R1:   %x\n", r1);
    printk("  R2:   %x\n", r2);
    printk("  R3:   %x\n", r3);
    printk("  R4:   %x\n", r4);
    printk("  PC:   %x\n", pc);
    printk("  LR:   %x\n", lr);
    printk("  SP:   %x\n", sp);
    printk("  CPSR: %b\n", cpsr);
}

rpi_thread_t *rpi_cur_thread()
{
    return cur;
}

void rpi_exit(saved_context_t *ctx)
{
    if (DEBUG_SCHED)
        printk("Thread TID=%d exiting\n", cur->tid);
    cur->exited = true;
    // page table clean up - free all user pages
    free_L1_page_table(cur->l1_pt); // free old page table (avoid kernel memory leak)
    cs_do_sched(ctx);
    printk("Reached unreachable point after exit\n");
    rpi_reboot();
}

void rpi_fork(saved_context_t *ctx)
{
    printk(">>Forking thread TID=%d, MY PC=%x\n", cur->tid, cur->ctx.resume_pc);
    rpi_thread_t *new_t = kalloc(sizeof(rpi_thread_t));
    memcpy(new_t, cur, sizeof(rpi_thread_t));
    memcpy(&new_t->ctx, ctx, sizeof(saved_context_t)); // here we copy the passed context
    new_t->tid = tid++;
    new_t->exited = false;
    new_t->ready = true;
    new_t->l1_pt = fork_page_table(cur->l1_pt);
    cur->ctx.regs[0] = new_t->tid; // return value in r0 for caller
    new_t->ctx.regs[0] = 0;        // return value in r0 for child
    new_t->ppid = cur->tid;
    new_t->fdt = init_fdtable(); // new fdtable for the child
    memcpy(new_t->fdt, cur->fdt, sizeof(struct fdtable)); // copy fdtable entries (shallow copy, they still point to same file structs)
    printk(">>RESUME PC FOR NEW THREAD: %x\n", new_t->ctx.resume_pc);
    if (DEBUG_SCHED)
        printk("Forked new thread TID=%d (user_sp=%x, user_lr=%x, cpsr=%x)\n",
               new_t->tid, new_t->ctx.user_sp, new_t->ctx.user_lr, new_t->ctx.cpsr);
    t_push(new_t);
}
