/* user_linker.ld - Enlazador para programas de usuario (Ring 3) */
ENTRY(_start)

SECTIONS
{
    /* Dirección de memoria donde el cargador espera el código */
    . = 0x08000000;

    .text : {
        *(.text)
        *(.text.*)
    }

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    .data : {
        *(.data)
        *(.data.*)
    }

    .bss : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }
}
