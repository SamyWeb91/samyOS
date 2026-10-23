ENTRY(_start)
SECTIONS
{
    . = 0x400000;

    .text : {
        *(.text._start)
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.data*)
        *(.bss*)
        *(.got*)
        *(.plt*)
    }
}
