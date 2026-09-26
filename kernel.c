#include "screen.h"
#include "idt.h"
#include "pic.h"
#include "pit.h"
#include "keyboard.h"
#include "shell.h"
#include "heap.h"

extern "C" void kernel_main() {
    screen_clear();
    kprintf("MiniOS v0.8\n");
    kprintf("===========\n\n");

    idt_init();
    kprintf("IDT cargada\n");

    pic_remap();
    kprintf("PIC remapeado\n");

    pit_init(100);
    kprintf("PIT a 100 Hz\n");

    keyboard_init();
    kprintf("Teclado listo\n");

    heap_init();
    kprintf("Heap inicializado\n");

    asm volatile("sti");

    kprintf("\nEscribe 'help' para ver los comandos.\n\n");

    shell_run();

    while (1) {}
}
