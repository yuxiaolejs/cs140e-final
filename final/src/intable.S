@ This is the interrupt vector table
.global _interrupt_table
.balign 32
_interrupt_table:
  b _reset_int_stub
  b undefined_interrupt
  b swi_interrupt
  b prefetch_abort_interrupt
  b data_abort_interrupt
  b _reset_int_stub
  b irq_timer_interrupt
fast_interrupt_asm:
  sub lr, lr, #4 @First instr of FIQ handler
  push {lr}
  push {r0-r12}
  mov r0, lr @ Pass old pc
  bl _reset_int_stub @ C function
  pop {r0-r12}
  ldm sp!, {pc}^
_reset_int_stub:
  mov sp, #0x80000 @ Switch to safe stack
  mov r0, lr
  bl _reset_int
