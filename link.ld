ENTRY(loader)                /* the name of the entry label */

SECTIONS {
    . = 1M;

    kernel_physical_start = .;

    .boot :
    {
        *(.multiboot_header)
    }

    .early_text :
    {
        *(.early_text)
    }

    .early_rodata :
    {
        *(.early_rodata)
    }

    .early_bss ALIGN (0x1000) :
    {
        *(.early_bss)
    }

    . += 0xffff800000000000;

    .text ALIGN (0x1000) : AT(ADDR(.text) - 0xffff800000000000)
    {
        /* Merges together sections for methods */
        *(.text .text.*)
    }

    .rodata ALIGN (0x1000) : AT(ADDR(.rodata) - 0xffff800000000000)
    {
        *(.rodata*)
    }

    .data ALIGN (0x1000) : AT(ADDR(.data) - 0xffff800000000000)
    {
        *(.data)
    }

    .bss ALIGN (0x1000) : AT(ADDR(.bss) - 0xffff800000000000)
    {
        *(COMMON)
        *(.bss)
    }

    kernel_physical_end = . - 0xffff800000000000;
}
