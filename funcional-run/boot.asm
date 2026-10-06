; =====================================================================
; Bootloader con E820, sin VBE (modo texto para diagnostico)
; Compilar con: fasm boot.asm boot.bin
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
; Arranque
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

    mov si, msg_loading
    call print_string

    call e820_capture

    ; --- Cargar kernel: 64 sectores desde LBA 1 a 0x10000 ---
    mov ax, 0x1000
    mov es, ax
    xor bx, bx

    mov ah, 0x02
    mov al, 96
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    mov si, msg_jump
    call print_string

    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:pm_start

; =====================================================================
; e820_capture
; =====================================================================
e820_capture:
    pusha
    mov ax, 0x0000
    mov es, ax
    mov di, 0x5004
    xor ebx, ebx
    xor bp, bp
.e820_loop:
    mov eax, 0xE820
    mov edx, 0x534D4150
    mov ecx, 24
    int 0x15
    jc .done
    cmp eax, 0x534D4150
    jne .done
    test ebx, ebx
    jz .done
    add di, 24
    inc bp
    cmp bp, 64
    jae .done
    jmp .e820_loop
.done:
    mov ax, 0x0000
    mov es, ax
    mov [0x5000], bp
    popa
    ret

; =====================================================================
; Modo protegido
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
; Rutinas modo real
; =====================================================================
use16
print_string:
    mov ah, 0x0E
.next:
    lodsb
    cmp al, 0
    je .done
    int 0x10
    jmp .next
.done:
    ret

disk_error:
    mov si, msg_error
    call print_string
    jmp $

; =====================================================================
; GDT
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
; Datos
; =====================================================================
boot_drive    db 0
msg_loading   db "Cargando kernel...", 13, 10, 0
msg_jump      db "Saltando a modo protegido...", 13, 10, 0
msg_error     db "Error al leer el disco.", 13, 10, 0

times 510 - ($ - $$) db 0
dw 0xAA55
