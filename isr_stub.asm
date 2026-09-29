; isr_stub.asm — Stubs de interrupciones para MiniOS
BITS 32

section .text

; ---------------------------------------------------------------
; Macros para stubs
; ---------------------------------------------------------------

; Excepciones que SÍ empujan código de error
%macro ISR_ERR 1
global isr%1
isr%1:
    push dword %1
    jmp isr_common
%endmacro

; Excepciones que NO empujan código de error (empujamos 0 dummy)
%macro ISR_NOERR 1
global isr%1
isr%1:
    push dword 0
    push dword %1
    jmp isr_common
%endmacro

; IRQs (siempre sin err_code)
%macro IRQ 2
global irq%1
irq%1:
    push dword 0
    push dword %2
    jmp isr_common
%endmacro

; ---------------------------------------------------------------
; Excepciones 0..31
; ---------------------------------------------------------------
ISR_NOERR 0    ; Divide by zero
ISR_NOERR 1    ; Debug
ISR_NOERR 2    ; NMI
ISR_NOERR 3    ; Breakpoint
ISR_NOERR 4    ; Overflow
ISR_NOERR 5    ; Bound range
ISR_NOERR 6    ; Invalid opcode
ISR_NOERR 7    ; Device not available
ISR_ERR   8    ; Double fault
ISR_NOERR 9    ; Coprocessor segment overrun
ISR_ERR   10   ; Invalid TSS
ISR_ERR   11   ; Segment not present
ISR_ERR   12   ; Stack fault
ISR_ERR   13   ; General protection fault
ISR_ERR   14   ; Page fault
ISR_NOERR 15   ; Reserved
ISR_NOERR 16   ; x87 FP
ISR_ERR   17   ; Alignment check
ISR_NOERR 18   ; Machine check
ISR_NOERR 19   ; SIMD FP
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
ISR_ERR   30   ; Security exception
ISR_NOERR 31

; ---------------------------------------------------------------
; IRQs 0..15 → int_no = 32..47
; ---------------------------------------------------------------
IRQ 0,  32     ; Timer
IRQ 1,  33     ; Keyboard
IRQ 2,  34     ; Cascade
IRQ 3,  35     ; COM2
IRQ 4,  36     ; COM1
IRQ 5,  37     ; LPT2
IRQ 6,  38     ; Floppy
IRQ 7,  39     ; LPT1
IRQ 8,  40     ; RTC
IRQ 9,  41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44     ; Mouse
IRQ 13, 45     ; FPU
IRQ 14, 46     ; ATA primary
IRQ 15, 47     ; ATA secondary

; ---------------------------------------------------------------
; isr_common: guarda contexto, llama a C, cambia de tarea si es IRQ0
; ---------------------------------------------------------------
extern isr_handler
extern task_save_current_esp
extern schedule_and_switch

isr_common:
    pusha                       ; EAX,ECX,EDX,EBX,ESP,EBP,ESI,EDI (32 bytes)

    ; Guardar segmentos de datos
    push ds
    push es
    push fs
    push gs

    ; Cargar selector de datos del kernel (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Llamar al handler en C pasando puntero al frame
    push esp
    call isr_handler
    add esp, 4

    ; Leer int_no desde el frame.
    ; Layout desde esp (tras pusha + 4 segmentos):
    ;   [esp+0]  GS
    ;   [esp+4]  FS
    ;   [esp+8]  ES
    ;   [esp+12] DS
    ;   [esp+16] EDI
    ;   [esp+20] ESI
    ;   [esp+24] EBP
    ;   [esp+28] ESP_orig
    ;   [esp+32] EBX
    ;   [esp+36] EDX
    ;   [esp+40] ECX
    ;   [esp+44] EAX
    ;   [esp+48] int_no
    ;   [esp+52] err_code
    ;   [esp+56] EIP
    ;   [esp+60] CS
    ;   [esp+64] EFLAGS
    mov eax, [esp + 48]
    cmp eax, 32
    jne .no_switch

    ; --- IRQ0: cambio de contexto ---
    ; Guardar ESP actual en current_task->esp
    push esp
    call task_save_current_esp
    add esp, 4

    ; Obtener el ESP de la nueva tarea y cambiar pila
    call schedule_and_switch
    mov esp, eax

.no_switch:
    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8                  ; descartar int_no + err_code
    iretd
