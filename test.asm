; red.asm — pinta un rectángulo rojo usando syscall 11 y luego exit
format ELF
use32
section '.text' executable
public _start
_start:
    ; int 0x80: SYS_DRAW_RECT (eax=11)
    ;   ebx = x = 100
    ;   ecx = y = 100
    ;   edx = w = 400
    ;   esi = h = 300
    ;   edi = color = 0x00FF0000 (rojo)
    mov eax, 11
    mov ebx, 100
    mov ecx, 100
    mov edx, 400
    mov esi, 300
    mov edi, 0x00FF0000
    int 0x80

    ; int 0x80: SYS_EXIT (eax=0)
    mov eax, 0
    int 0x80

    ; Por si acaso, bucle infinito
    jmp $
