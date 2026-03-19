#include "fd.h"
#include "mem.h"
#include "sched.h"
#include "mount.h"
#include "cstr.h"
#include <errno.h>

extern struct file *fd0, *fd1, *fd2;
extern rpi_thread_t *cur;

struct fdtable *init_fdtable()
{
    struct fdtable *t = kalloc(sizeof(struct fdtable));
    memset(t, 0, sizeof(struct fdtable));
    // now we need to putin fd 0,1,2 the tty files
    t->fd[0] = fd0;
    t->fd[1] = fd1;
    t->fd[2] = fd2;
    return t;
}

int alloc_fd(struct fdtable *t, struct file *f)
{
    for (int i = 0; i < MAX_FD; i++)
    {
        if (t->fd[i] == 0)
        {
            t->fd[i] = f;
            return i;
        }
    }
    return -24; // no space
}
struct file *fd_get(struct fdtable *t, int fd)
{
    if (fd < 0 || fd >= MAX_FD)
        return 0;
    return t->fd[fd];
}
long sys_close(int fd)
{
    struct file *f = fd_get(cur->fdt, fd);
    if (!f)
        return -9; // -EBADF
    cur->fdt->fd[fd] = 0;

    if (--f->refcnt == 0)
    {
        if (f->ops && f->ops->close)
            f->ops->close(f);
        kfree(f);
    }
    return 0;
}
long sys_read(int fd, void *buf, size_t n)
{
    // printk("Trying to get current fdt for sys_read: %x\n", (uint32_t)cur->fdt->fd[0]->ops);
    struct file *f = fd_get(cur->fdt, fd);
    // printk("Got file struct, ops at %x\n", (uint32_t)f->ops);
    if (!f || !f->ops || !f->ops->read)
    {
        if (!f)
            printk("sys_read: no file for fd %d\n", fd);
        else if (!f->ops)
            printk("sys_read: no ops for fd %d, file is at %x\n, and ops is at %x", fd, (uint32_t)f, (uint32_t)f->ops);
        else if (!f->ops->read)
            printk("sys_read: no read op for fd %d\n", fd);
        printk("fyi: current fdt at %x\n", (uint32_t)cur->fdt);
        printk("  fd0=%x, fd1=%x, fd2=%x\n",
               (uint32_t)cur->fdt->fd[0],
               (uint32_t)cur->fdt->fd[1],
               (uint32_t)cur->fdt->fd[2]);
        return -9;
    }
    ssize_t ret = f->ops->read(f, buf, n);
    // printk("Read returned %d\n", ret);
    return ret;
}
long sys_write(int fd, const void *buf, size_t n)
{
    struct file *f = fd_get(cur->fdt, fd);
    if (!f || !f->ops || !f->ops->write)
        return -9;
    return f->ops->write(f, buf, n);
}
long sys_openat(int dfd, const char *path, int flags, int mode)
{
    // for fat32 fs
    // need to by pass everything with /dev
    if (path && strn_eq(path, "/dev", 4))
    {
        printk("sys_openat: /dev not supported yet\n");
        return -ENOSYS;
    }
    // force_printk("  Trying mount openat %s with flags %x\n", path ? path : "(null)", flags);
    if (flags & O_CREAT)
    {
        printk("  O_CREAT flag set\n");
    }
    if (flags & O_TRUNC)
    {
        printk("  O_TRUNC flag set\n");
    }
    uint32_t clus = mount_openat(dfd, path, flags, mode);
    if ((int32_t)clus < 0)
        return clus; // error code
    printk("  mount_openat succeeded, parent cluster %u\n", clus);
    fat_dirent_t *de = mount_get_dirent(path, dfd == AT_FDCWD, 0);
    if (!de)
    {
        force_printk("sys_openat: file '%s' not found after mount_openat succeeded\n", path ? path : cur->cwd);
        return -EINVAL;
    }
    fat_dirent_t *pd = mount_get_dirent(path, dfd == AT_FDCWD, 1);
    if (!pd)
    {
        force_printk("sys_openat: parent dir for '%s' not found\n", path ? path : cur->cwd);
    }
    printk("  got dirent for '%s', file size %u bytes\n", path ? path : cur->cwd, de->file_size);

    struct file *f = kalloc(sizeof(*f));
    f->refcnt = 1;
    f->flags = flags;
    f->pos = 0;
    open_file_priv_t *priv = kalloc(sizeof(open_file_priv_t));
    priv->dirent = de;
    priv->parent_dirent = pd; // not used yet
    f->priv = priv;           // FIXME
    struct file_ops *ops = kalloc(sizeof(struct file_ops));
    ops->read = mount_read;
    ops->write = mount_write;
    ops->lseek = mount_lseek;
    ops->close = mount_close;
    ops->ioctl = mount_ioctl;
    ops->fcntl = mount_fcntl;
    f->ops = ops;
    int fd = alloc_fd(cur->fdt, f);
    if (fd < 0)
    {
        kfree(f);
        return fd;
    }
    return fd;
}

int sys_fd_poll(pollfd_t *fds, uint32_t nfds, int timeout)
{
    // printk("poll called: nfds=%u, timeout=%d\n", nfds, timeout);
    (void)timeout;
    int ready = 0;
    for (uint32_t i = 0; i < nfds; i++)
    {
        // printk("  fd %d, events %d\n", fds[i].fd, fds[i].events);
        fds[i].revents = 0;
        if (fds[i].fd == 0 && tty_system_tty_input_available() != 0) {
            fds[i].revents |= 0x001;
            ready++;
        }
    }
    if (ready > 0 || timeout >= 0)
        return ready;
    cur->ctx.resume_pc -= 4;
    cs_svc_sleep(1);
    return nfds;
}

int sys_ioctl(int fd, unsigned long req, termios_t *arg)
{
    struct file *f = fd_get(cur->fdt, fd);
    if (!f || !f->ops || !f->ops->ioctl)
        return -9;
    return f->ops->ioctl(f, req, (unsigned long)arg);
}

int sys_fcntl(int fd, unsigned cmd, unsigned long arg)
{
    struct file *f = fd_get(cur->fdt, fd);
    if (!f || !f->ops || !f->ops->fcntl)
        return -9;
    return f->ops->fcntl(f, cmd, arg);
}

int sys_dup2(int oldfd, int newfd)
{
    if (oldfd < 0 || oldfd >= MAX_FD || newfd < 0 || newfd >= MAX_FD)
    {
        force_printk("dup2: invalid fd(s), oldfd=%d, newfd=%d\n", oldfd, newfd);
        return -EACCES; // -EBADF
    }
    struct file *f = fd_get(cur->fdt, oldfd);
    if (!f)
        return -EBADF; // -EBADF
    if (newfd == oldfd)
        return newfd; // dup2 to same fd is a no-op
    if (cur->fdt->fd[newfd])
    {
        sys_close(newfd); // close newfd if it's already open
    }
    cur->fdt->fd[newfd] = f;
    f->refcnt++;
    return newfd;
}

int sys_lseek(int fd, off_t offset, int whence)
{
    struct file *f = fd_get(cur->fdt, fd);
    if (!f || !f->ops || !f->ops->lseek)
        return -9;
    return f->ops->lseek(f, offset, whence);
}

int sys_create(const char *pathname, int mode)
{
    return sys_openat(AT_FDCWD, pathname, O_CREAT | O_RDWR, mode);
}
