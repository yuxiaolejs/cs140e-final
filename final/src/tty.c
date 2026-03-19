#include "tty.h"
#include "types.h"
#include "mem.h"
#include "pictl.h"
#include "terminal.h"
#include "keyboard.h"

static char tty_in_buf[128]; // stupidest fifo ever
static char tty_out_buf[128];

struct file *fd0, *fd1, *fd2;

static uint8_t avail_in = 0;
static uint8_t avail_out = 0;
static termios_t tty_ctrl;
void tty_init()
{
    memset(tty_in_buf, 0, sizeof(tty_in_buf));
    memset(tty_out_buf, 0, sizeof(tty_out_buf));
    avail_in = 0;
    avail_out = 0;
    // We also need to create file structs for fd0,1,2
    fd0 = kalloc(sizeof(struct file));
    fd0->refcnt = -1; // never freed
    fd0->flags = 0;   // O_RDONLY
    fd0->pos = 0;
    fd0->priv = 0;
    struct file_ops *tty_fops = kalloc(sizeof(struct file_ops));
    tty_fops->read = tty_read;
    tty_fops->write = tty_write;
    tty_fops->ioctl = tty_ioctl;
    tty_fops->lseek = tty_lseek;
    tty_fops->close = tty_close;
    tty_fops->fcntl = tty_fcntl;
    fd0->ops = tty_fops;
    fd1 = kalloc(sizeof(struct file));
    memcpy(fd1, fd0, sizeof(struct file));
    fd1->flags = 1; // O_WRONLY
    fd2 = kalloc(sizeof(struct file));
    memcpy(fd2, fd0, sizeof(struct file));
    fd2->flags = 1; // O_WRONLY

    // ctl
    memset(&tty_ctrl, 0, sizeof(tty_ctrl));
    tty_ctrl.c_lflag = TERMIOS_LF_ECHO | TERMIOS_LF_ISIG | TERMIOS_LF_ICANON;
}

void tty_system_tty_input(char c)
{
    if (avail_in < sizeof(tty_in_buf))
        tty_in_buf[avail_in++] = c;
    else
        printk("TTY input buffer overflow, char lost: %c\n", c);
}

char tty_system_tty_output_getchar()
{
    if (avail_out == 0)
        return 0; // no data
    char c = tty_out_buf[0];
    avail_out--;
    memmove_fast(tty_out_buf, tty_out_buf + 1, avail_out);
    return c;
}

uint8_t tty_system_tty_output_available()
{
    return avail_out;
}

uint8_t tty_system_tty_input_available()
{
    return avail_in;
}

static int tty_find_newline()
{
    for (uint8_t i = 0; i < avail_in; i++)
        if (tty_in_buf[i] == '\n' || tty_in_buf[i] == '\r')
            return i;
    return -1;
}

ssize_t tty_read(struct file *f, void *buf, size_t n)
{
    if (tty_ctrl.c_lflag & TERMIOS_LF_ICANON)
    {
        // canonical mode
        int nl;
        while ((nl = tty_find_newline()) < 0)
        {
            asm volatile("cpsie i");
            asm volatile("wfi");
        }
        size_t line_len = nl + 1;
        size_t to_copy = line_len < n ? line_len : n;
        memcpy(buf, tty_in_buf, to_copy);
        avail_in -= line_len;
        memmove_fast(tty_in_buf, tty_in_buf + line_len, avail_in);
        return to_copy;
    }

    // raw mode
    while (avail_in < n)
    // wait for more data
    {
        // printk("[blocked] TTY READ: waiting for data, have %d, need %d\n", avail_in, n);
        asm volatile("cpsie i");
        asm volatile("wfi");
        // here we may get context switched out.... how to handle kernel stack? using dummy impl for now
    }

    {
        memcpy(buf, tty_in_buf, n);
        avail_in -= n;
        memmove_fast(tty_in_buf, tty_in_buf + n, avail_in);
        if (n == 0)
            n = -1;
        return n;
    }
}
ssize_t tty_write(struct file *f, const void *buf, size_t n)
{
    if (n > sizeof(tty_out_buf) - avail_out)
        n = sizeof(tty_out_buf) - avail_out;
    memcpy(tty_out_buf + avail_out, buf, n);
    avail_out += n;
    return n;
}
long tty_ioctl(struct file *f, unsigned long req, unsigned long arg)
{
    switch (req)
    {
    case TERMIOS_CMD_TCGETS:
        memcpy((void *)arg, &tty_ctrl, sizeof(termios_t));
        return 0;
    case TERMIOS_CMD_TCSETS:
    case TERMIOS_CMD_TCSETSW:
    case TERMIOS_CMD_TCSETSF:
        memcpy(&tty_ctrl, (void *)arg, sizeof(termios_t));
        return 0;
    case TERMIOS_CMD_TIOCGWINSZ:
    {
        struct winsize *ws = (struct winsize *)arg;
        ws->ws_row = 28; // or terminal height
        ws->ws_col = 79; // or terminal width
        ws->ws_xpixel = 0;
        ws->ws_ypixel = 0;
        return 0;
    }
    default:
        printk("tty_ioctl: unhandled req 0x%x\n", req);
        return 0;
    }
    return 0; // just success
}
long tty_lseek(struct file *f, off_t off, int whence)
{
    return 0; // just success
}
long tty_close(struct file *f)
{
    return 0; // just success
}
long tty_fcntl(struct file *f, unsigned cmd, unsigned long arg)
{
    printk("tty_fcntl called with cmd %u\n", cmd);
    return 0; // just success on everything
}

void tty_system_process_tty()
{
    // First handle USB -> TTY
    uint32_t handled = keyboard_poll_once();
    while (handled != (uint32_t)-1)
    {
        // A key was pressed, do something with it
        // printk("Key pressed: %c\n", (char)handled);
        char c = (char)(handled & 0xFF);

        if (c == 0x08 || c == 0x7F) // backspace or DEL
        {
            if (avail_in > 0)
            {
                avail_in--;
                if (tty_ctrl.c_lflag & TERMIOS_LF_ECHO)
                {
                    terminal_putc('\b');
                    terminal_putc(' ');
                    terminal_putc('\b');
                }
            }
        }
        else
        {
            // Now we add it to tty buffer
            tty_system_tty_input(c);
            // we also echo it (FIXME: this should not be a kernel behavior!!)
            if (tty_ctrl.c_lflag & TERMIOS_LF_ECHO)
                terminal_putc(c);
        }
        handled = keyboard_poll_once();
    }
    // Then do TTY -> screen
    char c;
    while ((c = tty_system_tty_output_getchar()) != 0)
    {
        terminal_putc(c);
    }
}