section .text
global _start

_start:
    ; Pintar el framebuffer
    mov edi, 0xE0000000
    mov ecx, 1024 * 768
    mov eax, 0x00FF0000
.loop:
    mov [edi], eax
    add edi, 4
    dec ecx
    jnz .loop

    ; Salir llamando a syscall
    mov eax, 0
    int 0x80

.halt:
    jmp .halt
