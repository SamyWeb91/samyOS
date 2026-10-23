format ELF
use32
public _start

section '.text' executable

_start:
    ; -------------------------------------------------------------
    ; 1. Pintar un rectángulo llamando a sys_fb_flip_rect
    ; Suponiendo que implementaste SYS_FB_FLIP_RECT en syscall.h
    ; (Si no la tienes, dará error, pero el kernel no debería petar)
    ; -------------------------------------------------------------
    
    ; Primero, un sys_write para asegurar que el paso a ring 3 va bien
    mov eax, 4          ; SYS_WRITE
    mov ebx, 1          ; stdout
    mov ecx, msg
    mov edx, msg_len
    int 0x80

    ; -------------------------------------------------------------
    ; 2. Bucle infinito (simula el bucle de un escritorio)
    ; -------------------------------------------------------------
hang:
    jmp hang

section '.data' writeable
msg:     db '>>> HOLA DESDE RING 3 REAL! <<<', 10
msg_len = $ - msg
