format ELF
use32

public isr_128
extrn syscall_handler_c

section '.text' executable

isr_128:
    pusha
    push ds
    push es
    push fs
    push gs

    ; Cargar segmentos del kernel
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Offsets correctos DESPUÉS de los 4 push de segmentos
    mov eax, [esp+44]   ; EAX original del usuario (11 o 0)
    mov ebx, [esp+32]   ; EBX
    mov ecx, [esp+40]   ; ECX
    mov edx, [esp+36]   ; EDX
    mov esi, [esp+20]   ; ESI
    mov edi, [esp+16]   ; EDI

    ; Pasamos a C++ (cdecl: derecha a izquierda)
    push edi
    push esi
    push edx
    push ecx
    push ebx
    push eax

    call syscall_handler_c
    add esp, 24         ; Limpiar los 6 argumentos

    pop gs
    pop fs
    pop es
    pop ds
    popa
    iretd
