#include <stdint.h>
#include <stdio.h>

// BCM2835 GPIO registers
#define GPIO_BASE 0x20200000
#define GPFSEL0 (GPIO_BASE + 0x0000)
#define GPSET0 (GPIO_BASE + 0x001C)
#define GPCLR0 (GPIO_BASE + 0x0028)

#define GPIO_PIN 27 // GPIO17

void gpio_set_output(unsigned pin)
{
    unsigned base = (GPIO_BASE + 0x00);
    unsigned fsel = base + (pin / 10) * 4;
    unsigned value = *(volatile uint32_t *)fsel;
    value &= ~(0b111 << ((pin % 10) * 3));
    value |= 1 << (pin % 10) * 3;
    *(volatile uint32_t *)fsel = value;
}

void gpio_set(int pin, int value)
{
    unsigned base = (GPIO_BASE + 0x1C);
    unsigned set = base + (pin / 32) * 4;
    unsigned clr = set - 0x1C + 0x28;
    value = 0;
    value |= 1 << (pin % 32);
    *(volatile uint32_t *)set = value;
}

void gpio_unset(int pin, int value)
{
   unsigned base = (GPIO_BASE + 0x28) ;
    unsigned set = base + (pin / 32) * 4;
 value = 0;
    value |= 1 << (pin % 32);
    *(volatile uint32_t *)set = value;
}

void sleep(int seconds)
{
    for (int i = 0; i < seconds; i++)
    {
        for (volatile int j = 0; j < 1000000; j++)
        {
        }
        // Busy wait
    }
}

int main(void)
{
    // gpio_set_output(GPIO_PIN);

    // while (1)
    // {
    //     gpio_set(GPIO_PIN, 1); // LED on
    //     sleep(1);
    //     gpio_unset(GPIO_PIN, 0); // LED off
    //     sleep(1);
    // }
    printf("Hello, World!\n");

    return 0;
}