format ELF
use32

public _start
extrn kernel_main

section '.text' executable

_start:
    call kernel_main
    jmp $
