#ifndef TTY_H
#define TTY_H
#include "fd.h"
#include "termios.h"
void tty_init();
ssize_t tty_read(struct file *f, void *buf, size_t n);
ssize_t tty_write(struct file *f, const void *buf, size_t n);
long tty_ioctl(struct file *f, unsigned long req, unsigned long arg);
long tty_lseek(struct file *f, off_t off, int whence);
long tty_close(struct file *f);
long tty_fcntl(struct file *f, unsigned cmd, unsigned long arg);

void tty_system_tty_input(char c);    // for OS components to call when a char is selected from like keyboard
char tty_system_tty_output_getchar(); // for OS components to call to get a char to output

uint8_t tty_system_tty_output_available();
uint8_t tty_system_tty_input_available();

void tty_system_process_tty(); // should be called every tick to handle tty
#endif