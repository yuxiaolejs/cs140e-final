#ifndef ELF_H
#define ELF_H
#include "types.h"
#include "sched.h"

//  PLATFORM DEPENDENT DEFINES

#define ELF_ERR ((uint32_t)0xFFFFFFFF)
#define PAGE_SZ 4096u
#define ALIGN_DOWN(x, a) ((uint32_t)(x) & ~((a) - 1u))
#define ALIGN_UP(x, a) (((uint32_t)(x) + ((a) - 1u)) & ~((a) - 1u))

#define USER_TOP 0xC0000000u

#define STACK_TOP USER_TOP - 0x10
#define STACK_PAGES 10
#define STACK_BASE (STACK_TOP - STACK_PAGES * PAGE_SZ)

//  ELF SPECIFIC STUFF

#define EI_NIDENT 16

/* e_ident[] indexes */
#define EI_MAG0 0
#define EI_MAG1 1
#define EI_MAG2 2
#define EI_MAG3 3
#define EI_CLASS 4
#define EI_DATA 5
#define EI_VERSION 6

/* ELF magic */
#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

/* ELF class */
#define ELFCLASS32 1
#define ELFCLASS64 2

/* Endianness */
#define ELFDATA2LSB 1
#define ELFDATA2MSB 2

/* e_type */
#define ET_EXEC 2
#define ET_DYN 3

/* e_machine */
#define EM_ARM 40 /* IMPORTANT: 40, not 8 */

/* Program header types */
#define PT_NULL 0
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_NOTE 4
#define PT_TLS 7
#define PT_ARM_EXIDX 0x70000001

/* p_flags */
#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

/* Auxiliary vector keys (Linux ABI) */
#define AT_NULL 0      /* End of auxv */
#define AT_IGNORE 1    /* Entry should be ignored */
#define AT_EXECFD 2    /* File descriptor of program */
#define AT_PHDR 3      /* Program headers for program */
#define AT_PHENT 4     /* Size of program header entry */
#define AT_PHNUM 5     /* Number of program headers */
#define AT_PAGESZ 6    /* System page size */
#define AT_BASE 7      /* Base address of interpreter */
#define AT_FLAGS 8     /* Flags */
#define AT_ENTRY 9     /* Entry point of program */
#define AT_NOTELF 10   /* Program is not ELF */
#define AT_UID 11      /* Real uid */
#define AT_EUID 12     /* Effective uid */
#define AT_GID 13      /* Real gid */
#define AT_EGID 14     /* Effective gid */
#define AT_PLATFORM 15 /* String identifying CPU */
#define AT_HWCAP 16    /* CPU capability hints */
#define AT_CLKTCK 17   /* Frequency of times() */

/* Extensions */
#define AT_SECURE 23 /* Secure mode boolean */
#define AT_BASE_PLATFORM 24
#define AT_RANDOM 25 /* Address of 16 random bytes */
#define AT_HWCAP2 26
#define AT_EXECFN 31 /* Filename of executable */

/// UTIL
#define PUSH(sp, v)                        \
    do                                     \
    {                                      \
        (sp) -= 4;                         \
        *(uint32_t *)(sp) = (uint32_t)(v); \
    } while (0)

typedef struct
{
    unsigned char e_ident[EI_NIDENT]; // ELF magic + ABI info
    uint16_t e_type;                  // ET_EXEC, ET_DYN, ...
    uint16_t e_machine;               // EM_ARM
    uint32_t e_version;               // EV_CURRENT
    uint32_t e_entry;                 // Entry point VA
    uint32_t e_phoff;                 // Program header table offset
    uint32_t e_shoff;                 // Section header table offset
    uint32_t e_flags;                 // ARM EABI flags
    uint16_t e_ehsize;                // sizeof(elf32_ehdr)
    uint16_t e_phentsize;             // sizeof(elf32_phdr)
    uint16_t e_phnum;                 // number of program headers
    uint16_t e_shentsize;             // sizeof(Elf32_Shdr)
    uint16_t e_shnum;                 // number of section headers
    uint16_t e_shstrndx;              // section header string table
} elf32_ehdr;

typedef struct
{
    uint32_t p_type;   // PT_LOAD, PT_TLS, ...
    uint32_t p_offset; // file offset
    uint32_t p_vaddr;  // virtual address
    uint32_t p_paddr;  // ignore (Linux does)
    uint32_t p_filesz; // bytes in file
    uint32_t p_memsz;  // bytes in memory
    uint32_t p_flags;  // PF_R | PF_W | PF_X
    uint32_t p_align;  // alignment (usually 0x1000)
} elf32_phdr;

uint32_t load_elf(rpi_thread_t *pcb, uint32_t *elf_data, uint32_t elf_size, int argc, char **argv, char **envp);

uint32_t init_user_stack_minimal(
    rpi_thread_t *pcb,
    const char *filename,
    uint8_t *elf_data,
    uint32_t entry_pc);
static inline uint32_t u32_max(uint32_t a, uint32_t b) { return a > b ? a : b; }
#endif