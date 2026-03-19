#include "mount.h"
#include "pifat.h"
#include "cstr.h"
#include <errno.h>
#include "mem.h"
#include "fd.h"
#include "sched.h"

// fat32 helpers

extern rpi_thread_t *cur;

extern fat_meta_t *fat_meta_obj;

static int translate_filename_posix_to_fat(const char *posix_name, char *fat_name)
{
    // Initialize fat_name with spaces
    for (int i = 0; i < 11; i++)
        fat_name[i] = ' ';

    const char *dot = strrchr(posix_name, '.');
    size_t name_len = dot ? (size_t)(dot - posix_name) : strlen(posix_name);
    size_t ext_len = dot ? strlen(dot + 1) : 0;
    // printk("Name len: %d, Ext len: %d\n", (int)name_len, (int)ext_len);
    if (ext_len > 3)
        return -1; // Name too long for FAT 8.3

    // Copy name part
    if (name_len > 8)
    {
        for (size_t i = 0; i < 6; i++)
            if (posix_name[i] >= 'a' && posix_name[i] <= 'z') // Only convert if it's a lowercase letter
                fat_name[i] = posix_name[i] & ~0b0100000;     // Force uppercase
            else
                fat_name[i] = posix_name[i];
        fat_name[6] = '~';
        fat_name[7] = '1'; // We don't handle collisions, so just append ~1 for truncated names
    }
    else
        for (size_t i = 0; i < name_len; i++)
            if (posix_name[i] >= 'a' && posix_name[i] <= 'z') // Only convert if it's a lowercase letter
                fat_name[i] = posix_name[i] & ~0b0100000;     // Force uppercase
            else
                fat_name[i] = posix_name[i];

    // Copy extension part
    for (size_t i = 0; i < ext_len; i++)
        if (dot[1 + i] >= 'a' && dot[1 + i] <= 'z')    // Only convert if it's a lowercase letter
            fat_name[8 + i] = dot[1 + i] & ~0b0100000; // Force uppercase
        else
            fat_name[8 + i] = dot[1 + i];

    // set null terminator for safety (not used in FAT)
    fat_name[11] = '\0';

    return 0; // Success
}

static int translate_filename_fat_to_posix(const char *fat_name, char *posix_name)
{
    // Copy name part
    int i;
    for (i = 0; i < 8 && fat_name[i] != ' '; i++)
        posix_name[i] = fat_name[i];

    // Check for extension
    int ext_start = 8;
    while (ext_start < 11 && fat_name[ext_start] == ' ')
        ext_start++;

    if (ext_start < 11)
    {
        posix_name[i++] = '.';
        for (int j = ext_start; j < 11 && fat_name[j] != ' '; j++)
            posix_name[i++] = fat_name[j];
    }

    posix_name[i] = '\0'; // Null-terminate
    return 0;             // Success
}

int split_path(const char *path, char *parent, char *name)
{
    if (str_eq(path, "/"))
    {
        memcpy(parent, "/", 2);
        name[0] = 0;
        return 0;
    }

    const char *slash = strrchr(path, '/');
    if (!slash)
        return -EINVAL;

    if (slash == path)
    {
        memcpy(parent, "/", 2);
        memcpy(name, slash + 1, strlen(slash + 1) + 1);
    }
    else
    {
        memcpy(parent, path, slash - path);
        parent[slash - path] = 0;
        memcpy(name, slash + 1, strlen(slash + 1) + 1);
    }
    return 0;
}

uint32_t get_dir_cluster_number(const char *path)
{
    if (str_eq(path, "/"))
        return fat_meta_obj->bpb.root_clus;
    // force_printk("    -> Getting cluster number for path '%s'\n", path);

    char parent[256];
    char name_[256];
    if (split_path(path, parent, name_) != 0)
    {
        force_printk("get_dir_cluster_number: split_path failed for path '%s'\n", path);
        return 0;
    }
    char name[12];
    if (translate_filename_posix_to_fat(name_, name) != 0)
    {
        force_printk("get_dir_cluster_number: translate_filename_posix_to_fat failed for name '%s'\n", name_);
        return 0;
    }
    // printk("Getting cluster number for directory '%s' in parent '%s'\n", name, parent);
    uint32_t parent_clus = get_dir_cluster_number(parent);
    // force_printk("    -> Parent '%s' for '%s' has cluster number %u\n", parent, name, parent_clus);
    if (parent_clus == 0)
        return 0;
    // printk("Parent '%s' is %d of %s\n", parent, parent_clus, name);

    fat32_file_list_entry_t *entries = fat32_list_fat(fat_meta_obj, parent_clus);
    fat32_file_list_entry_t *targ = 0;
    struct list_head *pos;
    for (pos = entries->list.next; pos != &entries->list; pos = pos->next)
    {
        fat32_file_list_entry_t *entry = container_of(pos, fat32_file_list_entry_t, list);
        // skip deleted
        if ((entry->dirent.name[0] == 0xE5) || (entry->dirent.attr & 0x0F) == 0x0F)
            continue;
        char entry_name[12];
        memcpy(entry_name, entry->dirent.name, 11);
        entry_name[11] = 0;
        // trim spaces
        if (strn_eq(entry_name, name, 11))
        {
            uint32_t clus = ((uint32_t)entry->dirent.fst_clus_hi << 16) | entry->dirent.fst_clus_lo;
            free_fat32_file_list_entry_t_list(entries);
            return clus;
        }
    }
    free_fat32_file_list_entry_t_list(entries);
    return 0;
}

fat_dirent_t *get_entry_from_directory(uint32_t dir_clus, const char *name, int name_is_fat)
{
    char tmp_name[12];
    if (!name_is_fat)
    {
        if (translate_filename_posix_to_fat(name, tmp_name) != 0)
            return NULL;
    }
    else
    {
        memcpy(tmp_name, name, 12);
    }
    // printk("get_entry_from_directory: Translated name: '%s'\n", tmp_name);
    fat32_file_list_entry_t *entries = fat32_list_fat(fat_meta_obj, dir_clus);
    fat32_file_list_entry_t *targ = 0;
    fat_dirent_t *res = 0;
    struct list_head *pos;
    for (pos = entries->list.next; pos != &entries->list; pos = pos->next)
    {
        fat32_file_list_entry_t *entry = container_of(pos, fat32_file_list_entry_t, list);
        char entry_name[12];
        memcpy(entry_name, entry->dirent.name, 11);
        entry_name[11] = 0;
        // trim spaces
        // force_printk("get_entry_from_directory: Checking entry '%s' against '%s'\n", entry_name, tmp_name);
        for (int i = 10; i >= 0; i--)
        {
            if (entry_name[i] == ' ')
                entry_name[i] = 0;
            else
                break;
        }
        if (strn_eq(entry_name, tmp_name, 11))
        {
            res = kalloc(sizeof(fat_dirent_t));
            memcpy(res, &entry->dirent, sizeof(fat_dirent_t));
            free_fat32_file_list_entry_t_list(entries);
            return res;
        }
    }
    free_fat32_file_list_entry_t_list(entries);
    return res;
}

int safe_path(const char *path, char *dest)
{
    // For now, only allow / and /filename
    memcpy(dest, path, strlen(path) + 1); // first make a modifiable copy
    // printk("[safe path] initial dest: '%s'\n", dest);
    // if (dest[0] != '/')
    // {
    //     // ok we need to concat with CWD
    //     uint32_t total_len = strlen(cur->cwd) + strlen(path) + 2;
    //     char *tmp = kalloc(total_len);
    //     memcpy(tmp, cur->cwd, strlen(cur->cwd));
    //     tmp[strlen(cur->cwd)] = '/';
    //     memcpy(tmp + strlen(cur->cwd) + 1, path, strlen(path) + 1);
    //     memcpy(dest, tmp, total_len);
    //     kfree(tmp);
    // }
    // printk("[safe path] after cwd check dest: '%s'\n", dest);
    // now, we first remove duplicate slashes
    char *src = dest;
    char *dst = dest;
    while (*src)
    {
        if (src[0] == '/' && src[1] == '/')
        {
            src++;
            continue;
        }
        *dst++ = *src++;
    }
    *dst = 0;
    // now we get rid of /./
    src = dest;
    dst = dest;
    while (*src)
    {
        if (src[0] == '/' && src[1] == '.' && (src[2] == '/' || src[2] == 0))
        {
            src += 2;
            continue;
        }
        *dst++ = *src++;
    }
    *dst = 0;
    // finally we resolve the .. parts
    src = dest;
    dst = dest;
    char *last_slash = 0;
    while (*src)
    {
        if (src[0] == '/' && src[1] == '.' && src[2] == '.' && (src[3] == '/' || src[3] == 0))
        {
            src += 3;
            // go back to last slash
            if (last_slash)
            {
                dst = last_slash;
                last_slash = 0;
                // find new last slash
                for (char *p = dest; p < dst; p++)
                {
                    if (*p == '/')
                        last_slash = p;
                }
            }
            continue;
        }
        if (*src == '/')
            last_slash = dst;
        *dst++ = *src++;
    }
    *dst = 0;
    // edge case: if dest is empty, make it "/"
    if (dest[0] == 0)
    {
        dest[0] = '/';
        dest[1] = 0;
    }
    // printk("[safe path] final dest: '%s'\n", dest);
    return 0;
}

int mount_probe(const char *path)
{
    char parent[256];
    char name[256];
    char safe[256];
    // printk("mount_probe: path='%s'\n", path);
    safe_path(path, safe); // Write to `safe`, not `parent`
    // printk("After safe_path: safe='%s'\n", safe);
    if (str_eq(safe, "/"))
        return 0;
    if (split_path(safe, parent, name) != 0)
        return -ENOENT;

    // printk("mount_probe: parent='%s' name='%s'\n", parent, name);
    uint32_t parent_clus = get_dir_cluster_number(parent);
    if (parent_clus == 0)
    {
        printk("mount_probe: parent directory '%s' not found\n", parent);
        return -ENOENT;
    }

    // printk("mount_probe: parent='%s' clus=%d, name='%s'\n", parent, parent_clus, name);

    fat_dirent_t *entry = get_entry_from_directory(parent_clus, name, 0);
    if (!entry)
    {
        printk("mount_probe: entry not found\n");
        return -ENOENT;
    }

    return 0;
}

char *get_basename(const char *path)
{
    const char *slash = strrchr(path, '/');
    if (!slash)
        return (char *)path;
    return (char *)(slash + 1);
}

fat_dirent_t *mount_get_dirent(const char *path, int use_cwd, int use_parent)
{
    char parent[256];
    char name[256];
    char safe[256];
    char init_path[256];
    if (use_cwd && (!path || path[0] != '/'))
    {
        memcpy(init_path, cur->cwd, strlen(cur->cwd) + 1);
        if (path)
        {
            init_path[strlen(cur->cwd)] = '/';
            memcpy(init_path + strlen(cur->cwd) + 1, path, strlen(path) + 1);
            printk("mount_get_dirent: Called with use_cwd, path='%s'\n", path);
        }
        else
        {
            printk("mount_get_dirent: Called with use_cwd, but path is NULL, using cwd='%s'\n", cur->cwd);
        }
    }
    else
        memcpy(init_path, path, strlen(path) + 1);
    // printk("mount_get_dirent: path='%s'\n", init_path);
    safe_path(init_path, safe); // Write to `safe`, not `parent`
    // printk("mount_get_dirent: After safe_path: safe='%s'\n", safe);
    if (str_eq(safe, "/"))
    {
        fat_dirent_t *root_de = kalloc(sizeof(fat_dirent_t));
        memset(root_de, 0, sizeof(fat_dirent_t));
        root_de->fst_clus_hi = (fat_meta_obj->bpb.root_clus >> 16) & 0xFFFF;
        root_de->fst_clus_lo = fat_meta_obj->bpb.root_clus & 0xFFFF;
        root_de->attr = 0x10; // directory
        return root_de;
    }
    if (split_path(safe, parent, name) != 0)
        return NULL;
    if (use_parent)
    {
        memset(safe, 0, sizeof(safe));
        memset(name, 0, sizeof(name));
        if (split_path(parent, safe, name) != 0)
            return NULL;
        memcpy(parent, safe, sizeof(parent));
    }

    uint32_t parent_clus = get_dir_cluster_number(parent);
    if (parent_clus == 0)
        return NULL;

    fat_dirent_t *entry = get_entry_from_directory(parent_clus, name, 0);
    return entry; // may be NULL
}

int mount_read_w_offset(const char *path, uint8_t *buf, uint32_t size, uint32_t offset)
{
    char parent[256];
    char name[256];
    char safe[256];
    safe_path(path, safe);
    if (split_path(safe, parent, name) != 0)
        return -ENOENT;

    uint32_t parent_clus = get_dir_cluster_number(parent);
    if (parent_clus == 0)
        return -ENOENT;

    fat_dirent_t *entry = get_entry_from_directory(parent_clus, name, 0);
    if (!entry)
        return -ENOENT;

    uint32_t start_clus = ((uint32_t)entry->fst_clus_hi << 16) | entry->fst_clus_lo;
    uint32_t fsize = entry->file_size;
    if (offset >= fsize)
        return 0; // EOF

    if (offset + size > fsize)
        size = fsize - offset; // adjust size to not exceed file size

    uint8_t *file_data = fat32_read_file(fat_meta_obj, start_clus, fsize);
    memcpy(buf, file_data + offset, size);
    kfree(file_data);
    return size;
}

int mount_get_dirents64(uint32_t fd, linux_dirent64_t *dirp, uint32_t count)
{
    struct file *f = fd_get(cur->fdt, fd);
    printk("mount_get_dirents64: fd=%d, dirp=%x, count=%u\n", fd, (uint32_t)dirp, count);
    if (!f)
        return -EBADF;
    printk("mount_get_dirents64: file flags=%d\n", f->flags);
    fat_dirent_t *de = ((open_file_priv_t *)f->priv)->dirent;
    uint32_t dir_clus = ((uint32_t)de->fst_clus_hi << 16) | de->fst_clus_lo;
    printk("mount_get_dirents64: dir cluster=%d\n", dir_clus);
    fat32_file_list_entry_t *entries = fat32_list_fat(fat_meta_obj, dir_clus);
    printk("mount_get_dirents64: got entries list\n");
    struct list_head *pos;
    uint32_t bytes_written = 0;
    uint32_t bytes_to_skip = f->pos;
    for (pos = entries->list.next; pos != &entries->list; pos = pos->next)
    {
        fat32_file_list_entry_t *entry = container_of(pos, fat32_file_list_entry_t, list);
        // skip deleted
        if ((entry->dirent.name[0] == 0xE5) || (entry->dirent.attr & 0x0F) == 0x0F)
            continue;
        printk("[getdirents64] adding entry name raw: '%s'\n", entry->dirent.name);
        char entry_name[256];
        translate_filename_fat_to_posix((char *)entry->dirent.name, entry_name);
        uint32_t reclen = (sizeof(linux_dirent64_t) + strlen(entry_name) + 1 + 7) & ~7;
        if (bytes_to_skip >= reclen)
        {
            bytes_to_skip -= reclen;
            continue;
        }
        if (bytes_written + reclen > count)
            break; // no more space
        linux_dirent64_t *d = (linux_dirent64_t *)((uint8_t *)dirp + bytes_written);
        d->d_ino = 1; // fake inode
        d->d_off = bytes_written + reclen;
        d->d_reclen = (unsigned short)reclen;
        d->d_type = (entry->dirent.attr & 0x10) ? 4 : 8; // DT_DIR=4, DT_REG=8
        memcpy(d->d_name, entry_name, strlen(entry_name) + 1);
        bytes_written += reclen;
        f->pos += reclen;
    }
    free_fat32_file_list_entry_t_list(entries);
    return bytes_written;
}

int mount_fstatat64(int dirfd, const char *path,
                    stat_t *statbuf,
                    int flags)
{
    (void)dirfd;
    (void)flags;
    char init_path[256];
    if (dirfd == AT_FDCWD && path[0] != '/')
    {
        printk("Called mount_fstatat64 with AT_FDCWD, cwd='%s', path='%s'\n", cur->cwd, path);
        memcpy(init_path, cur->cwd, strlen(cur->cwd) + 1);
        init_path[strlen(cur->cwd)] = '/';
        memcpy(init_path + strlen(cur->cwd) + 1, path, strlen(path) + 1);
    }
    else
        memcpy(init_path, path, strlen(path) + 1);

    char safe[512];
    safe_path(init_path, safe);
    printk("mount_fstatat64: path='%s'\n", safe);

    if (str_eq(safe, "/"))
    {
        // root dir
        memset(statbuf, 0, sizeof(stat_t));
        statbuf->st_dev = 1; // fake device
        statbuf->st_blksize = 512;
        statbuf->st_size = 0;
        statbuf->st_blocks = 0;
        statbuf->st_nlink = 1;      // FAT has no hard links
        statbuf->st_ino = 1;        // fake inode (non-zero)
        statbuf->__st_ino = 1;      // old 32-bit inode field
        statbuf->st_uid = 0;        // root user
        statbuf->st_gid = 0;        // root group
        statbuf->st_mode = 0040755; // directory with rwxr-xr-x permissions

        printk("fstatat64: st_mode=0x%x, st_size=%d, sizeof(stat_t)=%d\n",
               statbuf->st_mode, (int)statbuf->st_size, sizeof(stat_t));

        return 0;
    }

    fat_dirent_t *entry = mount_get_dirent(safe, 0, 0);
    if (!entry)
    {
        printk("mount_fstatat64: entry not found for path '%s'\n", safe);
        return -ENOENT;
    }

    memset(statbuf, 0, sizeof(stat_t));
    statbuf->st_dev = 1; // fake device
    statbuf->st_blksize = 512;
    statbuf->st_size = entry->file_size;
    statbuf->st_blocks = (statbuf->st_size + 511) / 512;
    statbuf->st_nlink = 1; // FAT has no hard links
    statbuf->st_ino = 1;   // fake inode (non-zero)
    statbuf->__st_ino = 1; // old 32-bit inode field
    statbuf->st_uid = 0;   // root user
    statbuf->st_gid = 0;   // root group
    // make everything executable: S_IFREG=0100000, S_IFDIR=0040000, 0755=rwxr-xr-x
    statbuf->st_mode = (entry->attr & 0x10) ? 0040755 : 0100755;

    printk("fstatat64: st_mode=0x%x, st_size=%d, sizeof(stat_t)=%d\n",
           statbuf->st_mode, (int)statbuf->st_size, sizeof(stat_t));

    return 0;
}

// for syscalls

uint32_t mount_openat(int dfd, const char *path, int flags, int mode)
{
    (void)flags;
    (void)mode;
    char init_path[256];
    if (dfd == AT_FDCWD && (!path || path[0] != '/'))
    {
        if (path)
            printk("Called mount_openat with AT_FDCWD, cwd='%s', path='%s'\n", cur->cwd, path);
        else
            printk("Called mount_openat with AT_FDCWD, cwd='%s', path=NULL\n", cur->cwd);
        memcpy(init_path, cur->cwd, strlen(cur->cwd) + 1);
        init_path[strlen(cur->cwd)] = '/';
        memcpy(init_path + strlen(cur->cwd) + 1, path, strlen(path) + 1);
    }
    else
        memcpy(init_path, path, strlen(path) + 1);

    char parent[256];
    char name[256];
    char safe[256];
    // force_printk("mount_openat: path='%s'\n", init_path);
    safe_path(init_path, safe);
    // printk("After safe_path: safe='%s'\n", safe);
    if (str_eq(safe, "/"))
    {
        // root dir
        printk("mount_openat: opening root dir\n");
        return fat_meta_obj->bpb.root_clus;
    }
    if (split_path(safe, parent, name) != 0)
        return -ENOENT;
    // force_printk("mount_openat: parent='%s' name='%s'\n", parent, name);

    uint32_t parent_clus = get_dir_cluster_number(parent);
    if (parent_clus == 0)
    {
        force_printk("mount_openat: parent directory '%s' not found\n", parent);
    }

    fat_dirent_t *entry = get_entry_from_directory(parent_clus, name, 0);
    if (!entry)
    {
        if (flags & O_CREAT)
        {
            printk("mount_openat: file '%s' not found, but O_CREAT is set, so we create it\n", name);
            char fat_name[12];
            if (translate_filename_posix_to_fat(name, fat_name) != 0)
            {
                force_printk("mount_openat: failed to translate filename '%s' to FAT format\n", name);
                return -EINVAL;
            }
            fat32_write_file_create(fat_meta_obj, parent_clus, "", 0, fat_name);
            return parent_clus;
        }
        else
        {
            printk("mount_openat: file '%s' not found and O_CREAT is not set\n", name);
            return -ENOENT;
        }
    }

    // uint32_t start_clus = ((uint32_t)entry->fst_clus_hi << 16) | entry->fst_clus_lo;
    return parent_clus;
}

int mount_close(struct file *f)
{
    (void)f;
    return 0;
}

ssize_t mount_read(struct file *f, void *buf, size_t n)
{
    uint32_t offset = (uint32_t)f->pos;
    uint32_t bytes_to_read = n;
    fat_dirent_t *de = ((open_file_priv_t *)f->priv)->dirent;
    if (offset >= de->file_size)
        return 0; // EOF
    uint32_t clus = ((uint32_t)de->fst_clus_hi << 16) | de->fst_clus_lo;
    if (offset + bytes_to_read > de->file_size)
        bytes_to_read = de->file_size - offset;
    fat32_read_file_with_offset(fat_meta_obj, clus, buf, bytes_to_read, offset);
    f->pos += bytes_to_read;
    return bytes_to_read;
}

ssize_t mount_write(struct file *f, const void *buf, size_t n)
{
    printk("mount_write: called with n=%u\n", n);
    uint32_t offset = (uint32_t)f->pos;
    printk("offset: %u\n", offset);
    uint32_t bytes_to_write = n;
    fat_dirent_t *de = ((open_file_priv_t *)f->priv)->dirent;
    fat_dirent_t *pd = ((open_file_priv_t *)f->priv)->parent_dirent;
    uint32_t written = fat32_write_with_offset(fat_meta_obj, ((uint32_t)de->fst_clus_hi << 16) | de->fst_clus_lo, buf, bytes_to_write, offset);
    printk("bytes written: %u\n", written);
    uint32_t new_size = offset + written;
    if (new_size > de->file_size)
    {
        de->file_size = new_size;
        fat32_update_dirent(fat_meta_obj, ((uint32_t)pd->fst_clus_hi << 16) | pd->fst_clus_lo, de->name, de);
        printk("updating dirent for file '%s'\n", de->name);
        ((open_file_priv_t *)f->priv)->dirent = get_entry_from_directory((uint32_t)pd->fst_clus_hi << 16 | (uint32_t)pd->fst_clus_lo, de->name, 1);
        printk("update ok\n\n");
        if (((open_file_priv_t *)f->priv)->dirent == NULL)
        {
            force_printk("ERROR: failed to update dirent after write for file '%s'\n", de->name);
            rpi_reboot();
        }
        kfree(de);
    }
    f->pos += written;
    return written;
}

long mount_lseek(struct file *f, off_t offset, int whence)
{
    if (whence == SEEK_SET)
        f->pos = offset;
    else if (whence == SEEK_CUR)
        f->pos += offset;
    else if (whence == SEEK_END)
        f->pos = ((open_file_priv_t *)f->priv)->dirent->file_size + offset;
    else
        return -EINVAL;
    return f->pos;
}

long mount_ioctl(struct file *f, unsigned long cmd, unsigned long arg)
{
    return -ENOSYS;
}

long mount_fcntl(struct file *f, unsigned cmd, unsigned long arg)
{
    printk("mount_fcntl: cmd=%d, arg=%lu\n", cmd, arg);
    if (cmd == 3) // F_GETFL
    {
        return O_RDWR;
    }
    else if (cmd == 1030) // F_DUPFD_CLOEXEC
    {
        int new_fd = alloc_fd(cur->fdt, f);
        if (new_fd < 0)
            return -EMFILE; // Too many open files
        f->refcnt++;
        return new_fd;
    }
    force_printk("WARNING: mount_fcntl: unhandled cmd %d\n", cmd);
    return 0;
}
