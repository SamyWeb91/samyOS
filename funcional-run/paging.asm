format ELF
use32

public paging_enable_asm

section '.text' executable

paging_enable_asm:
    mov eax, [esp + 4]
    mov cr3, eax
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax
    ret
