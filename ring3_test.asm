format binary
use32
org 0x08000000

_start:
    ; sys_write(1, msg, 18)
    mov eax, 4       ; SYS_WRITE
    mov ebx, 0x08000800
    mov ecx, 18
    int 0x80

    ; sys_exit(0)
    mov eax, 1       ; SYS_EXIT
    xor ebx, ebx
    int 0x80

bucle:
    jmp bucle
