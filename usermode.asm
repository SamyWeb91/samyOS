format ELF
use32

public enter_user_mode
extrn tss_set_kernel_stack
extrn user_get_kernel_stack

section '.text' executable

; void enter_user_mode(uint32_t entry, uint32_t user_stack);
;   [esp+4] = entry
;   [esp+8] = user_stack
enter_user_mode:
    ; Cargar argumentos en registros que NO se van a machacar
    ; Los guardamos en la pila directamente para no perderlos.
    mov eax, [esp + 4]      ; EIP del usuario
    mov edx, [esp + 8]      ; ESP del usuario

    ; Guardar EIP y ESP en la pila (aún en el stack del kernel)
    push eax                ; [esp]   = EIP del usuario
    push edx                ; [esp+4] = ESP del usuario

    ; Stack del kernel para el TSS
    call user_get_kernel_stack
    mov ecx, eax            ; ECX = stack del kernel para syscalls
    push ecx
    call tss_set_kernel_stack
    add esp, 4

    ; Recuperar EIP y ESP (en orden inverso)
    pop edx                 ; EDX = ESP del usuario
    pop eax                 ; EAX = EIP del usuario

    ; Construir el stack para iret (empujar en orden inverso)
    ;   [SS]  [ESP]  [EFLAGS]  [CS]  [EIP]
    ; El último push es el que iret pop primero.
    push 0x23               ; SS = user data | RPL 3
    push edx                ; ESP del usuario
    push 0x202              ; EFLAGS (IF=1)
    push 0x1B               ; CS = user code | RPL 3
    push eax                ; EIP del usuario
    iretd
