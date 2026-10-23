; =====================================================================
; ring3_test.asm — payload de prueba para ejecutar en ring 3
;
; REGLAS DE ORO:
;   - NO usar instrucciones privilegiadas: cli, sti, hlt, in, out,
;     mov crX, lgdt, lidt, ltr, iret (desde ring 3 no), etc.
;   - Cargar los segmentos de datos de usuario (0x23) al inicio.
;   - Terminar SIEMPRE con int 0x80 (syscall exit) o bucle infinito.
;
; Compilación:
;   fasm ring3_test.asm ring3_test.bin
; Luego el script que genera ring3_test_blob.c debe leer ring3_test.bin.
; =====================================================================

format binary
use32
org 0x08000000

_start:
    ; FIX: cargar segmentos de datos de usuario YA en ring 3.
    ; (En usermode.asm NO se deben cargar desde ring 0.)
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Marca visible: EAX = 0xCAFE0001 (para inspección con el monitor)
    mov eax, 0xCAFE0001
    mov ebx, 0xCAFE0002
    mov ecx, 0xCAFE0003
    mov edx, 0xCAFE0004

    ; Bucle "vivo": incrementa EAX para que se vea que ring 3 corre.
    ; (No usamos hlt porque es privilegiada.)
    mov esi, 0
.loop:
    inc esi
    inc eax
    ; Pequeña pausa (no privilegiada): nop x N
    nop
    nop
    nop
    nop

    ; FIX: syscall exit (int 0x80, EAX=0) si tu kernel lo soporta.
    ; Si todavía no tienes syscalls, comenta este bloque y deja el
    ; bucle infinito (jmp .loop) para inspeccionar el estado con QEMU.
    ;
    ; cmp esi, 0x100000
    ; jne .loop
    ; mov eax, 0          ; syscall 0 = exit
    ; int 0x80

    jmp .loop

; Relleno hasta 512 bytes para que el blob sea manejable
times 512 - ($ - $$) db 0x90
