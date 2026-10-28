format ELF
use32

public enter_user_mode
; Nota: Ya no necesitamos tss_set_kernel_stack aquí, 
; el kernel ya lo configuró antes de llamar a esta función.

section '.text' executable

; void enter_user_mode(uint32_t entry, uint32_t user_stack);
;   [esp+4] = entry
;   [esp+8] = user_stack
enter_user_mode:
    ; 1. Guardar argumentos en registros seguros
    mov eax, [esp + 4]      ; EAX = EIP del usuario
    mov edx, [esp + 8]      ; EDX = ESP del usuario

    ; 2. Deshabilitar interrupciones mientras cambiamos de contexto
    cli

    ; 3. Limpiar registros de segmento para no arrastrar basura
    xor ebx, ebx
    xor ecx, ecx
    xor esi, esi
    xor edi, edi
    xor ebp, ebp

    ; 4. Construir el frame para el iretd
    ; Orden EXACTO en la pila (de arriba a abajo): SS, ESP, EFLAGS, CS, EIP
    push 0x23               ; SS  = user data | RPL 3
    push edx                ; ESP = pila del usuario
    push 0x202              ; EFLAGS = IF=1
    push 0x1B               ; CS  = user code | RPL 3
    push eax                ; EIP = punto de entrada del usuario

    ; 5. Cargar los selectores de datos de usuario (¡CRÍTICO EN HARDWARE REAL!)
    ; Si no hacemos esto, el ELF intentará usar DS=0x10 (kernel) y fallará.
    mov cx, 0x23
    mov ds, cx
    mov es, cx
    mov fs, cx
    mov gs, cx

    ; 6. ¡Salto a ring 3!
    iretd
