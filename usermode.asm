format ELF
use32

public enter_user_mode
; FIX: ya no necesitamos tss_set_kernel_stack aquí. El caller (C++)
; es responsable de configurar TSS.esp0 con su propia pila dedicada.
; extrn tss_set_kernel_stack

section '.text' executable

; =====================================================================
; void enter_user_mode(uint32_t entry, uint32_t user_stack);
;   [esp+4] = entry   (EIP de ring 3)
;   [esp+8] = user_stack (ESP de ring 3)
;
; REQUISITOS DEL CALLER:
;   - TSS.esp0 ya debe apuntar a una pila de kernel válida y mapeada.
;   - La página de 'entry' debe estar mapeada con PTE_USER|PTE_PRESENT.
;   - La página de 'user_stack' debe estar mapeada con PTE_USER|PTE_PRESENT.
;   - El GDT debe tener: CS usuario = 0x1B, DS usuario = 0x23 (DPL=3).
; =====================================================================
enter_user_mode:
    ; 1. Guardar argumentos en registros seguros
    mov eax, [esp + 4]      ; EAX = EIP del usuario
    mov edx, [esp + 8]      ; EDX = ESP del usuario

    ; FIX: NO llamar a tss_set_kernel_stack aquí. El caller C++ ya
    ; configuró TSS.esp0. Forzar 0x90000 anulaba la pila dedicada.

    ; 2. Deshabilitar interrupciones mientras construimos el frame.
    ;    El iretd restaurará EFLAGS=0x202 (IF=1) al entrar a ring 3.
    cli

    ; FIX: NO cargar DS/ES/FS/GS con 0x23 desde ring 0.
    ; Es un antipatrón: si el iretd falla, quedan selectores de usuario
    ; con CS de kernel, y el #GP reporta basura. Los segmentos de datos
    ; se cargan YA en ring 3, al inicio del payload (ring3_test.asm).

    ; 3. Limpiar registros de propósito general que puedan filtrar
    ;    punteros del kernel a ring 3. No es obligatorio, pero es
    ;    buena higiene de seguridad.
    xor ebx, ebx
    xor ecx, ecx
    xor esi, esi
    xor edi, edi
    xor ebp, ebp

    ; 4. Construir el frame para el iretd.
    ;    Orden EXACTO en la pila (de arriba a abajo):
    ;      [SS]      ← se popea último
    ;      [ESP]
    ;      [EFLAGS]
    ;      [CS]
    ;      [EIP]     ← se popea primero
    push 0x23               ; SS  = user data | RPL 3  (índice 4)
    push edx                ; ESP = pila del usuario
    push 0x202              ; EFLAGS = IF=1, bit1 reservado=1
    push 0x1B               ; CS  = user code | RPL 3  (índice 3)
    push eax                ; EIP = punto de entrada del usuario

    ; FIX: el flush de TLB (mov cr3,cr3) es innecesario aquí.
    ; iretd no cambia CR3. Se deja comentado por claridad.
    ; mov ecx, cr3
    ; mov cr3, ecx

    ; 5. ¡Salto a ring 3!
    ;    La CPU:
    ;      - Verifica que CS/SS tengan RPL=3 y DPL=3.
    ;      - Verifica que EIP esté dentro del límite de CS.
    ;      - Carga SS:ESP desde la pila.
    ;      - Carga EFLAGS.
    ;      - Carga CS:EIP.
    ;      - Cambia CPL a 3.
    iretd

    ; Nunca llegamos aquí.
    ; Si el iretd falla (p.ej. EIP no mapeado), la CPU genera #GP
    ; y el handler de #GP se encarga. Si el handler está roto → triple fault.
