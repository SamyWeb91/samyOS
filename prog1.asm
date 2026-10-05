format binary
use32
org 0x400000

start:
    mov eax, 1
    mov ebx, msg
    mov ecx, msg_end - msg
    int 0x80

    mov eax, 2
    mov ebx, 0
    int 0x80

    jmp $

msg:
    db "Hola desde binario en ring 3!", 10
msg_end:

times 4096 - ($ - $$) db 0
