format ELF
use32

public isr_128
extrn syscall_handler

section '.text' executable

; int 0x80: syscall
; Similar a isr_common pero llama a syscall_handler en vez de isr_handler.
; El err_code y int_no son artificiales (push 0 y push 128).
isr_128:
    push 0              ; err_code falso
    push 128            ; int_no falso
    pusha
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push esp            ; puntero a Regs
    call syscall_handler
    add esp, 4
    popa
    add esp, 8          ; quitar int_no + err_code
    iretd
