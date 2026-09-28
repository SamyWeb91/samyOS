; =====================================================================
; Bootloader con captura de mapa E820
; Compilar con: fasm boot.asm boot.bin
; =====================================================================
format binary
use16
org 0x7C00

; --- Punto de entrada ---
start:
    jmp short boot_start
    nop

; =====================================================================
; BPB (BIOS Parameter Block) - Disquete 1.44MB
; =====================================================================
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
; Código de arranque (modo real)
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

    ; =================================================================
    ; Capturar mapa E820 de la BIOS
    ; Guardamos:
    ;   0x5000: uint32 num_entries
    ;   0x5004: entradas de 24 bytes cada una
    ; =================================================================
    call e820_capture

    ; =================================================================
    ; Cargar kernel desde sector 2 a 0x10000 (64 sectores = 32 KB)
    ; =================================================================
    mov ax, 0x1000
    mov es, ax
    xor bx, bx

    mov ah, 0x02
    mov al, 64
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    mov si, msg_jump
    call print_string

    ; =================================================================
    ; Cambiar a modo protegido
    ; =================================================================
    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:pm_start

; =====================================================================
; e820_capture: recorre el mapa E820 y lo guarda en 0x5000
; =====================================================================
e820_capture:
    pusha

    ; ES:DI = 0x5004 (destino de las entradas)
    mov ax, 0x0000
    mov es, ax
    mov di, 0x5004

    xor ebx, ebx           ; EBX = 0 (continuacion)
    xor bp, bp             ; contador de entradas (usamos BP)

.e820_loop:
    mov eax, 0xE820
    mov edx, 0x534D4150    ; 'SMAP'
    mov ecx, 24            ; tamano de la entrada
    int 0x15

    ; CF=1 -> error o fin
    jc .e820_done

    ; EAX debe ser 'SMAP' tras la llamada
    cmp eax, 0x534D4150
    jne .e820_done

    ; Si EBX = 0, hemos terminado
    test ebx, ebx
    jz .e820_done

    ; Avanzar DI a la siguiente entrada
    add di, 24
    inc bp

    ; Limite de 64 entradas (suficiente)
    cmp bp, 64
    jae .e820_done

    jmp .e820_loop

.e820_done:
    ; Guardar el numero de entradas en 0x5000
    mov ax, 0x0000
    mov es, ax
    mov [0x5000], bp

    popa
    ret

; =====================================================================
; Código en modo protegido de 32 bits
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
; Rutinas de modo real
; =====================================================================
use16
print_string:
    mov ah, 0x0E
.next_char:
    lodsb
    cmp al, 0
    je .done
    int 0x10
    jmp .next_char
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
boot_drive  db 0
msg_loading db "Cargando kernel...", 13, 10, 0
msg_jump    db "Saltando a modo protegido...", 13, 10, 0
msg_error   db "Error al leer el disco.", 13, 10, 0

; =====================================================================
; Relleno y firma
; =====================================================================
times 510 - ($ - $$) db 0
dw 0xAA55
