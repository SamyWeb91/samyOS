format ELF
use32

public tss_flush

section '.text' executable

; Carga el TSS en el registro TR.
; El selector 0x28 = índice 5 * 8 + RPL 0.
tss_flush:
    mov ax, 0x28
    ltr ax
    ret
