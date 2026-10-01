#include "screen.h"
#include "idt.h"
#include "pic.h"
#include "pit.h"
#include "keyboard.h"
#include "heap.h"
#include "paging.h"
#include "gdt.h"
#include "tss.h"
#include "task.h"

static void task_a(void) {
    for (;;) {
        kprintf("A");
        for (volatile int i = 0; i < 60000; i++);
    }
}

static void task_b(void) {
    for (;;) {
        kprintf("B");
        for (volatile int i = 0; i < 60000; i++);
    }
}

extern "C" void kernel_main(void) {
    screen_clear();
    kprintf("MiniOS - multitarea simple\n\n");

    gdt_init();
    tss_init();
    idt_init();
    pic_remap();
    pit_init(100);
    keyboard_init();
    heap_init();
    paging_init();
    paging_enable_now();

    task_init();
    task_create(task_a);
    task_create(task_b);

    kprintf("Iniciando...\n\n");
    asm volatile("sti");

    for (;;)
        asm volatile("hlt");
}
