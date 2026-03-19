ENTRY(_start)

SECTIONS
{
    /* program load address */
    . = 0x40000000;

    /* ELF header needs to be visible */
    __ehdr_start = .;

    /* text */
    .text : {
        *(.text .text.*)
    }

    /* ARM unwind tables */
    . = ALIGN(4);
    __exidx_start = .;
    .ARM.exidx : {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    }
    __exidx_end = .;

    .ARM.extab : {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    }

    /* read-only data */
    .rodata : {
        *(.rodata .rodata.*)
    }

    /* init arrays (glibc requires these) */
    . = ALIGN(8);
    __preinit_array_start = .;
    .preinit_array : {
        KEEP(*(.preinit_array*))
    }
    __preinit_array_end = .;

    . = ALIGN(8);
    __init_array_start = .;
    .init_array : {
        KEEP(*(.init_array*))
    }
    __init_array_end = .;

    . = ALIGN(8);
    __fini_array_start = .;
    .fini_array : {
        KEEP(*(.fini_array*))
    }
    __fini_array_end = .;

    /* writable data */
    .data : {
        *(.data .data.*)
    }

    /* TLS (yes, even static glibc uses this) */
    .tdata : {
        *(.tdata .tdata.*)
    }
    .tbss : {
        *(.tbss .tbss.*)
    }

    /* zeroed data */
    . = ALIGN(8);
    __bss_start = .;
    .bss : {
        *(.bss .bss.* COMMON)
    }
    __bss_end = .;

    /* end of image */
    _end = .;
}