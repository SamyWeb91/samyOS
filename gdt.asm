; =====================================================================
; gdt.asm - Cargar la GDT y recargar segmentos
; Compilar con: fasm gdt.asm gdt_asm.o
; =====================================================================
format ELF
use32

public gdt_load
public gdt_reload_segments

section '.text' executable

gdt_load:
    mov eax, [esp + 4]
    lgdt [eax]
    ret

gdt_reload_segments:
    ; Recargar CS con un salto lejano (far jump)
    ; El selector 0x08 es el codigo del kernel
    ; El offset es la direccion de la etiqueta .reload_cs
    push 0x08
    push .reload_cs
    retf

.reload_cs:
    ; Recargar los segmentos de datos
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret
