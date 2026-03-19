#ifndef SERIAL_H
#define SERIAL_H
#include "types.h"
// void libc_serial_puts(char *data);
// void libc_serial_printInt(uint32_t num);
// void libc_serial_wait_till_deadline(uint32_t deadline);
void uart_init();
void uart_putc(char c);
void uart_puts(char *str);
void uart_printInt(uint32_t num);
void uart_getc(char *c);
int uart_getc_timeout(char *c, uint32_t timeout_ms);
void uart_gets(char *buf, int maxlen);
void uart_wait_tx(void);
#endif