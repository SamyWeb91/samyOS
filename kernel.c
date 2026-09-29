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
#include "task.h"

static void task_a() {
    while (1) {
        kprintf("A");
        for (volatile int i = 0; i < 1000000; i++);
    }
}

static void task_b() {
    while (1) {
        kprintf("B");
        for (volatile int i = 0; i < 1000000; i++);
    }
}

extern "C" void kernel_main() {
    screen_clear();
    kprintf("MiniOS v1.5\n");
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

    task_init();
    kprintf("Sistema de tareas inicializado\n");

    task_create(task_a);
    task_create(task_b);

    kprintf("\nIniciando multitarea...\n\n");

    asm volatile("sti");

    while (1) {
        // El timer (IRQ0) se encarga del schedule
    }
}
