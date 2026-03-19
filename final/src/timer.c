#include "timer.h"

extern volatile uint32_t _interrupt_table[];
extern volatile uint32_t _interrupt_table_end[];

volatile unsigned cnt, period, period_sum;

void enable_timer(uint32_t intv, timer_prescaler_t pre)
{
    *PTR_TIMER_LOAD = intv;
    // *PTR_TIMER_PRE_DIVIDER = 128;
    uint32_t ctrl = 0;
    ctrl |= (1 << 7);   // Timer enable
    ctrl |= (1 << 5);   // Interrupt enable
    ctrl |= (pre << 2); // 256 prescaler
    ctrl |= (1 << 1);   // 32-bit mode
    *PTR_TIMER_CONTROL = ctrl;
}

void disable_timer(void)
{
    uint32_t ctrl = *PTR_TIMER_CONTROL;
    ctrl &= ~(1 << 7); // Disable timer
    *PTR_TIMER_CONTROL = ctrl;
}