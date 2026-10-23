format ELF
use32
public _start

section '.text' executable
_start:
    ; sys_write(1, msg, msg_len)
    mov eax, 4       ; SYS_WRITE
    mov ebx, 1       ; fd = stdout
    mov ecx, msg     ; puntero al mensaje
    mov edx, msg_len ; longitud
    int 0x80         ; Llamada al sistema!

    ; sys_exit(0)
    mov eax, 1       ; SYS_EXIT
    xor ebx, ebx     ; codigo de salida 0
    int 0x80

    jmp $            ; por si acaso

section '.data' writeable
msg:     db '>>> RING 3 OK! <<<', 10
msg_len = $ - msg
