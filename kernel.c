#include "screen.h"
#include "idt.h"
#include "gdt.h"
#include "tss.h"
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
// Workers
//
// worker1: sin yield → el timer debe expulsarlo.
// worker2: sin yield también, para probar expulsividad pura.
// =====================================================================
void worker1(void) {
    volatile unsigned int x = 0;
    while (1) {
        x++;
        // SIN yield → el timer debe expulsar
    }
}

void worker2(void) {
    volatile unsigned int y = 0;
    while (1) {
        y++;
        // SIN yield también
    }
}

// =====================================================================
// kernel_main
// =====================================================================
extern "C" void kernel_main(void) {
    screen_clear();
    kprintf("MiniOS - AHCI + FS + Threads (expulsivo)\n");
    kprintf("=========================================\n\n");

    idt_init();
    kprintf("IDT cargada\n");

    // GDT propia del kernel: 6 entradas (null, kcode, kdata,
    // ucode, udata, TSS). Reemplaza la GDT del bootloader que
    // solo tenía 3 entradas.
    gdt_init();
    kprintf("GDT recargada\n");

    // TSS: necesario para ring 3 (cambio de stack user→kernel).
    // Se registra como descriptor 0x28 en la GDT.
    tss_init();
    kprintf("TSS cargado\n");

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
