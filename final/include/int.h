#ifndef INT_H
#define INT_H
#include "types.h"

void interrupts_table_init(void);
void set_irq_enabled(bool enabled);
void syscall_yield(void);
void syscall_exit(void);

#endif