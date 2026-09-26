format ELF
use32

public syscall_stub
extrn syscall_handler

section '.text' executable

syscall_stub:
    pusha

    ; Argumentos: EAX = num, EBX, ECX, EDX
    push edx
    push ecx
    push ebx
    push eax
    call syscall_handler
    add esp, 16

    ; Guardar el resultado en EAX dentro de la pila de pusha
    ; (el EAX guardado está al final de pusha)
    mov [esp + 28], eax

    popa
    iretd
