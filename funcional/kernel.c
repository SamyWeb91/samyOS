#include "screen.h"
#include "idt.h"
#include "pic.h"
#include "pit.h"
#include "keyboard.h"
#include "shell.h"
#include "heap.h"
#include "rtc.h"
#include "paging.h"
#include "gdt.h"
#include "tss.h"

extern "C" void kernel_main() {
    screen_clear();
    kprintf("MiniOS v1.2\n");
    kprintf("===========\n\n");

    gdt_init();
    kprintf("GDT del kernel cargada\n");

    tss_init();
    kprintf("TSS inicializado\n");

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

    paging_init();
    paging_enable_now();
    kprintf("Paginacion activada\n");

    asm volatile("sti");

    kprintf("\nEscribe 'help' para ver los comandos.\n\n");

    shell_run();

    while (1) {}
}
