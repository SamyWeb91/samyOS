format ELF
use32

public enter_user_mode
public user_mode_entry

section '.text' executable

; void enter_user_mode(uint32_t entry, uint32_t user_stack);
enter_user_mode:
    mov eax, [esp + 4]      ; EIP
    mov edx, [esp + 8]      ; ESP de usuario

    cli

    ; Frame del iretd (solo esto)
    push 0x23               ; SS
    push edx                ; ESP
    push 0x202              ; EFLAGS (IF = 1)
    push 0x1B               ; CS
    push eax                ; EIP

    ; NO tocamos DS, ES, FS, GS aquí.
    iretd

; void user_mode_entry(uint32_t eip, uint32_t esp, uint32_t eflags, uint32_t cs, uint32_t ss);
; Esta funcion la usa el planificador
user_mode_entry:
    mov eax, [esp + 4]
    mov edx, [esp + 8]
    mov ecx, [esp + 12]
    mov ebx, [esp + 16]
    mov esi, [esp + 20]

    cli
    push esi
    push edx
    push ecx
    push ebx
    push eax
    iretd
