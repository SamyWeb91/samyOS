; =====================================================================
; disk.asm - Driver de disco que vuelve a modo real para INT 0x13
; Compilar con: fasm disk.asm disk.o
; =====================================================================
format ELF
use32

public disk_driver_stub_start
public disk_driver_stub_end
public disk_read_sector_call

section '.text' executable

; =====================================================================
; disk_read_sector_call
; Llamada desde C++ para ejecutar el stub.
; Entrada:
;   EAX = LBA
;   EBX = direccion fisica del buffer (512 bytes)
; Salida:
;   EAX = 0 si OK, != 0 si error (codigo de error de la BIOS en AH)
; =====================================================================
disk_read_sector_call:
    ; Guardar argumentos en variables del stub
    ; (no son variables de C++, son direcciones fisicas fijas)
    mov [0x8F00], eax           ; LBA
    mov [0x8F04], ebx           ; buffer

    ; Llamar al stub copiado en 0x8000
    ; El stub esta en 0x8000, en modo protegido con CS=0x08
    ; Usamos un call directo: el stub ya esta en 0x8000
    call 0x8000

    ; El resultado se guarda en 0x8F08
    mov eax, [0x8F08]
    ret

; =====================================================================
; disk_driver_stub_start
; Este es el stub que se copia a 0x8000 y se ejecuta.
; NO se ejecuta desde su posicion original (en el kernel, >1 MB).
; Se copia a 0x8000 y se llama alli.
; =====================================================================
disk_driver_stub_start:

    ; ---- GUARDAR ESTADO ----
    pusha
    push ds
    push es
    push fs
    push gs

    ; Guardar ESP actual
    mov [0x8E00], esp

    ; Guardar CR0
    mov eax, cr0
    mov [0x8E04], eax

    ; Guardar IDT
    sidt [0x8E08]               ; guarda limite (2 bytes) + base (4 bytes)

    ; Guardar GDT actual
    sgdt [0x8E10]               ; idem

    ; ---- CARGAR GDT DE MODO REAL ----
    lgdt [0x8E20]               ; GDT de modo real (base 0, limite 0xFFFF)

    ; ---- DESACTIVAR PROTECCION ----
    mov eax, cr0
    and eax, 0xFFFFFFFE         ; quitar bit PE
    mov cr0, eax

    ; ---- SALTAR A MODO REAL ----
    ; El segmento CS tiene que ser 0x0000
    ; La direccion de .rm_code debe ser < 64 KB
    jmp 0x0000:.rm_code

use16
.rm_code:
    ; Configurar segmentos de modo real
    mov ax, 0x0000
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Pila en modo real
    mov sp, 0x7C00

    ; Cargar argumentos desde las direcciones fisicas
    mov eax, [0x8F00]           ; LBA
    mov edx, [0x8F04]           ; buffer fisico (32 bits)

    ; ---- CONVERTIR LBA A CHS ----
    ; Geometria: 18 sectores/pista, 2 cabezas
    push edx                    ; guardar buffer
    xor edx, edx
    mov ecx, 18
    div ecx                     ; EAX = LBA/18, EDX = LBA%18
    inc dl
    mov cl, dl                  ; CL = sector (1-18)
    xor edx, edx
    mov ecx, 2
    div ecx                     ; EAX = cilindro, EDX = cabeza
    mov ch, al                  ; CH = cilindro
    mov dh, dl                  ; DH = cabeza

    pop edx                     ; recuperar buffer

    ; Unidad de disco
    mov dl, [0x8F0C]            ; numero de unidad guardado

    ; ES:BX = buffer
    ; El buffer fisico es de 32 bits, pero ES:BX es de 20 bits.
    ; Como el buffer es < 1 MB, podemos pasar los 16 bits altos a ES y los bajos a BX.
    mov eax, edx
    shr eax, 4                  ; ES = buffer >> 4
    mov es, ax
    mov bx, dx
    and bx, 0x000F              ; BX = buffer & 0xF

    ; ---- EJECUTAR INT 0x13 ----
    mov ah, 0x02                ; leer sectores
    mov al, 1                   ; 1 sector
    int 0x13

    ; Guardar resultado
    mov [0x8F08], ah            ; codigo de error
    mov byte [0x8F0D], 0
    jnc .no_err
    mov byte [0x8F0D], 1
.no_err:

    ; ---- VOLVER A MODO PROTEGIDO ----
    cli

    ; Cargar GDT de modo protegido
    lgdt [0x8E10]

    ; Activar proteccion
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Saltar a modo protegido
    ; CS = 0x08
    ; La direccion .pm_code debe ser < 4 GB pero en modo real
    ; solo se pueden usar 16 bits, asi que el codigo .pm_code tiene
    ; que estar en 0x8000+ (fisicamente)
    jmp 0x08:.pm_code

use32
.pm_code:
    ; Configurar segmentos de modo protegido
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Restaurar pila
    mov esp, [0x8E00]

    ; Restaurar IDT
    lidt [0x8E08]

    ; ---- RESTAURAR REGISTROS ----
    pop gs
    pop fs
    pop es
    pop ds
    popa

    ; Volver al llamante
    ret

disk_driver_stub_end:

; =====================================================================
; Datos
; =====================================================================
section '.data' data readable writeable

; GDT de modo real (se copia a 0x8E20)
real_gdt:
    dq 0                        ; nulo
    dq 0                        ; codigo (no usado, flat)
    dq 0                        ; datos (no usado, flat)
real_gdt_desc:
    dw 0x17                     ; limite = 24 - 1 = 23
    dd 0x8E20                   ; base = 0x8E20 (donde se copiara)
