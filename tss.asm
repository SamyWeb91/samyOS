; =====================================================================
; tss.asm - Cargar el TSS en el registro TR
; Compilar con: fasm tss.asm tss_asm.o
; =====================================================================
format ELF
use32

public tss_flush

section '.text' executable

tss_flush:
    mov ax, [esp + 4]
    ltr ax
    ret
