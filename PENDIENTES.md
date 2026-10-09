# MiniOS - Notas y pendientes

## v7.2-arranque-directo-desktop (2026-10-09)

### Funciona
- Arranque directo al escritorio grafico (1-2 seg en hardware real).
- Raton PS/2 funcionando en hardware real.
- Teclado PS/2 funcionando en hardware real.
- Shell integrada en ventana terminal.
- Comandos: help, clear, ps, heap, mem, ls, date, etc.
- Ventana arrastrable por la barra de titulo.
- Boton cerrar con hover.
- Doble buffer sin parpadeo.

### Bugs conocidos
- El prompt `> ` no se ve hasta que escribes algo (por el flip cada 64 chars).
- El comando `desktop` esta deshabilitado (ya estas dentro).

### Proximos pasos
1. Ver prompt al arrancar.
2. Varias ventanas con foco.
3. Iconos en el escritorio.
4. Barra de tareas con botones.
5. Reactivar AHCI + FS.
6. Driver USB HID.

### Hardware probado
- Laptop con PS/2 emulado / touchpad externo.
- QEMU con -vga std.

### Notas tecnicas
- Arranque: boot.asm carga kernel a 0x10000 con VBE activo.
- Paging mapea primeros 128 MB.
- Heap en 0x400000 - 0x1400000 (16 MB).
- Backbuffer en heap (~3 MB para 1024x768x32).
- Flip por caracter cada 64 en arranque, sin flip en terminal (el desktop hace flip por regiones sucias).
