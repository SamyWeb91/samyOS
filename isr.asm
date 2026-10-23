format ELF
use32

extrn isr_handler

section '.text' executable

; =====================================================================
; Layout de la pila al entrar a isr_common (coincide con struct Regs
; de isr.h en la Opción B):
;
;   [SS]         <- solo si cambió de ring (ring 3 -> ring 0)
;   [ESP]        <- solo si cambió de ring
;   [EFLAGS]
;   [CS]
;   [EIP]
;   [error_code] <- empujado por la CPU (o dummy 0 por el stub)
;   [num]        <- empujado por el stub
;   [EAX]        <- pusha
;   [ECX]
;   [EDX]
;   [EBX]
;   [ESP_dummy]  <- pusha empuja el ESP antes del pusha
;   [EBP]
;   [ESI]
;   [EDI]
;   [GS]         <- push gs (4 bytes, solo 16 bits útiles)
;   [FS]         <- push fs
;   [ES]         <- push es
;   [DS]         <- push ds
;   <-- ESP aquí es lo que se pasa a isr_handler como Regs*
;
; REGLA: TODOS los stubs deben empujar SIEMPRE 8 bytes (error_code + num).
;   - ISR_NOERR: push 0 (dummy error); push num        -> 8 bytes
;   - ISR_ERR:   la CPU ya empujó error_code; el stub
;                empuja num                              -> 4 + 4 = 8 bytes
;   El 'add esp, 8' final quita exactamente esos 8 bytes en ambos casos.
; =====================================================================

; ---------------------------------------------------------------------
; Macros
; ---------------------------------------------------------------------
macro ISR_NOERR num {
    public isr_#num
    isr_#num:
        push 0              ; dummy error_code (para unificar layout)
        push num            ; número de excepción
        jmp isr_common
}

macro ISR_ERR num {
    ; La CPU ya empujó el error_code real.
    public isr_#num
    isr_#num:
        push num            ; número de excepción
        jmp isr_common
}

macro IRQ num, irqnum {
    public irq_#num
    irq_#num:
        push 0              ; dummy error_code
        push (32 + irqnum)  ; vector
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
ISR_ERR   8     ; double fault (con error code)
ISR_NOERR 9
ISR_ERR   10    ; invalid TSS
ISR_ERR   11    ; segment not present
ISR_ERR   12    ; stack fault
ISR_ERR   13    ; general protection fault
ISR_ERR   14    ; page fault
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17    ; alignment check
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
ISR_ERR   30    ; security exception
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
    ; ABI x86: DF=0 al llamar a C.
    cld

    ; Guardar registros de propósito general.
    ; pusha deja en memoria (de menor a mayor):
    ;   [EDI][ESI][EBP][ESP_orig][EBX][EDX][ECX][EAX]
    pusha

    ; Guardar segmentos de datos del contexto interrumpido.
    ; IMPORTANTE: el orden push ds/es/fs/gs deja en memoria:
    ;   [GS][FS][ES][DS]  (de menor a mayor dirección)
    ; que coincide con struct Regs { gs, fs, es, ds }.
    push ds
    push es
    push fs
    push gs

    ; Cargar segmentos de datos del kernel (0x10).
    ; El handler C++ asume base 0 / límite 4 GB, que es lo que
    ; proporciona el descriptor 0x10 de la GDT.
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Pasar puntero al frame a isr_handler.
    ; ESP apunta a [DS], que es el primer campo (gs) de struct Regs.
    push esp
    call isr_handler
    add esp, 4

    ; Restaurar segmentos de datos del contexto interrumpido.
    ; Orden inverso al push: pop gs, pop fs, pop es, pop ds.
    pop gs
    pop fs
    pop es
    pop ds

    ; Restaurar registros de propósito general.
    popa

    ; Quitar 'num' (4 bytes) y 'error_code' (4 bytes: dummy o real).
    add esp, 8

    ; Volver al código interrumpido.
    ; iretd popea: EIP, CS, EFLAGS (y SS:ESP si cambió de ring).
    iretd
