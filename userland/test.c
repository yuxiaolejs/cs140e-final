// Minimal test - no glibc printf, just raw syscall
void main(void) {
    const char msg[] = "Hello from raw syscall!\n";
    
    // write(1, msg, sizeof(msg)-1) using raw syscall
    register int r0 asm("r0") = 1;  // fd = stdout
    register const char *r1 asm("r1") = msg;
    register int r2 asm("r2") = sizeof(msg) - 1;
    register int r7 asm("r7") = 4;  // __NR_write
    
    asm volatile(
        "svc #0"
        : "+r"(r0)
        : "r"(r1), "r"(r2), "r"(r7)
        : "memory"
    );
    
    // exit(0)
    r0 = 0;
    r7 = 1;  // __NR_exit
    asm volatile(
        "svc #0"
        :
        : "r"(r0), "r"(r7)
    );
    
    // Should never reach here
    while(1);
}
