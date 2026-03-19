#ifndef KUSER_H
#define KUSER_H
#include "sched.h"
#include "types.h"
uint32_t *kuser_init();
void kuser_verify();
void kuser_get_tls(saved_context_t *ctx);
void kuser_memory_barrier(saved_context_t *ctx);
void kuser_cmpxchg(saved_context_t *ctx);
#endif