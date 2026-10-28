section .text
global _start

_start:
    ; Pintar la pantalla de rojo (direccion fisica del framebuffer de tu captura: 0xE0000000)
    mov edi, 0xE0000000
    mov ecx, 1024 * 768
    mov eax, 0x00FF0000 ; Color rojo (BGRA)
.loop:
    mov [edi], eax
    add edi, 4
    dec ecx
    jnz .loop

.halt:
    jmp .halt
