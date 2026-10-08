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
#include "syscall.h"
#include "user.h"
#include "vbe.h"
#include "gfx.h"

// Declarado en screen.cpp
void screen_set_gfx_mode(void);

// =====================================================================
// Worker threads (demo)
// =====================================================================
void worker1(void) {
    volatile unsigned int x = 0;
    while (1) { x++; }
}

void worker2(void) {
    volatile unsigned int y = 0;
    while (1) { y++; }
}

// =====================================================================
// kernel_main
// =====================================================================
extern "C" void kernel_main(void) {
    // =============================================================
    // PASO 1: init básico (SIN paging todavía, SIN heap todavía)
    // =============================================================
    idt_init();
    gdt_init();
    tss_init();
    pic_remap();
    pit_init(100);
    keyboard_init();

    // =============================================================
    // PASO 2: paging ANTES que el heap
    //
    // paging_init mapea los primeros 128 MB (0x0 - 0x8000000).
    // El heap (0x400000 - 0x1400000) cae dentro de esa zona.
    // Sin este orden, heap_init escribiría en memoria no mapeada
    // y en hardware real daría page fault.
    // =============================================================
    paging_init();
    paging_enable_now();

    // =============================================================
    // PASO 3: heap AHORA, con paging ya activo
    // =============================================================
    heap_init();

    // =============================================================
    // PASO 4: VBE + GFX
    // =============================================================
    vbe_init();

    if (vbe_available()) {
        gfx_init();
        screen_set_gfx_mode();

        kprintf("MiniOS v6.10\n");
        kprintf("=================\n\n");
        kprintf("VBE: %ux%u %ubpp\n",
                vbe_width(), vbe_height(), vbe_bpp());
        kprintf("FB:  %x  pitch=%u bytes\n",
                vbe_fb_addr(), vbe_pitch());
        kprintf("\n");
    } else {
        kprintf("VBE no disponible, sigo en modo texto\n");
    }

    // =============================================================
    // PASO 5: resto del init
    // =============================================================
    kprintf("IDT cargada\n");
    kprintf("GDT recargada\n");
    kprintf("TSS cargado\n");
    kprintf("PIC remapeado\n");
    kprintf("PIT a 100 Hz\n");
    kprintf("Teclado listo\n");
    kprintf("Paging activado\n");
    kprintf("Heap inicializado\n");

    kprintf("MMIO: no mapeado (modo seguro, sin AHCI)\n");

    kprintf("\n");
    syscall_init();
    user_init();

    // =============================================================
    // AHCI + FS: DESHABILITADOS para modo seguro en hardware real
    // =============================================================
    kprintf("\n");
    kprintf("[MODO SEGURO] AHCI y FS deshabilitados (no se toca el disco)\n");

    // if (ahci_init()) {
    //     kprintf("AHCI listo\n");
    // } else {
    //     kprintf("AHCI no disponible\n");
    // }
    //
    // fs_init();

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
