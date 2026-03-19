#include "gpio.h"
#include "regs.h"
void gpio_set_output(uint32_t pin)
{
    uint32_t addr = ADD_GPFSEL + (pin / 10) * 4;
    uint32_t shift = (pin % 10) * 3;
    uint32_t val = GET32(addr);
    val &= ~(0b111 << shift); // Clear bits
    val |= (0b001 << shift);  // Set to output
    PUT32(addr, val);
}
void gpio_set_input(uint32_t pin)
{
    uint32_t addr = ADD_GPFSEL + (pin / 10) * 4;
    uint32_t shift = (pin % 10) * 3;
    uint32_t val = GET32(addr);
    val &= ~(0b111 << shift); // Clear bits
    val |= (0b000 << shift);  // Set to input
    PUT32(addr, val);
}
void gpio_set_on(uint32_t pin)
{
    uint32_t addr = ADD_GPSET + (pin / 32) * 4;
    uint32_t shift = pin % 32;
    PUT32(addr, 1u << shift);
}
void gpio_set_off(uint32_t pin)
{
    uint32_t addr = ADD_GPCLR + (pin / 32) * 4;
    uint32_t shift = pin % 32;
    PUT32(addr, 1u << shift);
}
uint32_t gpio_read(uint32_t pin)
{
    uint32_t addr = ADD_GPLEV + (pin / 32) * 4;
    uint32_t shift = pin % 32;
    uint32_t val = GET32(addr);
    return (val >> shift) & 0x1;
}

void gpio_pin_set_func(uint32_t pin, uint32_t func)
{
    uint32_t addr = ADD_GPFSEL + (pin / 10) * 4;
    uint32_t shift = (pin % 10) * 3;
    uint32_t val = GET32(addr);
    val &= ~(0b111 << shift); // Clear bits
    val |= (func << shift);   // Set to func
    PUT32(addr, val);
}