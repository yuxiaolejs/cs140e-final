#ifndef FD_H
#define FD_H
#include <stddef.h>
#include "types.h"
#include "termios.h"
typedef long ssize_t;
typedef long off_t;

struct file; /* forward declaration */

struct file_ops
{
    ssize_t (*read)(struct file *f, void *buf, size_t n);
    ssize_t (*write)(struct file *f, const void *buf, size_t n);
    long (*ioctl)(struct file *f, unsigned long req, unsigned long arg);
    long (*lseek)(struct file *f, off_t off, int whence);
    long (*close)(struct file *f);
    long (*fcntl)(struct file *f, unsigned cmd, unsigned long arg);
};

struct file
{
    int refcnt;
    int flags;  // O_RDONLY/O_WRONLY/O_RDWR/...
    off_t pos;  // current offset
    void *priv; // points to inode, pipe object, tty state, etc.
    const struct file_ops *ops;
};

typedef struct pollfd
{
    int fd;        /* file descriptor */
    short events;  /* requested events */
    short revents; /* returned events */
} pollfd_t;

#define MAX_FD 64
struct fdtable
{
    struct file *fd[MAX_FD];
};

int alloc_fd(struct fdtable *t, struct file *f);
struct file *fd_get(struct fdtable *t, int fd);
long sys_close(int fd);
long sys_read(int fd, void *buf, size_t n);
long sys_write(int fd, const void *buf, size_t n);
long sys_openat(int dfd, const char *path, int flags, int mode);
int sys_fd_poll(pollfd_t *fds, uint32_t nfds, int timeout);
int sys_ioctl(int fd, unsigned long req, termios_t *arg);
int sys_fcntl(int fd, unsigned cmd, unsigned long arg);

/* open/fcntl.  */
#define O_ACCMODE	   0003
#define O_RDONLY	     00
#define O_WRONLY	     01
#define O_RDWR		     02
#ifndef O_CREAT
# define O_CREAT	   0100	/* Not fcntl.  */
#endif
#ifndef O_EXCL
# define O_EXCL		   0200	/* Not fcntl.  */
#endif
#ifndef O_NOCTTY
# define O_NOCTTY	   0400	/* Not fcntl.  */
#endif
#ifndef O_TRUNC
# define O_TRUNC	  01000	/* Not fcntl.  */
#endif
#ifndef O_APPEND
# define O_APPEND	  02000
#endif
#ifndef O_NONBLOCK
# define O_NONBLOCK	  04000
#endif
#ifndef O_NDELAY
# define O_NDELAY	O_NONBLOCK
#endif
#ifndef O_SYNC
# define O_SYNC	       04010000
#endif
#define O_FSYNC		O_SYNC
#ifndef O_ASYNC
# define O_ASYNC	 020000
#endif
#endif