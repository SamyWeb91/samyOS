; =====================================================================
; task_switch.asm - Context switch entre dos tareas
; Compilar con: fasm task_switch.asm task_switch.o
; =====================================================================
format ELF
use32

public task_switch

section '.text' executable

; =====================================================================
; task_switch(Task *old_task, Task *new_task)
;   [esp+4]  = old_task
;   [esp+8]  = new_task
; Estructura Task:
;   offset 0:  pid
;   offset 4:  state
;   offset 8:  esp
;   offset 12: stack_base
;   offset 16: eip
;   offset 20: next
; =====================================================================
task_switch:
    ; Guardar registros de la tarea actual en su pila
    pusha
    pushf

    ; Guardar ESP actual en old_task->esp (offset 8)
    mov eax, [esp + 36]     ; EAX = old_task (pusha=32 + pushf=4)
    mov [eax + 8], esp

    ; Cargar ESP desde new_task->esp
    mov eax, [esp + 40]     ; EAX = new_task
    mov esp, [eax + 8]

    ; Restaurar registros de la tarea nueva
    popf
    popa

    ret
