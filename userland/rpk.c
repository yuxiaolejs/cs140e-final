#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

static inline int replace_kernel_call(const char *kernel_file, uint32_t load_address)
{
    int ret;

    asm volatile (
        "mov r0, %1\n"
        "mov r1, %2\n"
        "svc 3\n"
        "mov %0, r0\n"
        : "=r"(ret)
        : "r"(kernel_file), "r"(load_address)
        : "memory"
    );

    return ret;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Usage: %s <kernel_file> <load_address>\n", argv[0]);
        return 1;
    }
    const char *kernel_file = argv[1];
    uint32_t load_address = (uint32_t)strtoul(argv[2], NULL, 0);
    printf("Replacing kernel with file '%s' at address 0x%x\n", kernel_file, load_address);
    replace_kernel_call(kernel_file, load_address);
    // Should never reach here
    printf("Error: kernel_replace_file syscall returned\n");
    return 1;
}