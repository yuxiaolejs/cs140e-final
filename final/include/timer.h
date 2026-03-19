#ifndef TIMER_H
#define TIMER_H
#include "types.h"
#include "serial.h"
#include "cstr.h"
#include "pictl.h"

typedef enum timer_prescaler
{
    TIMER_PRE_1 = 0,
    TIMER_PRE_16 = 0b01,
    TIMER_PRE_256 = 0b10
} timer_prescaler_t;

void enable_timer(uint32_t intv, timer_prescaler_t pre);
void disable_timer(void);

#endif // TIMER_H