# MiniOS

Sistema operativo mínimo para x86, con bootloader en FASM y kernel en C++.

## Características

- Bootloader FASM con BPB de disquete 1.44 MB
- Cambio a modo protegido de 32 bits
- Kernel en C++ freestanding (compilado con g++ -m32)
- IDT con 32 excepciones + 16 IRQs
- PIC remapeado
- PIT a 100 Hz
- Teclado PS/2 con buffer circular
- Heap con kmalloc/kfree
- Shell con comandos: help, clear, ticks, uptime, echo, cpuid, mem, heap, test, reboot, shutdown

## Compilación

Requisitos: `fasm`, `g++` con soporte `-m32`, `ld`, `dd`.

Ver `build.sh`.

## Ejecución

Grabar `minios.img` en un disquete virtual (QEMU) o en un USB con Rufus en modo DD.
