format ELF
use32

public _start
extrn kernel_main

section '.text' executable

_start:
    ; Ceroear desde 0x20000 hasta 0x400000 (4 MiB)
    ; 0x20000 = 128 KB, suficiente si el kernel es menor
    mov edi, 0x20000
    mov ecx, 0x400000
    sub ecx, edi
    xor eax, eax
    rep stosb

    call kernel_main
    jmp $
