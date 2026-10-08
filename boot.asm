; =====================================================================
; Bootloader: E820 + VBE + GDT + carga kernel
; =====================================================================
format binary
use16
org 0x7C00

start:
    jmp short boot_start
    nop

; --- BPB ---
bpbOEM          db "MINIOS  "
bpbBytesPerSec  dw 512
bpbSecPerClus   db 1
bpbRsvdSecCnt   dw 1
bpbNumFATs      db 2
bpbRootEntCnt   dw 224
bpbTotSec16     dw 2880
bpbMedia        db 0xF0
bpbFATSz16      dw 9
bpbSecPerTrk    dw 18
bpbNumHeads     dw 2
bpbHiddSec      dd 0
bpbTotSec32     dd 0
bsDrvNum        db 0
bsReserved1     db 0
bsBootSig       db 0x29
bsVolID         dd 0
bsVolLab        db "MINIOS     "
bsFilSysType    db "FAT12   "

; =====================================================================
boot_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    ; ---- E820: capturar mapa de memoria ----
    call e820_capture

    ; Cargar kernel: 128 sectores desde LBA 1 a 0x10000
    mov ax, 0x1000
    mov es, ax
    xor bx, bx
    mov ah, 0x02
    mov al, 128
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    ; ---- VBE ----
    call vbe_setup

    ; ---- Modo protegido ----
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:pm_start

; =====================================================================
; e820_capture
;   Guarda:
;     0x5000: numero de entradas (u32)
;     0x5004: signature 0xE820E820 (u32)
;     0x5008: entradas (24 bytes cada una)
; =====================================================================
e820_capture:
    pusha

    xor ax, ax
    mov es, ax
    mov di, 0x5008          ; destino de las entradas
    xor ebx, ebx
    xor bp, bp              ; contador
.loop:
    mov eax, 0xE820
    mov edx, 0x534D4150     ; "SMAP"
    mov ecx, 24
    int 0x15
    jc .done
    cmp eax, 0x534D4150
    jne .done
    test ebx, ebx
    jz .done
    add di, 24
    inc bp
    cmp bp, 32
    jae .done
    jmp .loop
.done:
    mov [0x5000], bp
    mov dword [0x5004], 0xE820E820
    popa
    ret

; =====================================================================
; vbe_setup
;
; Prueba varios modos. Guarda info en 0x6000.
;   0x6000: magic (0x31454256 "VBE1") o 0
;   0x6004: framebuffer addr (u32)
;   0x6008: pitch (u16)
;   0x600A: width (u16)
;   0x600C: height (u16)
;   0x600E: bpp (u8)
; =====================================================================
vbe_setup:
    pusha

    mov dword [0x6000], 0

    mov ax, 0x0000
    mov es, ax
    mov di, 0x6100
    mov cx, 256
    xor ax, ax
    rep stosw
    mov dword [0x6100], 0x32454256

    mov ax, 0x4F00
    mov di, 0x6100
    int 0x10
    cmp ax, 0x004F
    jne .done

    mov cx, 0x011B          ; 1024x768x32
    call vbe_try
    jnc .done

    mov cx, 0x0118          ; 1024x768x24
    call vbe_try
    jnc .done

    mov cx, 0x0112          ; 640x480x32
    call vbe_try
    jnc .done

.done:
    popa
    ret

; =====================================================================
; vbe_try: intenta cambiar al modo CX. CF=0 si OK.
; =====================================================================
vbe_try:
    pusha

    mov [0x6300], cx

    mov ax, 0x0000
    mov es, ax
    mov di, 0x6200
    mov cx, 128
    xor ax, ax
    rep stosw

    mov cx, [0x6300]
    mov ax, 0x4F01
    mov di, 0x6200
    int 0x10
    cmp ax, 0x004F
    jne .fail

    mov ax, [0x6200]
    and ax, 0x0081
    cmp ax, 0x0081
    jne .fail

    mov cx, [0x6300]
    mov bx, cx
    or bx, 0x4000
    mov ax, 0x4F02
    int 0x10
    cmp ax, 0x004F
    jne .fail

    mov ax, [0x6212]
    mov [0x600A], ax
    mov ax, [0x6214]
    mov [0x600C], ax
    mov al, [0x6219]
    mov [0x600E], al
    mov ax, [0x6210]
    mov [0x6008], ax
    mov eax, [0x6228]
    mov [0x6004], eax

    mov dword [0x6000], 0x31454256

    popa
    clc
    ret

.fail:
    popa
    stc
    ret

; =====================================================================
disk_error:
    jmp $

; =====================================================================
use32
pm_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000
    jmp 0x10000

; =====================================================================
gdt_start:
    dq 0
gdt_code:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b
    db 0x00
gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

; =====================================================================
boot_drive    db 0

times 510 - ($ - $$) db 0
dw 0xAA55
