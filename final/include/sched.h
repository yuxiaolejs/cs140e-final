#ifndef SCHED_H
#define SCHED_H
#include "types.h"
#include "mem.h"
#include "fd.h"

extern void switch_to_user_mode_asm();
extern void _force_user_exit();

#define trace(fmt, ...) \
    printk("TRACE:%s:" fmt, __func__, ##__VA_ARGS__)
#define output trace

typedef struct saved_context
{
    uint32_t user_sp;
    uint32_t user_lr;
    uint32_t cpsr;
    uint32_t regs[13]; // r0-r12 at offsets 0-48
    uint32_t resume_pc;
} saved_context_t;

typedef struct
{
    uint32_t tid;
    uint8_t exited;
    uint8_t ready;
    saved_context_t ctx;
    uint32_t *l1_pt;
    uint32_t *tls_base;
    uint32_t brk;
    uint32_t brk_min;
    uint32_t *clear_tid;
    struct fdtable *fdt;
    char *cwd;
    uint32_t *svc_sp; // for saved context on svc
    uint32_t ppid;

} rpi_thread_t;

typedef struct rpi_thread_fifo
{
    rpi_thread_t *t;
    struct rpi_thread_fifo *next;
} rpi_thread_fifo;

typedef struct
{
    rpi_thread_t *t;
    uint32_t after_how_many_ticks;

} rpi_sleeping_thread_t;

typedef struct rpi_sleeping_thread_fifo
{
    rpi_sleeping_thread_t *s;
    struct rpi_sleeping_thread_fifo *next;
} rpi_sleeping_thread_fifo;

rpi_thread_t *rpi_cur_thread();
void rpi_thread_start();
void rpi_fork(saved_context_t *ctx);
void rpi_exit(saved_context_t *ctx);
void *kmalloc(uint32_t x);
void cs_do_sched(saved_context_t *saved_sp);
void cs_svc_sleep(uint32_t ticks);
int number_of_childs(uint32_t ppid);
#endif