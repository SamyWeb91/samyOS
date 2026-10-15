#!/bin/bash
# build.sh - Compila MiniOS completo (v9.2 + fat32_win + user_prog blob)

set -e

CXXFLAGS="-x c++ -m32 -ffreestanding -fno-pic -fno-stack-protector \
          -fno-asynchronous-unwind-tables -fno-exceptions -fno-rtti -c"

LIBGCC="/usr/lib/gcc/x86_64-linux-gnu/13/32/libgcc.a"

echo "=== Compilando bootloader ==="
fasm boot.asm boot.bin
echo "boot.bin: $?"

echo "=== Compilando entry.asm ==="
fasm entry.asm entry.o
echo "entry.o: $?"

echo "=== Compilando isr.asm ==="
fasm isr.asm isr.o
echo "isr.o: $?"

echo "=== Compilando switch.asm ==="
fasm switch.asm switch.o
echo "switch.o: $?"

echo "=== Compilando gdt.asm ==="
fasm gdt.asm gdt_asm.o
echo "gdt_asm.o: $?"

echo "=== Compilando tss.asm ==="
fasm tss.asm tss_asm.o
echo "tss_asm.o: $?"

echo "=== Compilando paging.asm ==="
fasm paging.asm paging_asm.o
echo "paging_asm.o: $?"

echo "=== Compilando syscall.asm ==="
fasm syscall.asm syscall_asm.o
echo "syscall_asm.o: $?"

echo "=== Compilando usermode.asm ==="
fasm usermode.asm usermode_asm.o
echo "usermode_asm.o: $?"

echo "=== Compilando C++ ==="

SRCS="
kernel.c
isr.cpp
screen.cpp
idt.cpp
pic.cpp
pit.cpp
keyboard.cpp
mouse.cpp
shell.cpp
heap.cpp
rtc.cpp
ramdisk.cpp
mfs.cpp
cpu_monitor.cpp
pat.cpp
composer.cpp
taskbar_start.cpp
folder.cpp
ahci.cpp
disk.cpp
paging.cpp
thread.cpp
gdt.cpp
tss.cpp
syscall.cpp
user.cpp
vbe.cpp
gfx.cpp
desktop.cpp
win.cpp
libc.cpp
font8x16.c
fat32_win.cpp
"

for f in $SRCS; do
    if [ ! -f "$f" ]; then
        echo "  !! FALTA $f, salto"
        continue
    fi
    obj="${f%.*}.o"
    echo "  - $f -> $obj"
    g++ $CXXFLAGS "$f" -o "$obj"
    echo "  $obj: $?"
done

echo "=== Generando blob de user_prog (ring 3) ==="

# 1. Compilar user_prog.cpp a .o (ring 3, freestanding)
g++ -x c++ -m32 -ffreestanding -fno-pic -fno-stack-protector \
    -fno-asynchronous-unwind-tables -fno-exceptions -fno-rtti \
    -c user_prog.cpp -o user_prog.o
echo "user_prog.o: $?"

# 2. Enlazar a binario plano en 0x400000 (direccion de carga en ring 3)
ld -m elf_i386 -Ttext 0x400000 -e user_prog_start --oformat binary \
   -o user_prog.bin user_prog.o
echo "user_prog.bin: $?"

# 3. Convertir a .o con simbolos renombrados
objcopy -I binary -O elf32-i386 -B i386 \
    --redefine-sym _binary_user_prog_bin_start=user_prog_start \
    --redefine-sym _binary_user_prog_bin_end=user_prog_end \
    --redefine-sym _binary_user_prog_bin_size=user_prog_size \
    user_prog.bin user_prog_blob.o
echo "user_prog_blob.o: $?"

ls -l user_prog.bin user_prog_blob.o

echo "=== Enlazando kernel ==="
ld -m elf_i386 -T linker.ld -e _start --oformat binary \
   -o kernel.bin \
   entry.o isr.o kernel.o screen.o idt.o \
   pic.o pit.o keyboard.o mouse.o shell.o heap.o rtc.o \
   ramdisk.o mfs.o cpu_monitor.o pat.o composer.o taskbar_start.o folder.o \
   ahci.o disk.o \
   paging.o paging_asm.o thread.o switch.o \
   gdt.o tss.o gdt_asm.o tss_asm.o \
   syscall.o syscall_asm.o user.o usermode_asm.o user_prog_blob.o \
   vbe.o gfx.o font8x16.o desktop.o win.o libc.o \
   fat32_win.o \
   "$LIBGCC"
echo "ld: $?"

echo "=== Construyendo imagen ==="
dd if=/dev/zero of=minios-safe.img bs=512 count=2880 status=none
dd if=boot.bin of=minios-safe.img bs=512 count=1 conv=notrunc status=none
dd if=kernel.bin of=minios-safe.img bs=512 seek=1 conv=notrunc status=none

ls -lh minios-safe.img kernel.bin boot.bin
echo "=== OK ==="
