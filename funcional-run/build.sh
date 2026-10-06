#!/bin/bash
# build.sh - Compila MiniOS completo

set -e  # Detener si algo falla

echo "=== Compilando bootloader ==="
fasm boot.asm boot.bin

echo "=== Compilando entry.asm ==="
fasm entry.asm entry.o

echo "=== Compilando isr.asm ==="
fasm isr.asm isr.o

echo "=== Compilando C++ ==="
CXXFLAGS="-x c++ -m32 -ffreestanding -fno-pic -fno-stack-protector \
    -fno-asynchronous-unwind-tables -fno-exceptions -fno-rtti -c"

for f in kernel screen idt isr pic pit keyboard heap shell; do
    echo "  - $f.cpp"
    g++ $CXXFLAGS $f.cpp -o $f.o
done

echo "=== Enlazando kernel ==="
ld -m elf_i386 -Ttext 0x10000 -e _start --oformat binary \
   -o kernel.bin entry.o isr.o kernel.o screen.o idt.o \
      isr_cpp.o pic.o pit.o keyboard.o shell.o heap.o

echo "=== Construyendo imagen ==="
dd if=/dev/zero of=minios.img bs=512 count=2880 2>/dev/null
dd if=boot.bin of=minios.img bs=512 count=1 conv=notrunc 2>/dev/null
dd if=kernel.bin of=minios.img bs=512 seek=1 conv=notrunc 2>/dev/null

echo "=== OK ==="
ls -l minios.img kernel.bin boot.bin
