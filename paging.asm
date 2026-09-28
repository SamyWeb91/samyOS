; =====================================================================
; paging.asm - Activar paginacion
; Compilar con: fasm paging.asm paging_asm.o
; =====================================================================
format ELF
use32

public paging_enable_asm

section '.text' executable

paging_enable_asm:
    mov eax, [esp + 4]      ; EAX = direccion del PD
    mov cr3, eax

    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    ret
