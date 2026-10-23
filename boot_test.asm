; =====================================================================
; boot_test.asm - Bootloader mínimo para test AHCI
; Carga kernel-test-ahci.bin desde LBA 1 a 0x10000 y salta en PM.
; NO toca boot.asm ni entry.asm del proyecto principal.
; =====================================================================
format binary
use16
org 0x7C00

start:
    jmp short boot_start
    nop

; --- BPB (para que algunas BIOS no se quejen) ---
bpbOEM          db "TESTAHCI"
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
bsVolLab        db "TESTAHCI   "
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

    ; Mensaje
    mov si, msg_load
    call print

    ; Cargar 128 sectores desde LBA 1 a 0x10000 (ES:BX = 0x1000:0x0000)
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

    mov si, msg_ok
    call print

    ; ---- Modo protegido ----
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:pm_start

; =====================================================================
; print: imprime string en DS:SI (modo real, BIOS teletype)
; =====================================================================
print:
    pusha
.next:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    mov bx, 0x0007
    int 0x10
    jmp .next
.done:
    popa
    ret

; =====================================================================
disk_error:
    mov si, msg_err
    call print
    cli
    hlt
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
boot_drive  db 0
msg_load    db "Cargando test AHCI...", 13, 10, 0
msg_ok      db "OK. Entrando a modo protegido.", 13, 10, 0
msg_err     db "ERROR leyendo disco.", 13, 10, 0

times 510 - ($ - $$) db 0
dw 0xAA55
