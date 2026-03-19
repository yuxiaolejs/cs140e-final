static inline int syscall3(int num, int arg1, int arg2, int arg3)
{
    int ret;

    asm volatile (
        "mov r7, %1\n"
        "mov r0, %2\n"
        "mov r1, %3\n"
        "mov r2, %4\n"
        "svc 0\n"
        "mov %0, r0\n"
        : "=r"(ret)
        : "r"(num), "r"(arg1), "r"(arg2), "r"(arg3)
        : "memory"
    );

    return ret;
}

int main()
{
    const char *msg = "Hello from user space!\n";
    syscall3(4, 1, msg, 23); // write char to stdout
}

// void _start()
// {
//     main();
//     syscall3(1, 0, 0, 0); // exit
// }