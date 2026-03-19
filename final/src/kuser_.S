
@ .section .kuser, "ax", %progbits
.balign 4096 @ 4KB page alignment (2^12)

.global __kuser_section_start__
__kuser_section_start__:
    b   . @ optional: trap if executed at base

    /*
     * Linux kuser helper offsets within the vectors page:
     *   __kuser_memory_barrier : 0xffff0fa0
     *   __kuser_cmpxchg        : 0xffff0fc0
     *   __kuser_get_tls        : 0xffff0fe0
     *
     * We implement them as SVC stubs so we don't touch user IRQ state.
     */

.org 0x0FA0
.global __kuser_memory_barrier
__kuser_memory_barrier:
    svc #0xA0 @ kuser op id (you choose)
    bx  lr

.org 0x0FC0
.global __kuser_cmpxchg
__kuser_cmpxchg:               @ r0=old, r1=new, r2=ptr
    svc #0xA1                  @ kuser op id
    bx  lr

.org 0x0FE0
.global __kuser_get_tls
__kuser_get_tls:
    svc #0xA2 @ kuser op id
    bx  lr

.org 0x1000
.global __kuser_section_end__
__kuser_section_end__:
    nop