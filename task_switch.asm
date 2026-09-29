; =====================================================================
; task_switch.asm - Cambio de tarea desde el timer
; Compilar con: fasm task_switch.asm task_switch.o
; =====================================================================
format ELF
use32

public task_switch_to

section '.text' executable

; =====================================================================
; task_switch_to(Task *new_task)
;   [esp+4] = new_task
; Devuelve en EAX el valor de new_task->esp
; Estructura Task:
;   offset 0:  pid
;   offset 4:  state
;   offset 8:  esp
;   offset 12: stack_base
;   offset 16: eip
;   offset 20: next
; =====================================================================
task_switch_to:
    mov eax, [esp + 4]      ; EAX = new_task
    mov eax, [eax + 8]      ; EAX = new_task->esp
    ret
