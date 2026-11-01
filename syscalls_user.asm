format ELF
use32

public asm_sys_draw_rect
public asm_sys_exit

section '.text' executable

; void asm_sys_draw_rect(int x, int y, int w, int h, uint32_t color);
asm_sys_draw_rect:
    push ebx
    push esi
    push edi

    ; [esp] = edi, [esp+4] = esi, [esp+8] = ebx (por los push)
    ; [esp+12] = ret addr
    ; [esp+16] = x
    ; [esp+20] = y
    ; [esp+24] = w
    ; [esp+28] = h
    ; [esp+32] = color

    mov eax, 11         ; SYS_DRAW_RECT
    mov ebx, [esp + 16] ; x
    mov ecx, [esp + 20] ; y
    mov edx, [esp + 24] ; w
    mov esi, [esp + 28] ; h
    mov edi, [esp + 32] ; color

    int 0x80

    pop edi
    pop esi
    pop ebx
    ret

; void asm_sys_exit(int code);
asm_sys_exit:
    push ebx
    mov eax, 0          ; SYS_EXIT
    mov ebx, [esp + 8]  ; code
    int 0x80
    pop ebx
    ret
