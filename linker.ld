/* linker.ld - MiniOS bootloader
 * Arranque en 0x10000 (sector 1 del floppy cargado por boot.bin)
 * Salida: --oformat binary
 */

ENTRY(_start)

SECTIONS
{
    /* Dirección de carga del kernel */
    . = 0x10000;

    /* Código ejecutable */
    .text : {
        *(.text)
        *(.text.*)
    }

    /* Datos de solo lectura */
    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    /* Datos inicializados (van al binario) */
    .data : {
        *(.data)
        *(.data.*)
    }

    /* BSS: NO va al binario, se limpia en runtime.
       Usamos los nombres estándar __bss_start y __bss_end
       (con doble guion bajo) que espera entry.asm. */
    .bss : {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    }

    /* _end del kernel, alineado a 16 bytes.
       Se usa para inicializar el heap. */
    . = ALIGN(16);
    _kernel_end = .;
    _end = .;

    /* Descartar secciones basura que no queremos en el binario */
    /DISCARD/ : {
        *(.comment)
        *(.note.*)
        *(.eh_frame)
        *(.eh_frame_hdr)
    }
}
