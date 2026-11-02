format ELF
use32

extrn isr_handler

section '.text' executable

; =====================================================================
; Macros
;
; IMPORTANTE: el CPU pushea el error code SOLO para algunas excepciones.
; Para las que no lo pushean, el stub debe pushear un 0 dummy.
; Para las que sí lo pushean, el stub NO debe pushear nada extra.
;
; El orden final en la pila (direcciones crecientes) debe ser:
;   ... int_no, err_code, EIP, CS, EFLAGS [, useresp, ss]
; =====================================================================

; Excepción SIN error code: el stub pushea un 0 como err_code
macro ISR_NOERR num {
    public isr_#num
    isr_#num:
        push 0              ; err_code dummy
        push num            ; int_no
        jmp isr_common
}

; Excepción CON error code: el CPU ya pusheó el err_code,
; el stub solo pushea el int_no
macro ISR_ERR num {
    public isr_#num
    isr_#num:
        push num            ; int_no (err_code ya está en la pila)
        jmp isr_common
}

; IRQ: nunca tiene error code
macro IRQ num, irqnum {
    public irq_#num
    irq_#num:
        push 0              ; err_code dummy
        push (32 + irqnum)  ; int_no
        jmp isr_common
}

; =====================================================================
; Excepciones 0-31
;
; Las que pushean error code (Intel SDM Vol 3, Tabla 6-1):
;   8 (#DF), 10 (#TS), 11 (#NP), 12 (#SS), 13 (#GP), 14 (#PF),
;   17 (#AC), 21 (#CP), 29 (#VC), 30 (#SX)
; =====================================================================
ISR_NOERR 0     ; #DE
ISR_NOERR 1     ; #DB
ISR_NOERR 2     ; NMI
ISR_NOERR 3     ; #BP
ISR_NOERR 4     ; #OF
ISR_NOERR 5     ; #BR
ISR_NOERR 6     ; #UD
ISR_NOERR 7     ; #NM
ISR_ERR   8     ; #DF  (error code = 0)
ISR_NOERR 9     ; coprocessor segment overrun (obsoleto)
ISR_ERR   10    ; #TS
ISR_ERR   11    ; #NP
ISR_ERR   12    ; #SS
ISR_ERR   13    ; #GP
ISR_ERR   14    ; #PF
ISR_NOERR 15    ; reserved
ISR_NOERR 16    ; #MF
ISR_ERR   17    ; #AC
ISR_NOERR 18    ; #MC
ISR_NOERR 19    ; #XM
ISR_NOERR 20    ; #VE
ISR_ERR   21    ; #CP
ISR_NOERR 22    ; reserved
ISR_NOERR 23    ; reserved
ISR_NOERR 24    ; reserved
ISR_NOERR 25    ; reserved
ISR_NOERR 26    ; reserved
ISR_NOERR 27    ; reserved
ISR_NOERR 28    ; #HV
ISR_ERR   29    ; #VC
ISR_ERR   30    ; #SX
ISR_NOERR 31    ; reserved

; =====================================================================
; IRQs 0-15 (remapeadas a 32-47)
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
;
; En este punto la pila contiene (direcciones crecientes):
;   [ESP+0] = int_no
;   [ESP+4] = err_code
;   [ESP+8] = EIP
;   [ESP+12]= CS
;   [ESP+16]= EFLAGS
;   [ESP+20]= useresp  (solo si cambió de ring)
;   [ESP+24]= ss       (solo si cambió de ring)
;
; El objetivo es construir el struct Regs y llamar a isr_handler.
; =====================================================================
isr_common:
    ; 1) Guardar registros de propósito general
    pusha

    ; 2) Guardar los selectores de datos actuales
    push ds
    push es
    push fs
    push gs

    ; 3) Cargar selectores de datos del kernel
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; 4) Llamar al handler en C.
    ;    ESP apunta al inicio del struct Regs (gs, fs, es, ds, ...)
    push esp
    call isr_handler
    add esp, 4

    ; 5) Restaurar selectores de datos originales
    pop gs
    pop fs
    pop es
    pop ds

    ; 6) Restaurar registros de propósito general
    popa

    ; 7) Descartar int_no y err_code
    add esp, 8

    ; 8) Volver al contexto interrumpido
    iretd
