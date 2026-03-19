#include "krep.h"
#include "mem.h"
#include "pifat.h"
#include "pictl.h"
#include "sched.h"
#include "errno.h"
#include "fb.h"

extern volatile uint32_t krep_mem_move_start;
extern volatile uint32_t krep_mem_move_end;
extern fat_meta_t *fat_meta_obj;

int kernel_replace(uint32_t target_addr, const void *new_code, uint32_t code_size)
{
    // MMU off
    disable_mmu();
    set_irq_enabled(false);
    // maybe other clear up routines
    framebuffer_free();
    printk("Mover code start: %x, end: %x\n", &krep_mem_move_start, &krep_mem_move_end);
    uint32_t mover_code_size = (uint32_t)&krep_mem_move_end - (uint32_t)&krep_mem_move_start;
    force_printk("Mover code size: %u bytes\n", mover_code_size);
    uint32_t *mover_code = kalloc(mover_code_size);
    // copy the mover code to the new location
    memmove(mover_code, (void *)&krep_mem_move_start, mover_code_size);
    // call the mover code to move the new code to the target address
    void (*mover_func)(uint32_t, const void *, uint32_t, void (*)(void)) = (void (*)(uint32_t, const void *, uint32_t, void (*)(void)))mover_code;
    printk("Jumping to mover code at %x to move new kernel code to %x\n", (uint32_t)mover_func, target_addr);
    mover_func(target_addr, new_code, code_size, target_addr);
    // unreachable: boot into new kernel
    force_printk("Error: kernel_replace should not return\n");
    force_printk("Error: kernel_replace should not return\n");
    force_printk("Error: kernel_replace should not return\n");
    force_printk("Error: kernel_replace should not return\n");
    force_printk("Error: kernel_replace should not return\n");
    return -114; // just in case it does return, return an error code that won't be mistaken for a valid address
}

int kernel_replace_file(const char *new_kernel_file_path, uint32_t new_kernel_addr)
{
    char parent[256];
    char name[256];
    char safe[256];
    force_printk("replace_kernel_syscall: path='%s'\n", new_kernel_file_path);
    safe_path(new_kernel_file_path, safe);
    printk("After safe_path: safe='%s'\n", safe);
    if (split_path(safe, parent, name) != 0)
        return -ENOENT;
    force_printk("replace_kernel_syscall: parent='%s' name='%s'\n", parent, name);

    uint32_t parent_clus = get_dir_cluster_number(parent);
    if (parent_clus == 0)
    {
        force_printk("replace_kernel_syscall: parent directory '%s' not found\n", parent);
        return -ENOENT;
    }
    fat_dirent_t *entry = get_entry_from_directory(parent_clus, name, 0);
    if (!entry)
    {
        force_printk("replace_kernel_syscall: file '%s' not found in parent directory '%s'\n", name, parent);
        return -ENOENT;
    }
    uint32_t start_clus = ((uint32_t)entry->fst_clus_hi << 16) | entry->fst_clus_lo;
    uint32_t fsize = entry->file_size;
    if (fsize == 0)    {
        force_printk("replace_kernel_syscall: file '%s' has size 0, cannot replace kernel with empty file\n", name);
        return -EINVAL;
    }
    uint8_t *file_data = fat32_read_file(fat_meta_obj, start_clus, fsize);
    printk("replace_kernel_syscall: Kernel file %s found, size %u bytes, data at %x\n", name, fsize, (uint32_t)file_data);
    kernel_replace(new_kernel_addr, file_data, fsize);
}

// /rpk.elf /dmode/kernel.elf 0xc0008000