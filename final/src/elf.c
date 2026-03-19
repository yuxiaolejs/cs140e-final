#include "elf.h"
#include "mem.h"
#include "vm.h"

uint32_t load_elf(rpi_thread_t *pcb, uint32_t *elf_data, uint32_t elf_size, int argc, char **argv, char **envp)
{
    uint8_t *elf = (uint8_t *)elf_data;
    elf32_ehdr *eh = (elf32_ehdr *)elf;

    pcb->tls_base = 0;
    pcb->brk_min = 0;
    pcb->brk = 0;

    if (eh->e_ident[0] != 0x7F || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L' || eh->e_ident[3] != 'F')
    {
        printk("Invalid ELF magic\n");
        return ELF_ERR;
    }

    if (eh->e_ident[EI_CLASS] != ELFCLASS32)
    {
        printk("Unsupported ELF class\n");
        return ELF_ERR;
    }

    if (eh->e_ident[EI_DATA] != ELFDATA2LSB)
    {
        printk("Unsupported ELF endianness\n");
        return ELF_ERR;
    }

    if (eh->e_machine != EM_ARM)
    {
        printk("Unsupported ELF machine (need EM_ARM)\n");
        return ELF_ERR;
    }

    if (eh->e_phoff == 0 || eh->e_phnum == 0)
    {
        printk("No program headers\n");
        return ELF_ERR;
    }

    uint32_t max_rw_end = 0;
    for (int i = 0; i < eh->e_phnum; i++)
    {
        elf32_phdr *ph = (elf32_phdr *)(elf + eh->e_phoff + (uint32_t)i * sizeof(elf32_phdr));
        if (ph->p_type != PT_LOAD)
            continue;

        uint32_t va_start = ALIGN_DOWN(ph->p_vaddr, PAGE_SZ);
        uint32_t va_end = ALIGN_UP(ph->p_vaddr + ph->p_memsz, PAGE_SZ);
        uint32_t page_off = ph->p_vaddr - va_start;

        make_sure_user_space(pcb->l1_pt, (void *)va_start, va_end - va_start);

        uint8_t *dest = (uint8_t *)(va_start + page_off);
        uint8_t *src = elf + ph->p_offset;

        memcpy(dest, src, ph->p_filesz);
        memset(dest + ph->p_filesz, 0, ph->p_memsz - ph->p_filesz);

        // Sync caches: clean dcache so data is visible to userland, invalidate icache for code
        code_onload_cache_sync((void *)va_start, va_end - va_start);

        if (ph->p_flags & PF_W)
            max_rw_end = u32_max(max_rw_end, ph->p_vaddr + ph->p_memsz);
    }

    pcb->brk_min = ALIGN_UP(max_rw_end, PAGE_SZ);
    pcb->brk = pcb->brk_min;

    // Now loading is done, need to setup stack (kernel dependent)
    // Since stack grows, we just allocate one page for initial args (FIXME later if we allow cmdline args)
    char *filename = "elf_prog";
    make_sure_user_space(pcb->l1_pt, ALIGN_DOWN(STACK_TOP, 4096), 4096);
    memset((void *)ALIGN_DOWN(STACK_TOP, 4096), 0, 4096);
    code_onload_cache_sync((void *)ALIGN_DOWN(STACK_TOP, 4096), 4096);

    // Find AT_PHDR value (virtual address of program headers)
    elf32_phdr *phdrs = (elf32_phdr *)((uint8_t *)elf_data + eh->e_phoff);
    uint32_t at_phdr = 0;
    for (int i = 0; i < eh->e_phnum; i++)
    {
        elf32_phdr *ph = &phdrs[i];
        if (ph->p_type == PT_LOAD &&
            eh->e_phoff >= ph->p_offset &&
            eh->e_phoff < ph->p_offset + ph->p_filesz)
        {
            at_phdr = ph->p_vaddr + (eh->e_phoff - ph->p_offset);
            break;
        }
    }

    uint32_t sp = STACK_TOP;
    // size_t len = strlen(filename) + 1;
    // sp -= len;
    // char *argv0 = (char *)sp;
    // memcpy(argv0, filename, len);

    // generic "arg"
    for (int i = 0; argv && i < argc && argv[i]; i++)
    {
        // printk("Copying arg %d: '%s'\n", i, argv[i]);
        size_t len = strlen(argv[i]) + 1;
        sp -= len;
        sp = ALIGN_DOWN(sp, 8);
        char *argp = (char *)sp;
        memcpy(argp, argv[i], len);
        argv[i] = argp; // update to point to user space
    }

    // reverse argv
    for (int i = 0; i < argc / 2; i++)
    {
        char *tmp = argv[i];
        argv[i] = argv[argc - 1 - i];
        argv[argc - 1 - i] = tmp;
    }

    // generic "env"
    for (int i = 0; envp && envp[i]; i++)
    {
        // printk("Copying env %d: '%s'\n", i, envp[i]);
        size_t len = strlen(envp[i]) + 1;
        sp -= len;
        sp = ALIGN_DOWN(sp, 8);
        char *envp_i = (char *)sp;
        memcpy(envp_i, envp[i], len);
        envp[i] = envp_i; // update to point to user space
    }

    sp = ALIGN_DOWN(sp, 8);

    sp -= 16; // random stuff
    memset((void *)sp, 0x11, 16);
    void *at_random = (void *)sp;

    // auxv - pushed in reverse order (AT_NULL must be last/first pushed)
    // All of these are for you LIBC!!! FUCK YOU!!!
    PUSH(sp, 0);
    PUSH(sp, AT_NULL);

    PUSH(sp, at_random);
    PUSH(sp, AT_RANDOM);

    PUSH(sp, 0); // no HWCAP2
    PUSH(sp, AT_HWCAP2);

    PUSH(sp, 0x197); // ARM HWCAP flags (no VFP - bit 6 removed to avoid undefined instruction)
    PUSH(sp, AT_HWCAP);

    PUSH(sp, 0); // egid
    PUSH(sp, AT_EGID);

    PUSH(sp, 0); // gid
    PUSH(sp, AT_GID);

    PUSH(sp, 0); // euid
    PUSH(sp, AT_EUID);

    PUSH(sp, 0); // uid
    PUSH(sp, AT_UID);

    PUSH(sp, eh->e_entry);
    PUSH(sp, AT_ENTRY);

    PUSH(sp, eh->e_phnum);
    PUSH(sp, AT_PHNUM);

    PUSH(sp, sizeof(elf32_phdr));
    PUSH(sp, AT_PHENT);

    PUSH(sp, at_phdr);
    PUSH(sp, AT_PHDR);

    PUSH(sp, PAGE_SZ);
    PUSH(sp, AT_PAGESZ);

    PUSH(sp, 0); // envp
    for (int i = 0; envp && envp[i]; i++)
    {
        PUSH(sp, envp[i]);
    }

    PUSH(sp, 0); // argv[n]
    for (int i = 0; argv && i < argc && argv[i]; i++)
    {
        PUSH(sp, argv[i]);
    }

    PUSH(sp, argc); // argc

    pcb->ctx.user_sp = sp;

    return eh->e_entry;
}
