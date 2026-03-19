#include "syscall.h"
#include "sched.h"
#include "vm.h"
#include "serial.h"
#include "fd.h"
#include "mount.h"
#include "elf.h"
#include "flags.h"
#include "errno.h"

#define ENOSYS 38
#define EINVAL 22

struct iovec
{
    void *iov_base; // pointer to buffer
    size_t iov_len; // length in bytes
};

extern rpi_thread_t *cur;

uint32_t set_brk_impl(uint32_t brk)
{
    uint32_t old_brk = cur->brk;
    if (brk == 0)
        return old_brk;
    if (brk < cur->brk_min)
        return old_brk;
    if (brk > old_brk)
    {
        // Allocate and zero new pages (POSIX requires brk memory to be zeroed)
        uint32_t size_needed = brk - old_brk;
        make_sure_user_space(cur->l1_pt, old_brk, size_needed);
        uint32_t end_va = (old_brk + size_needed + 0xFFF) & ~0xFFF;
        uint32_t start_page = old_brk & ~0xFFF;
        memset((void *)start_page, 0, end_va - start_page);
        brk = end_va;
    }
    cur->brk = brk;
    return brk;
}

uint32_t write_impl(int fd, const void *buf, uint32_t count)
{
    // printk("Writing: %d bytes", count);
    // printk("-------- Data: '");
    // printk((const char *)buf);
    // printk("'\n--------\n");
    const char *cbuf = (const char *)buf;
    // for (uint32_t i = 0; i < count; i++)
    // {
    //     uart_putc(cbuf[i]);
    // }
    // printk("\n");
    return sys_write(fd, buf, count);
}

uint32_t open_impl(const char *pathname, int flags, int mode)
{
    (void)pathname;
    (void)flags;
    (void)mode;
    return -1; // Not implemented
}

int openat_impl(int dirfd, const char *pathname, int flags, int mode)
{
    if (pathname)
        printk("openat called: dirfd=%d, pathname='%s', flags=%d, mode=%d\n", dirfd, pathname, flags, mode);
    else
        printk("openat called: dirfd=%d, pathname=NULL, flags=%d, mode=%d\n", dirfd, flags, mode);

    return sys_openat(dirfd, pathname, flags, mode);
}

int close_impl(int fd)
{
    (void)fd;
    return 0; // Not implemented
}

int read_impl(int fd, void *buf, uint32_t count)
{
    // check if IRQ is still on?
    uint32_t cpsr;
    asm volatile("mrs %0, cpsr" : "=r"(cpsr));
    // printk("CPSR in read_impl: %b\n", cpsr);
    // printk("Reading into buffer at %x, count %u from fd %d\n", (uint32_t)buf, count, fd);
    uint32_t ret = sys_read(fd, buf, count);
    // printk("Read returned %d\n", ret);
    return ret;
}

int fork_impl()
{
    return -ENOSYS; // Not implemented
}

int waitpid_impl(int pid, int *status, int options)
{
    if (DEBUG_SYSCALL)
        printk("waitpid called for pid %d, options %d\n", pid, options);
    while (see_if_pid_still_exists(pid)) // wait until pid exits
    {
        cs_svc_sleep(1);
    }
    if (DEBUG_SYSCALL)
        printk("waitpid: pid %d has exited\n", pid);
    if (status)
        *status = 0; // dummy status
    return pid;
}

int sys_set_tid_address_impl(int *tidptr)
{
    cur->clear_tid = tidptr; // save it just in case lol
    return cur->tid;         // stub 0: I don't have real TIDs, tids are pids
}

int sys_set_robust_list_impl(struct robust_list_head *head, size_t len)
{
    // Stub by GPT:
    // glibc just stores these in its pthread structs;
    // kernel doesn’t need to do anything yet.
    (void)head;
    (void)len;
    return 0; // success
}

int sys_rseq()
{
    return -ENOSYS;
}

int writev_impl(int fd, const struct iovec *iov, int iovcnt)
{
    uint32_t total_bytes = 0;
    for (int i = 0; i < iovcnt; i++)
    {
        total_bytes += write_impl(fd, iov[i].iov_base, iov[i].iov_len);
    }
    return total_bytes;
}

void get_cwd_impl(char *buf, size_t size)
{
    const char *cwd = cur->cwd;
    size_t len = strlen(cwd);
    if (size < len + 1)
    {
        // Not enough space
        if (size > 0)
            buf[0] = '\0';
        return;
    }
    memcpy(buf, cwd, len + 1);
}

int wait4_impl(int pid, int *status, int options, struct rusage *ru)
{
    if (DEBUG_SYSCALL)
        printk("waitpid called for pid %d, options %d\n", pid, options);
    if (pid == -1)
    {
        // all child processes
        if (number_of_childs(cur->tid) > 0)
        {
            if (DEBUG_SYSCALL)
                printk("Nah there are still childs\n");
            cur->ctx.resume_pc -= 4; // rewind pc to re-execute syscall after sleep
            cs_svc_sleep(5);
        }
    }
    else
    {
        if (see_if_pid_still_exists(pid)) // wait until pid exits
        {
            cur->ctx.resume_pc -= 4; // rewind pc to re-execute syscall after sleep
            cs_svc_sleep(5);
        }
        if (DEBUG_SYSCALL)
            printk("waitpid: pid %d has exited\n", pid);
    }
    return pid;
}

uint32_t ioctl_impl(int fd, uint32_t cmd, termios_t *arg)
{
    return sys_ioctl(fd, cmd, arg);
}

int chdir_impl(const char *path)
{
    if (mount_probe(path) != 0)
    {
        return -EINVAL;
    }
    kfree(cur->cwd);
    uint32_t len = strlen(path);
    cur->cwd = kalloc(len + 1);
    memcpy(cur->cwd, path, len + 1);
    printk(">>>>>Changed directory to '%s'\n", cur->cwd);
    return 0;
}
int statx64_impl(int dirfd, const char *path,
                 int flags, unsigned int mask,
                 struct statx *statxbuf) // this is fucked, do not use
{
    printk("statx64 called: dirfd=%d, path='%s', flags=%d, mask=%u\n", dirfd, path, flags, mask);
    if (dirfd != AT_FDCWD)
    {
        return -ENOSYS; // Not implemented
    }
    (void)flags;
    (void)mask;
    if (mount_probe(path) != 0)
    {
        printk("statx64: mount_probe failed for path '%s'\n", path);
        return -EINVAL;
    }
    // Stub: fill statxbuf with dummy data
    memset(statxbuf, 0, sizeof(statx_t));
    statxbuf->stx_mask = 0;
    statxbuf->stx_blksize = 4096;
    statxbuf->stx_nlink = 1;
    statxbuf->stx_uid = 1000;    // dummy user
    statxbuf->stx_gid = 1000;    // dummy group
    statxbuf->stx_mode = 0x81A4; // regular file with rw-r--r-- permissions
    statxbuf->stx_ino = 123456;  // dummy inode number
    statxbuf->stx_size = 1024;   // dummy file size
    statxbuf->stx_blocks = (statxbuf->stx_size + 511) / 512;
    printk("statx64: returning dummy statx for path '%s'\n", path);
    return 0;
}

int fstatat64_impl(int dirfd, const char *path,
                   stat_t *statbuf,
                   int flags)
{
    printk("fstatat64 called: dirfd=%d, path='%s', flags=%d\n", dirfd, path, flags);

    // AT_EMPTY_PATH (0x1000): stat the file referred to by dirfd
    if ((flags & 0x1000) && (!path || path[0] == '\0')) {
        struct file *f = fd_get(cur->fdt, dirfd);
        if (!f) return -EBADF;
        memset(statbuf, 0, sizeof(stat_t));
        statbuf->st_dev = 1;
        statbuf->st_blksize = 512;
        statbuf->st_nlink = 1;
        statbuf->st_ino = 1;
        statbuf->__st_ino = 1;
        if (f->priv) {
            fat_dirent_t *de = ((open_file_priv_t *)f->priv)->dirent;
            if (de) {
                statbuf->st_size = de->file_size;
                statbuf->st_blocks = (de->file_size + 511) / 512;
                statbuf->st_mode = (de->attr & 0x10) ? 0040755 : 0100755;
                printk("fstatat64: st_mode=0x%x, st_size=%d, sizeof(stat_t)=%d\n",
                       statbuf->st_mode, (int)statbuf->st_size, sizeof(stat_t));
                return 0;
            }
        }
        // fd without priv (e.g. tty) - return char device info
        statbuf->st_mode = 0020755; // S_IFCHR | 0755
        statbuf->st_size = 0;
        printk("fstatat64: st_mode=0x%x, st_size=%d, sizeof(stat_t)=%d\n",
               statbuf->st_mode, (int)statbuf->st_size, sizeof(stat_t));
        return 0;
    }

    return mount_fstatat64(dirfd, path, statbuf, flags);
}

int getdent64_impl(uint32_t fd, linux_dirent64_t *dirp, uint32_t count)
{
    printk("getdent64 called: fd=%d, dirp=%x, count=%u\n", fd, (uint32_t)dirp, count);
    return mount_get_dirents64(fd, dirp, count);
}

int execve_impl(const char *filename, char *const argv[], char *const envp[])
{
    if (mount_probe(filename) != 0)
        return -EINVAL;
    fat_dirent_t *de = mount_get_dirent(filename, 1, 0);
    if (!de)
        de = mount_get_dirent(filename, 0, 0);
    if (!de)
        return -EINVAL;
    printk("execve: would load file '%s' of size %u bytes\n", filename, de->file_size);
    uint8_t *file_buf = kalloc(de->file_size);
    mount_read_w_offset(filename, file_buf, de->file_size, 0);
    /// OK, pause a bit to process argc and argv
    int k_argc = 0;
    while (argv[k_argc])
        k_argc++;
    char **k_argv = kalloc((k_argc + 1) * sizeof(char *));
    for (int i = 0; i < k_argc; i++)
    {
        // printk("execve+++++++++++: arg %d: '%s'\n", i, argv[i]);
        uint32_t len = strlen(argv[i]) + 1;
        k_argv[i] = kalloc(len);
        memcpy(k_argv[i], argv[i], len);
    }
    k_argv[k_argc] = 0;
    int k_envc = 0;
    while (envp && envp[k_envc])
        k_envc++;
    char **k_envp = 0;
    if (k_envc > 0)
    {
        k_envp = kalloc((k_envc + 1) * sizeof(char *));
        for (int i = 0; i < k_envc; i++)
        {
            uint32_t len = strlen(envp[i]) + 1;
            k_envp[i] = kalloc(len);
            memcpy(k_envp[i], envp[i], len);
        }
        k_envp[k_envc] = 0;
    }
    /// OK, continue
    // we need to do some memory clean up: first we get a fresh L1 page table
    uint32_t *new_l1_pt = make_initial_L1_table();
    switch_L1_page_table((uint32_t)new_l1_pt, cur->tid);
    free_L1_page_table(cur->l1_pt); // free old page table (avoid kernel memory leak)
    cur->l1_pt = new_l1_pt;
    uint32_t user_pc = load_elf(cur, (uint32_t *)file_buf, de->file_size, k_argc, k_argv, k_envp);
    kfree(file_buf);
    // clean up context
    if (user_pc == ELF_ERR)
        return -EINVAL;
    cur->ctx.resume_pc = user_pc;
    // cur->cwd = kalloc(2);
    // cur->cwd[0] = '/';
    // cur->cwd[1] = '\0';
    for (int i = 0; i < 13; i++)
        cur->ctx.regs[i] = 0; // clear r0-r12
    // This new process is ready, so we set ready to true
    cur->ready = true;
    // this syscall should now return into the new user code
    return 0;
}

int fcntl64_impl(int fd, int cmd, uint64_t arg)
{
    if (fd >= 0 && fd < 3)
    {
        return 0;
    }
    (void)fd;
    (void)cmd;
    (void)arg;
    printk("fcntl64 called: fd=%d, cmd=%d, arg=%lu\n", fd, cmd, arg);
    return sys_fcntl(fd, cmd, arg);
}

int _llseek_impl(
    unsigned int fd,
    unsigned long offset_high,
    unsigned long offset_low,
    void *result,
    unsigned int whence)
{
    long off = sys_lseek(fd, (off_t)(((uint64_t)offset_high << 32) | offset_low), whence);
    if (off < 0)
        return off;
    *(long long *)result = off;
    return 0;
}

// https://gpages.juszkiewicz.com.pl/syscalls-table/syscalls.html
void posix_syscall(saved_context_t *ctx)
{
    uint32_t syscall_number = ctx->regs[7]; // syscall number in r7
                                            // if (DEBUG_SYSCALL)
    if (DEBUG_SYSCALL)
        printk(" [%d] POSIX syscall number: %d, resume_PC=%x, user_lr=%x\n", cur->tid, syscall_number, ctx->resume_pc, ctx->user_lr);
    // force_printk(" [%d] POSIX syscall number: %d, resume_PC=%x, user_lr=%x\n", cur->tid, syscall_number, ctx->resume_pc, ctx->user_lr);
    if (DEBUG_SYSCALL)
        for (uint16_t i = 0; i < 8; i++)
            printk("  r%d: %x\n", i, ctx->regs[i]);

    cur->ctx = *ctx; // save context in case of sleep

    switch (syscall_number)
    {
    case 248: // exit_group, we do like exit
    case 1:   // exit
        rpi_exit(ctx);
        break;
    case 2:
        ctx->regs[0] = fork_impl();
        break;
    case 3:
        ctx->regs[0] = read_impl((int)ctx->regs[0], (void *)ctx->regs[1], (uint32_t)ctx->regs[2]);
        break;
    case 4:
        ctx->regs[0] = write_impl((int)ctx->regs[0], (const void *)ctx->regs[1], (uint32_t)ctx->regs[2]);
        break;
    case 5:
        ctx->regs[0] = open_impl((const char *)ctx->regs[0], ctx->regs[1], ctx->regs[2]);
        break;
    case 6:
        ctx->regs[0] = close_impl((int)ctx->regs[0]);
        break;
    case 7:
        ctx->regs[0] = waitpid_impl((int)ctx->regs[0], (int *)ctx->regs[1], (int)ctx->regs[2]);
        break;
    case 8:  // create
    case 9:  // link
    case 10: // unlink
        ctx->regs[0] = -ENOSYS;
        break;
    case 11: // execve
        // rpi_exit(ctx);
        ctx->regs[0] = execve_impl((const char *)ctx->regs[0],
                                   (char *const *)ctx->regs[1],
                                   (char *const *)ctx->regs[2]);
        *ctx = cur->ctx; // reload context in case execve changed it
        break;
    case 12: // chdir
        ctx->regs[0] = chdir_impl((const char *)ctx->regs[0]);
        break;
    case 14: // mknod
    case 15: // chmod
    case 16: // lchown
    // case 19: // lseek
    //     ctx->regs[0] = -ENOSYS;
    //     break;
    case 20:                     // getpid
        ctx->regs[0] = cur->tid; // stub: return thread id as pid
        break;
    case 45:
        ctx->regs[0] = set_brk_impl(ctx->regs[0]);
        break;
    case 140:
        ctx->regs[0] = _llseek_impl((unsigned int)ctx->regs[0],
                                     (unsigned long)ctx->regs[1],
                                     (unsigned long)ctx->regs[2],
                                     (void *)ctx->regs[3],
                                     (unsigned int)ctx->regs[4]);
        break;
    case 146:
        ctx->regs[0] = writev_impl((int)ctx->regs[0], (const struct iovec *)ctx->regs[1], (int)ctx->regs[2]);
        break;
    case 168: // polll
        ctx->regs[0] = sys_fd_poll((pollfd_t *)ctx->regs[0], (uint32_t)ctx->regs[1], (int)ctx->regs[2]);
        break;
    case 183: // get CWD
        get_cwd_impl((char *)ctx->regs[0], (size_t)ctx->regs[1]);
        ctx->regs[0] = strlen((char *)ctx->regs[0]);
        break;
    case 54: // ioctl, the real important one
        ctx->regs[0] = ioctl_impl(ctx->regs[0], ctx->regs[1], ctx->regs[2]);
        break;
    case 114: // wait4 pid_t pid	int *stat_addr	int options	struct rusage *ru
        ctx->regs[0] = wait4_impl((int)ctx->regs[0], (int *)ctx->regs[1], (int)ctx->regs[2], (struct rusage *)ctx->regs[3]);
        break;
    case 322: // Openat https://www.man7.org/linux/man-pages/man2/openat.2.html
        ctx->regs[0] = openat_impl((int)ctx->regs[0], (const char *)ctx->regs[1], (int)ctx->regs[2], (int)ctx->regs[3]);
        break;
    case 221: // fcntl64
        ctx->regs[0] = fcntl64_impl((int)ctx->regs[0], (int)ctx->regs[1], ctx->regs[2]);
        break;
    case 33: // access(pathname, mode) - stub to succeed
        ctx->regs[0] = 0;
        break;
    case 41: // dup
    {
        int oldfd = (int)ctx->regs[0];
        struct file *dupf = fd_get(cur->fdt, oldfd);
        if (!dupf) {
            ctx->regs[0] = (uint32_t)-2;
        } else {
            int newfd = alloc_fd(cur->fdt, dupf);
            if (newfd >= 0)
                dupf->refcnt++;
            ctx->regs[0] = (uint32_t)newfd;
        }
        break;
    }
    case 63: // dup2
        ctx->regs[0] = sys_dup2((int)ctx->regs[0], (int)ctx->regs[1]);
        break;
    case 85: // create
        ctx->regs[0] = sys_create((const char *)ctx->regs[0], (int)ctx->regs[1]);
    case 44:              // geteuid32
    case 64:              // getppid
    case 125:             // mprotect - stub to succeed
    case 174:             // rt_sigaction - stub to succeed
    case 175:             // rt_sigprocmask - stub to succeed
    case 201:             // geteuid32 or time
        ctx->regs[0] = 0; // dummy time
        break;
    case 263: // clock_gettime
    case 403: // clock_gettime64
    {
        // Return a dummy time to satisfy glibc initialization
        // r0 = clock_id, r1 = timespec pointer
        uint32_t *ts = (uint32_t *)ctx->regs[1];
        if (ts)
        {
            ts[0] = 0; // tv_sec
            ts[1] = 0; // tv_nsec (or tv_sec high bits for 64-bit)
            if (syscall_number == 403)
            {
                ts[2] = 0; // tv_nsec for clock_gettime64
            }
        }
        ctx->regs[0] = 0; // success
        break;
    }
    case 120: // clone FIXME: using fork logic for CLONE!!
    case 190: // vfork
        printk("vfork called\n");
        printk("MY_RESUME_PC: %x\n", ctx->resume_pc);
        rpi_fork(ctx);
        printk("vfork returning\n");
        break;
    case 191:
    case 192:
        ctx->regs[0] = -ENOSYS;
        break;
    case 217: // getdent64, seekdir64, telldir64
        ctx->regs[0] = getdent64_impl((uint32_t)ctx->regs[0], (struct linux_dirent64 *)ctx->regs[1], (uint32_t)ctx->regs[2]);
        break;
    case 327: // fstatat64
        // ctx->regs[0] = -ENOSYS;

        ctx->regs[0] = fstatat64_impl((int)ctx->regs[0], (const char *)ctx->regs[1],
                                      (stat_t *)ctx->regs[2], (int)ctx->regs[3]);
        break;
    case 332:
        ctx->regs[0] = -ENOSYS;
        break;
    case 397: // statx64
        ctx->regs[0] = -ENOSYS;
        // ctx->regs[0] = statx64_impl((int)ctx->regs[0], (const char *)ctx->regs[1],
        // (int)ctx->regs[2], (unsigned int)ctx->regs[3],
        // (struct statx *)ctx->regs[4]);
        break;
    case 384: // getrandom(buf, buflen, flags)
    {
        // getrandom syscall - fill buffer with "random" bytes
        // glibc uses this for pointer guard initialization
        uint8_t *buf = (uint8_t *)ctx->regs[0];
        size_t buflen = (size_t)ctx->regs[1];
        // uint32_t flags = ctx->regs[2];  // GRND_NONBLOCK=1, GRND_RANDOM=2

        // Use a simple deterministic pattern for reproducibility
        // Match AT_RANDOM so pointer guard is consistent
        for (size_t i = 0; i < buflen; i++)
        {
            buf[i] = 0x11;
        }
        ctx->regs[0] = buflen; // return number of bytes written
        break;
    }
    case 256: // set_tid_address
        ctx->regs[0] = sys_set_tid_address_impl((int *)ctx->regs[0]);
        break;
    case 338:
        ctx->regs[0] = -ENOSYS; // sys_set_robust_list_impl((struct robust_list_head *)ctx->regs[0], (size_t)ctx->regs[1]);
        break;
    case 398:
        ctx->regs[0] = (uint32_t)-ENOSYS;
        break;
    case 439: // faccessat2
        // just return access allowed
        ctx->regs[0] = 0;
        break;
    case 0xf0005: // set_tls
        cur->tls_base = (uint32_t *)ctx->regs[0];
        set_tls_base((uint32_t)cur->tls_base);
        isb();
        ctx->regs[0] = 0;
        break;
    default:
        force_printk("Unhandled POSIX syscall number %d\n", syscall_number);
        // rpi_reboot();
        ctx->regs[0] = (uint32_t)-ENOSYS;
        break;
    }
    cur->ctx = *ctx; // save context in case changed during syscall
    if (DEBUG_SYSCALL)
        printk(" [%d] POSIX syscall done, return value: %d, next resume_PC=%x\n", cur->tid, ctx->regs[0], ctx->resume_pc);
    // dump_saved_context(ctx);
}
