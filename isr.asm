format ELF
use32

extrn isr_handler

section '.text' executable

; =====================================================================
; Macros
; =====================================================================
macro ISR_NOERR num {
    public isr_#num
    isr_#num:
        push 0
        push num
        jmp isr_common
}

macro ISR_ERR num {
    public isr_#num
    isr_#num:
        push num
        jmp isr_common
}

macro IRQ num, irqnum {
    public irq_#num
    irq_#num:
        push 0
        push (32 + irqnum)
        jmp isr_common
}

; =====================================================================
; Excepciones 0-31
; =====================================================================
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30
ISR_NOERR 31

; =====================================================================
; IRQs 0-15
; =====================================================================
IRQ 0, 0
IRQ 1, 1
IRQ 2, 2
IRQ 3, 3
IRQ 4, 4
IRQ 5, 5
IRQ 6, 6
IRQ 7, 7
IRQ 8, 8
IRQ 9, 9
IRQ 10, 10
IRQ 11, 11
IRQ 12, 12
IRQ 13, 13
IRQ 14, 14
IRQ 15, 15

; =====================================================================
; isr_common
; =====================================================================
isr_common:
    pusha
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push esp
    call isr_handler
    add esp, 4
    popa
    add esp, 8
    iretd
