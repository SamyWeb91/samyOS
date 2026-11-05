cd ~/samyos/samyOS

echo "=== 1. Compilando ASM ==="
fasm boot.asm boot.bin
fasm entry.asm entry.o
fasm gdt.asm gdt_asm.o
fasm tss.asm tss_asm.o
fasm isr.asm isr_asm.o
fasm paging.asm paging_asm.o
fasm switch.asm switch.o
fasm syscall.asm syscall_asm.o
fasm usermode.asm usermode_asm.o

echo "=== 2. Compilando C++ ==="
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c kernel.cpp -o kernel.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c isr.cpp -o isr_cpp.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c screen.cpp -o screen.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c idt.cpp -o idt.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c pic.cpp -o pic.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c pit.cpp -o pit.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c keyboard.cpp -o keyboard.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c mouse.cpp -o mouse.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c shell.cpp -o shell.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c heap.cpp -o heap.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c rtc.cpp -o rtc.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c paging.cpp -o paging.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c thread.cpp -o thread.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c process.cpp -o process.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c gdt.cpp -o gdt.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c tss.cpp -o tss.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c syscall.cpp -o syscall.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c user.cpp -o user.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c vbe.cpp -o vbe.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c gfx.cpp -o gfx.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c desktop.cpp -o desktop.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c win.cpp -o win.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c libc.cpp -o libc.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c ahci.cpp -o ahci.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c disk.cpp -o disk.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c ramdisk.cpp -o ramdisk.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c cpu_monitor.cpp -o cpu_monitor.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c composer.cpp -o composer.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c taskbar_start.cpp -o taskbar_start.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c pat.cpp -o pat.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c ring3_test.cpp -o ring3_test.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c fat32_win.cpp -o fat32_win.o
g++ -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -O2 -Wall -c font8x16.c -o font8x16.o

echo "=== 3. Enlazando kernel ==="
ld -m elf_i386 -T linker.ld -e _start --oformat binary \
   -o kernel.bin --start-group \
   entry.o gdt_asm.o tss_asm.o isr_asm.o paging_asm.o switch.o syscall_asm.o usermode_asm.o \
   kernel.o screen.o idt.o pic.o pit.o keyboard.o mouse.o shell.o heap.o rtc.o \
   paging.o thread.o process.o gdt.o tss.o isr_cpp.o syscall.o user.o \
   vbe.o gfx.o font8x16.o desktop.o win.o libc.o fat32_win.o \
   ahci.o disk.o ramdisk.o cpu_monitor.o composer.o taskbar_start.o \
   pat.o ring3_test.o user_prog_blob.o \
   --end-group /usr/lib/gcc/x86_64-linux-gnu/13/32/libgcc.a

echo "=== 4. Creando imagen ==="
dd if=/dev/zero of=samyOS.img bs=512 count=2880 status=none
dd if=boot.bin of=samyOS.img bs=512 count=1 conv=notrunc status=none
dd if=kernel.bin of=samyOS.img bs=512 seek=1 conv=notrunc status=none

echo "✅ ¡Listo! Imagen generada: samyOS.img"
