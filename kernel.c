#include "screen.h"
#include "idt.h"
#include "pic.h"
#include "pit.h"
#include "keyboard.h"
#include "shell.h"
#include "heap.h"
#include "rtc.h"
#include "paging.h"
#include "fs.h"
#include "ahci.h"
#include "thread.h"

// =====================================================================
// Workers: trabajo ligero + yield
// =====================================================================
void worker1(void) {
    while (1) {
        for (volatile int i = 0; i < 1000; i++) {}
        thread_yield();
    }
}

void worker2(void) {
    while (1) {
        for (volatile int i = 0; i < 1000; i++) {}
        thread_yield();
    }
}

// =====================================================================
// kernel_main
// =====================================================================
extern "C" void kernel_main(void) {
    screen_clear();
    kprintf("MiniOS - AHCI + FS + Threads\n");
    kprintf("============================\n\n");

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

    paging_map_range(0xFE000000, 0xFE000000, 32768, PTE_PRESENT | PTE_RW);
    kprintf("MMIO mapeado (0xFE000000 - 0xFFFFFFFF)\n");

    kprintf("\n");
    if (ahci_init()) {
        kprintf("AHCI listo\n");
    } else {
        kprintf("AHCI no disponible\n");
    }

    fs_init();

    kprintf("\n");
    thread_init();

    thread_create("worker1", worker1);
    thread_create("worker2", worker2);

    kprintf("Threads listos\n");

    asm volatile("sti");

    kprintf("\nEscribe 'help' para ver los comandos.\n\n");

    shell_run();

    while (1) {}
}
