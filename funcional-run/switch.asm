format ELF
use32

public context_switch
public thread_entry_trampoline

extrn thread_trampoline_c

section '.text' executable

; void context_switch(uint32_t *old_esp, uint32_t new_esp);
context_switch:
    pushfd
    pushad

    ; Stack tras pushfd(4) + pushad(32) = 36:
    ;   [esp+0]  eax
    ;   [esp+4]  ecx
    ;   [esp+8]  edx
    ;   [esp+12] ebx
    ;   [esp+16] esp_dummy
    ;   [esp+20] ebp
    ;   [esp+24] esi
    ;   [esp+28] edi
    ;   [esp+32] eflags
    ;   [esp+36] ret addr
    ;   [esp+40] old_esp   ← arg1
    ;   [esp+44] new_esp   ← arg2

    mov eax, [esp + 40]     ; old_esp
    mov edx, [esp + 44]     ; new_esp

    mov [eax], esp          ; guardar ESP actual
    mov esp, edx            ; cargar nuevo ESP

    popad
    popfd
    ret

; Primera entrada de un hilo nuevo.
; thread_entry_trampoline se llama desde `ret` en context_switch.
thread_entry_trampoline:
    call thread_trampoline_c
    ; si vuelve, no debería pasar
    cli
    jmp $
