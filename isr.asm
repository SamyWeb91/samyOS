; =====================================================================
; isr.asm - Stubs de interrupciones y handler comun con cambio de tarea
; Compilar con: fasm isr.asm isr.o
; =====================================================================
format ELF
use32

; =====================================================================
; Funciones externas (definidas en C++)
; =====================================================================
extrn isr_handler
extrn task_save_esp
extrn schedule_next
extrn task_set_current
extrn task_get_current

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
;
; Pila al entrar (despues de pusha):
;   [esp+0]  = EDI
;   [esp+4]  = ESI
;   [esp+8]  = EBP
;   [esp+12] = ESP
;   [esp+16] = EBX
;   [esp+20] = EDX
;   [esp+24] = ECX
;   [esp+28] = EAX
;   [esp+32] = err_code
;   [esp+36] = int_no      <-- aqui
;   [esp+40] = EIP
;   [esp+44] = CS
;   [esp+48] = EFLAGS
;
; Espera: el stub IRQ empuja primero err_code, luego int_no.
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

    ; Comprobar si es IRQ0 (int_no == 32)
    mov eax, [esp + 32]
    cmp eax, 32
    jne .no_switch

    ; --- Es IRQ0: cambiar de tarea ---
    push esp
    call task_save_esp
    add esp, 4

    call schedule_next
    push eax
    call task_set_current
    add esp, 4

    call task_get_current
    mov eax, [eax + 8]      ; current_task->esp
    mov esp, eax            ; <-- CAMBIO DE PILA

.no_switch:
    popa
    add esp, 8
    iretd
