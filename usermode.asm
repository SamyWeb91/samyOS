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

    ; Frame del iretd
    push 0x23               ; SS (DPL=3)
    push edx                ; ESP
    push 0x202              ; EFLAGS (IF=1, bit1 reservado=1)
    push 0x1B               ; CS (DPL=3)
    push eax                ; EIP

    ; ⚠️ CRÍTICO: recargar DS/ES/FS/GS con el selector de usuario.
    ; Es legal hacerlo desde Ring 0 porque DPL(0x23)=3 >= max(CPL=0, RPL=3).
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    iretd

; void user_mode_entry(uint32_t eip, uint32_t esp, uint32_t eflags, uint32_t cs, uint32_t ss);
user_mode_entry:
    mov eax, [esp + 4]      ; eip
    mov edx, [esp + 8]      ; esp
    mov ecx, [esp + 12]     ; eflags
    mov ebx, [esp + 16]     ; cs
    mov esi, [esp + 20]     ; ss

    cli
    push esi                ; SS
    push edx                ; ESP
    push ecx                ; EFLAGS
    push ebx                ; CS
    push eax                ; EIP

    mov ax, si              ; si = ss = 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    iretd
