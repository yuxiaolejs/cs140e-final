#ifndef MOUNT_H
#define MOUNT_H
#include "types.h"
#include "pifat.h"
#include "fd.h"

#define DN_ACCESS 0x00000001    /* File accessed */
#define DN_MODIFY 0x00000002    /* File modified */
#define DN_CREATE 0x00000004    /* File created */
#define DN_DELETE 0x00000008    /* File removed */
#define DN_RENAME 0x00000010    /* File renamed */
#define DN_ATTRIB 0x00000020    /* File changed attibutes */
#define DN_MULTISHOT 0x80000000 /* Don't remove notifier */

#define AT_FDCWD -100             /* Special value used to indicate \
                                     openat should use the current  \
                                     working directory. */
#define AT_SYMLINK_NOFOLLOW 0x100 /* Do not follow symbolic links.  */
#define AT_REMOVEDIR 0x200        /* Remove directory instead of \
                                 unlinking file.  */
#define AT_SYMLINK_FOLLOW 0x400   /* Follow symbolic links.  */

#define SEEK_SET 0 /* Seek from beginning of file.  */
#define SEEK_CUR 1 /* Seek from current position.  */
#define SEEK_END 2 /* Seek from end of file.  */

struct statx_timestamp
{
   int64_t tv_sec;   /* Seconds since the Epoch (UNIX time) */
   uint32_t tv_nsec; /* Nanoseconds since tv_sec */
};

typedef struct statx
{
   uint32_t stx_mask;       /* Mask of bits indicating
                            filled fields */
   uint32_t stx_blksize;    /* Block size for filesystem I/O */
   uint64_t stx_attributes; /* Extra file attribute indicators */
   uint32_t stx_nlink;      /* Number of hard links */
   uint32_t stx_uid;        /* User ID of owner */
   uint32_t stx_gid;        /* Group ID of owner */
   uint16_t stx_mode;       /* File type and mode */
   uint64_t stx_ino;        /* Inode number */
   uint64_t stx_size;       /* Total size in bytes */
   uint64_t stx_blocks;     /* Number of 512B blocks allocated */
   uint64_t stx_attributes_mask;
   /* Mask to show what's supported
      in stx_attributes */

   /* The following fields are file timestamps */
   struct statx_timestamp stx_atime; /* Last access */
   struct statx_timestamp stx_btime; /* Creation */
   struct statx_timestamp stx_ctime; /* Last status change */
   struct statx_timestamp stx_mtime; /* Last modification */

   /* If this file represents a device, then the next two
      fields contain the ID of the device */
   uint32_t stx_rdev_major; /* Major ID */
   uint32_t stx_rdev_minor; /* Minor ID */

   /* The next two fields contain the ID of the device
      containing the filesystem where the file resides */
   uint32_t stx_dev_major; /* Major ID */
   uint32_t stx_dev_minor; /* Minor ID */

   uint64_t stx_mnt_id; /* Mount ID */

   /* Direct I/O alignment restrictions */
   uint32_t stx_dio_mem_align;
   uint32_t stx_dio_offset_align;

   uint64_t stx_subvol; /* Subvolume identifier */

   /* Direct I/O atomic write limits */
   uint32_t stx_atomic_write_unit_min;
   uint32_t stx_atomic_write_unit_max;
   uint32_t stx_atomic_write_segments_max;

   /* File offset alignment for direct I/O reads */
   uint32_t stx_dio_read_offset_align;

   /* Direct I/O atomic write max opt limit */
   uint32_t stx_atomic_write_unit_max_opt;
} statx_t;

// ARM Linux stat64 structure - must match kernel ABI exactly!
// Using packed + explicit padding so TCC and GCC produce identical layout (104 bytes).
typedef struct __attribute__((packed)) stat64
{
   uint64_t st_dev;        // 0
   uint8_t __pad0[4];      // 8
   uint32_t __st_ino;      // 12  old 32-bit inode
   uint32_t st_mode;       // 16
   uint32_t st_nlink;      // 20
   uint32_t st_uid;        // 24
   uint32_t st_gid;        // 28
   uint64_t st_rdev;       // 32
   uint8_t __pad3[4];      // 40
   uint8_t __pad4[4];      // 44  align st_size to offset 48
   int64_t st_size;        // 48
   uint32_t st_blksize;    // 56
   uint8_t __pad5[4];      // 60  align st_blocks to offset 64
   uint64_t st_blocks;     // 64  number of 512-byte blocks
   uint32_t st_atime;      // 72
   uint32_t st_atime_nsec; // 76
   uint32_t st_mtime;      // 80
   uint32_t st_mtime_nsec; // 84
   uint32_t st_ctime;      // 88
   uint32_t st_ctime_nsec; // 92
   uint64_t st_ino;        // 96  64-bit inode
} stat_t;                  // 104 bytes total

typedef struct linux_dirent64
{
   uint64_t d_ino;
   int64_t d_off;
   unsigned short d_reclen;
   unsigned char d_type;
   char d_name[];
} linux_dirent64_t;

typedef struct open_file_priv
{
   fat_dirent_t *dirent;
   fat_dirent_t *parent_dirent;
} open_file_priv_t;

int mount_read_w_offset(const char *path, uint8_t *buf, uint32_t size, uint32_t offset);
int mount_probe(const char *path);
fat_dirent_t *mount_get_dirent(const char *path, int use_cwd, int use_parent);

uint32_t mount_openat(int dfd, const char *path, int flags, int mode);
int mount_close(struct file *f);
ssize_t mount_read(struct file *f, void *buf, size_t n);
ssize_t mount_write(struct file *f, const void *buf, size_t n);
long mount_lseek(struct file *f, off_t offset, int whence);
long mount_ioctl(struct file *f, unsigned long cmd, unsigned long arg);
long mount_fcntl(struct file *f, unsigned cmd, unsigned long arg);

int mount_fstatat64(int dirfd, const char *path,
                    stat_t *statbuf,
                    int flags);
int mount_get_dirents64(uint32_t fd, linux_dirent64_t *dirp, uint32_t count);
char *get_basename(const char *path);
#endif
