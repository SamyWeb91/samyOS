format binary
use32
org 0x08000000

_start:
    ; sys_write(4, msg, 18)
    mov eax, 4
    mov ebx, 0x08000800
    mov ecx, 18
    int 0x80

    ; sys_exit(1, 0)
    mov eax, 1
    xor ebx, ebx
    int 0x80

bucle:
    jmp bucle
