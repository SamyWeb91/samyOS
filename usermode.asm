format ELF
use32

public enter_user_mode
extrn tss_set_kernel_stack
extrn user_get_kernel_stack

section '.text' executable

enter_user_mode:
    mov eax, [esp + 4]
    mov edx, [esp + 8]

    push eax
    push edx

    call user_get_kernel_stack
    push eax
    call tss_set_kernel_stack
    add esp, 4

    pop edx
    pop eax

    cli

    mov cx, 0x23
    mov ds, cx
    mov es, cx
    mov fs, cx
    mov gs, cx

    push 0x23
    push edx
    push 0x202
    push 0x1B
    push eax

    mov eax, cr3
    mov cr3, eax

    iretd
