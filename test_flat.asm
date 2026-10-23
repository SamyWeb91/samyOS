use32
org 0x400000

_start:
    ; sys_write(1, msg, msg_len)
    mov eax, 4
    mov ebx, 1
    mov ecx, msg
    mov edx, msg_len
    int 0x80

    ; sys_exit(0)
    mov eax, 1
    xor ebx, ebx
    int 0x80

    jmp $

msg:     db '>>> RING 3 PLANO OK! <<<', 10
msg_len = $ - msg
