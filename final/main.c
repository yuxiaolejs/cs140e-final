#include "types.h"
#include "mem.h"
#include "pictl.h"
#include "sched.h"
#include "pifat.h"
#include "ll.h"
#include "vm.h"
#include "elf.h"
#include "timer.h"
#include "fd.h"
#include "tty.h"
#include "piusb.h"
#include "keyboard.h"
#include "fb.h"
#include "font.h"
#include "banners.h"
uint32_t *L1_page_table_base;

extern rpi_thread_t *cur;

fat_meta_t *fat_meta_obj;

void exec(char *filename)
{
    fat32_file_list_entry_t *list = fat32_list_fat(fat_meta_obj, fat_meta_obj->bpb.root_clus);
    fat32_file_list_entry_t *targ = 0;
    struct list_head *pos;
    for (pos = list->list.next; pos != &list->list; pos = pos->next)
    {
        fat32_file_list_entry_t *entry = container_of(pos, fat32_file_list_entry_t, list);
        if (strn_eq(filename, (char *)entry->dirent.name, 11))
        {
            targ = entry;
            break;
        }
        else
        {
            printk("File not match: %s\n", entry->dirent.name);
        }
    }
    if (!targ)
    {
        printk("exec: file %s not found\n", filename);
        return;
    }
    printk("Found file %s in FAT32 root dir, size %u bytes, starting cluster %u\n", filename, targ->dirent.file_size,
           ((uint32_t)targ->dirent.fst_clus_hi << 16) | targ->dirent.fst_clus_lo);
    uint32_t fsize = targ->dirent.file_size;
    uint8_t *file_data = fat32_read_file(fat_meta_obj,
                                         ((uint32_t)targ->dirent.fst_clus_hi << 16) | targ->dirent.fst_clus_lo,
                                         targ->dirent.file_size);
    printk("User file %s found, size %u bytes, data at %x\n", filename, fsize, (uint32_t)file_data);
    free_fat32_file_list_entry_t_list(list);
    uint32_t code_start = load_elf(cur, (uint32_t *)file_data, fsize, 1, (char *[]){"sh"}, (char *[]){"PATH=/bin", "TERM=linux", NULL});
    if (code_start == ELF_ERR)
    {
        printk("Failed to load ELF file %s\n", filename);
        return;
    }
    uint32_t user_sp = cur->ctx.user_sp; // use the stack pointer set up by init_user_stack_minimal
    cur->cwd = kalloc(2);
    cur->cwd[0] = '/';
    cur->cwd[1] = '\0';
    printk("Dropping to user code %s at %x with size %u, sp=%x\n", filename, code_start, fsize, user_sp);
    // Here we need to cleanup the allocated mem before it OOMs
    kfree(file_data);

    *PTR_IRQ_ENABLE_BASIC = 1; // Enable Timer IRQ
    drop_to_user(user_sp, (uint32_t)code_start);
}

void notmain(void)
{
    printk("Hello, kernel world!\n");
    kalloc_init();
    printk("Init interrupt table\n");
    interrupts_table_init();
    printk("Init stacks\n");
    init_all_stacks();
    printk("Init IRQ stack pointer\n");
    init_irq_stack_pointer();
    printk("Init TTY\n");
    tty_init();
#ifdef USE_PHYSICAL_TTY
    printk("Use physical TTY, initializing USB and keyboard\n");
    do_usb();
    printk("Init framebuffer\n");
    framebuffer_init(font_8x16);
#endif
    printk("Enabling interrupts\n");
    set_irq_enabled(true); // Only after USB: need to mask USB IRQ first
    uint32_t my_lr;
    asm volatile("mov %0, lr" : "=r"(my_lr));
    printk("In notmain, LR=%x\n", my_lr);
    L1_page_table_base = mmu_init();
    cur->l1_pt = L1_page_table_base; // save into thread struct
    cur->tid = 1;
    cur->ready = true;
    kuser_verify();
    printk("Current CPSR: 0x%x\n", read_cpsr());
    int intterrupts_enabled = !(read_cpsr() & (1 << 7));
    if (intterrupts_enabled)
    {
        printk("Interrupts are enabled.\n");
    }
    else
    {
        printk("Interrupts are disabled.\n");
    }
    printk("enabling timer\n");
    // enable_timer(0xff, TIMER_PRE_256); // Set timer interval - much slower
    enable_timer(0xff, TIMER_PRE_16); // Set timer interval - much slower
    printk("Enabling timer IRQ\n");
    // before we start, assert table vector base is set
    uint32_t table_base;
    asm volatile("mrc p15, 0, %0, c12, c0, 0" : "=r"(table_base));
    printk("Interrupt Vector Table Base: 0x%x\n", table_base);

    // printk("running on IRQ now\n\n");
    // // stop here to check usb
    // while (1)
    // {
    // }
    printk("Initializing eMMC and FAT32\n");
    emmc_init();
    fat_meta_obj = fat32_init();
    printk("Init fdtable for current thread\n");
    cur->fdt = init_fdtable();
    printk("Initialized fdtable at %x\n", (uint32_t)cur->fdt);
    // exec("TEST    ELF");
    while (*linux_banner)
    {
        terminal_putc(*linux_banner++);
    }
    exec("SH      ELF");
    kernel_replace_file("/dmode/kernel.elf", 0xc0008000);

    rpi_reboot();
}
