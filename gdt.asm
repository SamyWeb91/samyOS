format ELF
use32

public gdt_flush

section '.text' executable

; void gdt_flush(uint32_t gdtr_addr);
gdt_flush:
    mov eax, [esp + 4]
    lgdt [eax]

    ; Recargar los segmentos de datos con el selector 0x10
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Recargar CS usando un push/retf (más seguro en hardware real)
    push 0x08
    push .reload_cs
    retf
.reload_cs:
    ret
