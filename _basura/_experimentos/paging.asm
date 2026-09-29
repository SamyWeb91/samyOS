; =====================================================================
; paging.asm - Rutinas de paginacion
; Compilar con: fasm paging.asm paging_asm.o
; =====================================================================
format ELF
use32

public paging_enable

section '.text' executable

; =====================================================================
; paging_enable
; Entrada: [esp+4] = direccion fisica del Page Directory
; =====================================================================
paging_enable:
    mov eax, [esp + 4]      ; EAX = direccion del PD

    ; Cargar CR3 con la direccion del PD
    mov cr3, eax

    ; Activar paginacion: bit 31 de CR0
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    ret
